#!/usr/bin/env python3
"""Run Windows builds without GitHub Actions; never claim host acceptance."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import zipfile
import xml.etree.ElementTree as ET
from datetime import datetime, timezone


def run(command, cwd, log=None):
    result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    if log:
        Path(log).write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {command[0]}; log={log}")
    return result.stdout.strip()


def preflight(repo, expected):
    head = run(["git", "rev-parse", "HEAD"], repo)
    if head != expected:
        raise RuntimeError(f"Expected commit {expected}, found {head}")
    if run(["git", "status", "--porcelain", "--untracked-files=all"], repo):
        raise RuntimeError("Use a clean isolated worktree; local changes are not validated")
    run(["git", "diff", "--check", "HEAD"], repo)
    return head


def find_msbuild():
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not vswhere.is_file():
        raise RuntimeError("Visual Studio Installer/vswhere.exe is missing")
    paths = run([str(vswhere), "-latest", "-products", "*", "-version", "[17.0,18.0)",
                 "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                 "-find", "MSBuild/**/Bin/MSBuild.exe"], Path.cwd()).splitlines()
    if not paths:
        raise RuntimeError("VS 2022 C++ tools were not found; install v143 and Windows SDK")
    return paths[0]


def policy_project(source, destination):
    # ElementTree escapes paths; no command shell or hand-built XML interpolation.
    root = ET.Element("Project", DefaultTargets="Build",
                      xmlns="http://schemas.microsoft.com/developer/msbuild/2003")
    group = ET.SubElement(root, "ItemGroup", Label="ProjectConfigurations")
    config = ET.SubElement(group, "ProjectConfiguration", Include="Release|x64")
    ET.SubElement(config, "Configuration").text = "Release"
    ET.SubElement(config, "Platform").text = "x64"
    globals_ = ET.SubElement(root, "PropertyGroup", Label="Globals")
    ET.SubElement(globals_, "WindowsTargetPlatformVersion").text = "10.0"
    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props")
    props = ET.SubElement(root, "PropertyGroup", Label="Configuration")
    for key, value in {"ConfigurationType": "Application", "PlatformToolset": "v143",
                       "UseDebugLibraries": "false"}.items():
        ET.SubElement(props, key).text = value
    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.props")
    defs = ET.SubElement(root, "ItemDefinitionGroup")
    compile_ = ET.SubElement(defs, "ClCompile")
    for key, value in {"LanguageStandard": "stdcpp20", "WarningLevel": "Level4",
                       "TreatWarningAsError": "true", "ExceptionHandling": "Sync"}.items():
        ET.SubElement(compile_, key).text = value
    items = ET.SubElement(root, "ItemGroup")
    ET.SubElement(items, "ClCompile", Include=str(source))
    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.targets")
    ET.ElementTree(root).write(destination, encoding="utf-8", xml_declaration=True)


def verify_package(package, expected_dll_hash):
    digest = hashlib.sha256(package.read_bytes()).hexdigest()
    checksum = Path(str(package) + ".sha256").read_text(encoding="ascii").strip()
    if checksum.lower() != f"{digest}  {package.name}".lower():
        raise RuntimeError("ZIP checksum sidecar mismatch")
    try:
        with zipfile.ZipFile(package) as archive:
            names = archive.namelist()
            if names.count("NppAIAssistant.dll") != 1 or any(n.lower().endswith(".pdb") for n in names):
                raise RuntimeError("Invalid plugin ZIP layout or bundled symbols")
            if hashlib.sha256(archive.read("NppAIAssistant.dll")).hexdigest() != expected_dll_hash.lower():
                raise RuntimeError("Packaged DLL differs from validated build")
            if archive.testzip() is not None:
                raise RuntimeError("ZIP integrity check failed")
    except zipfile.BadZipFile as error:
        raise RuntimeError("Invalid ZIP archive") from error
    return digest


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expected-commit", required=True,
                        help="Full reviewed PR head SHA (mandatory)")
    parser.add_argument("--all-platforms", action="store_true",
                        help="Build x64, Win32 and ARM64; default builds x64 Debug/Release")
    args = parser.parse_args(argv)
    repo = Path(__file__).resolve().parents[1]
    output = Path(tempfile.mkdtemp(prefix="npp-manual-validation-"))
    receipt = {"status": "FAILED", "host_acceptance": "NOT_RUN",
               "expected_commit": args.expected_commit, "platform": platform.platform(),
               "started_utc": datetime.now(timezone.utc).isoformat(),
               "builds": [], "packages": [], "policy_test": "NOT_RUN"}
    code = 1
    try:
        receipt["commit"] = preflight(repo, args.expected_commit)
        if os.name != "nt":
            raise RuntimeError("Windows is required for DLL build and executable tests")
        msbuild = find_msbuild()
        receipt["msbuild"] = msbuild
        receipt["msbuild_version"] = run([msbuild, "-version", "-nologo"], repo)
        targets = ["x64", "Win32", "ARM64"] if args.all_platforms else ["x64"]
        for target in targets:
            for config in ["Debug", "Release"]:
                folder = output / target / config
                folder.mkdir(parents=True)
                run([msbuild, str(repo / "NppAIAssistant.vcxproj"), "/t:Rebuild", "/m",
                     f"/p:Platform={target}", f"/p:Configuration={config}",
                     f"/p:OutDir={folder}{os.sep}", f"/p:IntDir={folder / 'obj'}{os.sep}"],
                    repo, folder / "build.log")
                dll = folder / "NppAIAssistant.dll"
                if not dll.is_file():
                    raise RuntimeError(f"Build returned success without DLL: {dll}")
                receipt["builds"].append({"platform": target, "configuration": config,
                                          "dll": str(dll), "sha256": hashlib.sha256(dll.read_bytes()).hexdigest()})
        testdir = output / "policy"
        testdir.mkdir()
        project = testdir / "policy.vcxproj"
        policy_project(repo / "tests/context-menu-policy.test.cpp", project)
        run([msbuild, str(project), "/t:Rebuild", "/p:Platform=x64", "/p:Configuration=Release",
             f"/p:OutDir={testdir}{os.sep}", f"/p:IntDir={testdir / 'obj'}{os.sep}"],
            repo, testdir / "build.log")
        receipt["policy_output"] = run([str(testdir / "policy.exe")], repo, testdir / "test.log")
        receipt["policy_test"] = "PASS"
        preflight(repo, args.expected_commit)
        powershell = shutil.which("pwsh") or shutil.which("powershell")
        if not powershell:
            raise RuntimeError("PowerShell is required to create Windows ZIP packages")
        for build in receipt["builds"]:
            if build["configuration"] != "Release":
                continue
            package_dir = output / "packages" / build["platform"]
            package_dir.mkdir(parents=True)
            run([powershell, "-NoProfile", "-NonInteractive", "-File",
                 str(repo / "scripts/package-npp-ai-plugin.ps1"),
                 "-Platform", build["platform"], "-DllPath", build["dll"],
                 "-ExpectedDllSha256", build["sha256"], "-SourceCommit", args.expected_commit,
                 "-Candidate", "-OutDir", str(package_dir)], repo, package_dir / "package.log")
            packages = list(package_dir.glob("*.zip"))
            if len(packages) != 1:
                raise RuntimeError("Expected exactly one canonical ZIP per architecture")
            package = packages[0]
            receipt["packages"].append({"platform": build["platform"], "zip": str(package),
                                        "sha256": verify_package(package, build["sha256"])})
        preflight(repo, args.expected_commit)
        receipt["status"] = "BUILD_TEST_PASS_HOST_PENDING"
        code = 0
    except (RuntimeError, OSError) as error:
        receipt["error"] = str(error)
    finally:
        receipt["finished_utc"] = datetime.now(timezone.utc).isoformat()
        path = output / "result.json"
        path.write_text(json.dumps(receipt, ensure_ascii=False, indent=2), encoding="utf-8")
        print(json.dumps({"status": receipt["status"], "receipt": str(path),
                          "error": receipt.get("error")}, ensure_ascii=False))
    return code


if __name__ == "__main__":
    sys.exit(main())
