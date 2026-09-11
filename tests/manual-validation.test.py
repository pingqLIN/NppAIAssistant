"""Portable failure-path checks; these do not simulate Windows build success."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location("manual", Path(__file__).resolve().parents[1] / "scripts/manual-validate.py")
manual = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manual)


class ValidationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.repo = self.root / "repo"
        self.repo.mkdir()
        self.git("init")
        (self.repo / "source.txt").write_text("reviewed source\n")
        self.git("add", "source.txt")
        self.git("-c", "user.name=Validation Test", "-c", "user.email=test@example.invalid",
                 "-c", "commit.gpgsign=false", "commit", "-m", "fixture")
        self.head = self.git("rev-parse", "HEAD")

    def tearDown(self):
        self.temp.cleanup()

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.repo, stderr=subprocess.STDOUT, text=True).strip()

    def test_clean_pinned_source(self):
        self.assertEqual(manual.preflight(self.repo, self.head), self.head)

    def test_wrong_revision_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "Expected commit"):
            manual.preflight(self.repo, "0" * 40)

    def test_modified_source_rejected(self):
        (self.repo / "source.txt").write_text("unreviewed change")
        with self.assertRaisesRegex(RuntimeError, "clean isolated"):
            manual.preflight(self.repo, self.head)

    def test_untracked_source_rejected(self):
        (self.repo / "extra.cpp").write_text("unreviewed file")
        with self.assertRaisesRegex(RuntimeError, "clean isolated"):
            manual.preflight(self.repo, self.head)

    def test_failed_command_keeps_log_and_raises(self):
        log = self.root / "failed.log"
        with self.assertRaisesRegex(RuntimeError, "Command failed \\(7\\)"):
            manual.run([sys.executable, "-c", "print('build failed'); raise SystemExit(7)"], self.repo, log)
        self.assertIn("build failed", log.read_text())

    def test_successful_command_keeps_output(self):
        self.assertEqual(manual.run([sys.executable, "-c", "print('actual result')"], self.repo), "actual result")

    def test_policy_project_preserves_special_paths(self):
        source = self.root / "中文 & spaces" / "policy.test.cpp"
        project = self.root / "policy.vcxproj"
        manual.policy_project(source, project)
        tree = ET.parse(project)
        items = tree.findall(".//{*}ClCompile[@Include]")
        self.assertEqual(items[0].attrib["Include"], str(source))
        self.assertEqual(tree.find(".//{*}LanguageStandard").text, "stdcpp20")


if __name__ == "__main__":
    unittest.main()
