# GitHub Actions unavailable: manual Windows validation

This is an alternative execution path during the reported billing interruption.
It does not change billing, repository protection, required checks, or existing
CI results. A manually dispatched GitHub workflow still needs Actions admission.

## Current evidence (2026-09-11)

- PR #6 run `34564051080`: six failed jobs, `steps=[]`, `runner_id=0`.
- Billing is the owner's suspected cause; account billing details have not been
  independently accessed. Public repositories using standard hosted runners
  have free execution minutes according to the [GitHub billing documentation](https://docs.github.com/en/billing/concepts/product-billing/github-actions).
- Repository rulesets API returned `[]`. Classic branch protection could not be
  read by the connected integration (`403 Resource not accessible by integration`).
  This does not prove that no branch protection exists.
- Both Local Files MCP diagnostics and TB2 probe returned `-32603 Internal error`.
  No local Windows execution or agent dispatch has occurred in this session.

## Recommended temporary path

Use a clean, isolated Windows worktree and run the build directly, outside
GitHub Actions. This consumes no GitHub-hosted runner minutes and does not rely
on the Actions scheduler. Prerequisites are Python 3, Git, Visual Studio 2022 C++
tools (v143) and Windows SDK. ARM64 tools are needed only for the full matrix.

Suggested worktree: `Q:\Projects\_worktrees\NppAIAssistant-pr6-manual`.
Open a new PowerShell terminal for this task. Before creating it, inspect existing
worktrees and reuse a matching clean worktree if one already exists. Never reset
or overwrite an existing checkout to make it match.

```powershell
Set-Location Q:\Projects\NppAIAssistant
git fetch origin fix/optional-ai-context-menu-20260911
git worktree list
# Review the fetched SHA and diff before substituting the full SHA below.
git show --stat FETCH_HEAD
git worktree add --detach Q:\Projects\_worktrees\NppAIAssistant-pr6-manual <reviewed-full-sha>
Set-Location Q:\Projects\_worktrees\NppAIAssistant-pr6-manual
python scripts/manual-validate.py --expected-commit <reviewed-full-sha>
```

Default execution builds x64 Debug and Release, builds and runs the eight
context-menu policy cases, and verifies the source remains clean and unchanged.
Add `--all-platforms` to build the original six-target CI matrix; the policy
executable is x64 and therefore needs an x64 Windows host (or compatible execution).
The script does not download or install tools, call any model API, start Notepad++,
install a DLL, publish a release, or write a GitHub check status.

Each run creates a fresh `npp-manual-validation-*` directory under the Windows
temporary directory. It contains build logs, DLLs, their SHA-256 values and
`result.json`. Build output is redirected outside the source worktree. Preserve
the directory with the review evidence before clearing temporary files.

Exit code 0 with `BUILD_TEST_PASS_HOST_PENDING` means only that the requested
build matrix and policy tests passed. Exit code 1 / `FAILED` records the actual
failure; missing Windows or tools cannot produce a pass. Follow
[`tests/README.md`](../tests/README.md) for the mandatory Notepad++ host matrix.
Use an isolated portable host and keep the tested DLL SHA-256 with the results.

## Manual merge decision

Only after build, source review and actual host acceptance pass for the same
commit may the owner use GitHub's normal manual merge controls. Keep the original
red CI result visible and attach the manual evidence. If GitHub blocks merging
because a required check is missing, inspect the exact active rule in repository
Settings first. This procedure does not remove checks or grant bypass roles.
After merging, the reviewed tree must match the merged source, or validation must
be repeated. Release publication still follows `RELEASE_WORKFLOW.md`.

No workflow YAML changes are required for this temporary path. When billing is
resolved, rerun CI for the current reviewed commit and return to normal automated
validation. Switching to self-hosted runners would add maintenance and untrusted
PR execution concerns and is not needed for this one-off task.

## Test the validation entrypoint itself

```powershell
python tests/manual-validation.test.py
```

These portable tests exercise real Git revision/dirty-worktree rejection,
subprocess failure and logging, and generated MSBuild XML path escaping. They
do not stand in for a Windows build.
