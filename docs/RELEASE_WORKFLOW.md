# NppAIAssistant Release Workflow

文件版本：2.1  
最後規則查核：2026-08-23  
適用專案：`pingqLIN/NppAIAssistant`  
預設正式發佈架構：`x64`

> 本文件是專案層級的長期 Release SOP。個別版本的實際 SHA、Gate 狀態、阻塞條件與驗收證據應寫入 `docs/releases/<version>-release-plan.md`。一次性的執行進度不要混入本文件。

## 1. Release 成功條件

Release 的成功條件不是「程式可以編譯」，而是以下供應鏈可追溯且一致：

```text
reviewed source commit
  -> DLL binary version
  -> canonical ZIP bytes
  -> canonical ZIP SHA-256
  -> immutable Git tag
  -> GitHub Release asset
  -> downloaded Release asset SHA-256
  -> nppPluginList entry
  -> official validator
  -> Plugins Admin local install/update test
```

任何一段不一致，Release Coordinator 必須回報 `NO-GO` 或 `BLOCKED`，不可用推論代替證據。

本流程涵蓋：

1. 最新 `main` 與功能分支的隔離整合。
2. Release Candidate（RC）版本凍結。
3. x64 Release build、靜態／安全驗證。
4. Portable Notepad++ host acceptance。
5. Canonical package 與 SHA-256。
6. Git tag / GitHub Release。
7. Release asset 下載後重新驗 hash。
8. Notepad++ 官方 `nppPluginList` 驗證與 Plugins Admin local test。
9. 正式 upstream PR 前的使用者核准 Gate。

## 2. Release 階段禁止事項

Release Agent 不得：

- 在 release branch 開發新功能或進行大型 refactor。
- 在有不明所有權未提交修改的 worktree 做 merge / package / release。
- 使用 `git reset --hard`、`git clean -fdx`、未經授權的 `git stash` 或 force push 來「清理」工作區。
- 對 resource ID / Settings UI / provider enum 衝突單純使用 `ours` 或 `theirs`。
- 移動或重寫已公開 tag，讓同一版本號代表不同 binary。
- 以本機某個 ZIP 的 hash 直接假設 GitHub Release asset hash 相同。
- 同一版本使用兩個獨立壓縮流程產生兩份「正式 ZIP」。
- 未經使用者明確核准即送出 `notepad-plus-plus/nppPluginList` 官方 PR。

## 3. Branch / Worktree 責任邊界

| 類型 | 用途 | 允許內容 |
|---|---|---|
| `main` | 已整合的專案基線 | 已審查、可維護變更 |
| feature branch | 功能開發 | 功能、測試、文件 |
| `release/vX.Y.Z.W-prep` | integration / RC / release preparation | 整合、版本、release scripts、release docs；禁止新功能 |
| `vX.Y.Z.W` tag | immutable release identity | 不得改寫 |

### 3.1 Release worktree 建立方式

先在任一已知安全的 repo worktree：

```powershell
cd Q:\Projects\NppAIAssistant
git fetch origin --prune
git worktree list --porcelain
git branch -a --list '*release/*'
```

如果遠端 release branch 已存在，而本機尚沒有同名 local branch：

```powershell
git worktree add --track `
  -b release/v0.2.0.0-prep `
  Q:\Projects\NppAIAssistant.worktrees\release-v0.2.0.0 `
  origin/release/v0.2.0.0-prep
```

如果 local `release/v0.2.0.0-prep` 已存在、且沒有被其他 worktree checkout：

```powershell
git worktree add `
  Q:\Projects\NppAIAssistant.worktrees\release-v0.2.0.0 `
  release/v0.2.0.0-prep
```

如果該 branch 已被另一個 worktree checkout，**使用既有 worktree**；不要建立同名替代 branch 後又忘記 push mapping。

若真的需要臨時 local integration branch，push 時必須明確指定：

```powershell
git push origin HEAD:release/v0.2.0.0-prep
```

### 3.2 Worktree ownership preflight

進入 release worktree 後：

```powershell
git rev-parse --show-toplevel
git branch --show-current
git rev-parse HEAD
git status --short --branch
git remote -v
git worktree list --porcelain
```

確認無進行中的 merge / rebase / cherry-pick：

```powershell
$gitDir = git rev-parse --git-dir
@('MERGE_HEAD','CHERRY_PICK_HEAD','REVERT_HEAD','rebase-merge','rebase-apply') |
  ForEach-Object {
    if (Test-Path (Join-Path $gitDir $_)) {
      throw "Git operation in progress: $_"
    }
  }
```

Worktree 不乾淨時，先辨識變更所有權；不得自行 reset、clean 或覆寫。

## 4. External Task Contract / Handoff Governance

長任務建議在 repository 外建立 authority bundle：

```text
C:\Users\miles\TaskContracts\NppAIAssistant-release-v0.2.0.0\
  TASK.json
  CONTRACT.sha256
  STATE.json
  events.jsonl
  artifacts\
```

### 4.1 TASK.json

至少保存：objective、repo、release branch、feature branch、target version、allowed actions、forbidden actions、hard gates、acceptance、artifact requirements。

範例：

```json
{
  "task_id": "NppAIAssistant-release-v0.2.0.0",
  "objective": "Prepare and validate NppAIAssistant v0.2.0.0 x64, stopping before official nppPluginList submission until explicit user approval.",
  "repository": "pingqLIN/NppAIAssistant",
  "release_branch": "release/v0.2.0.0-prep",
  "target_version": "0.2.0.0",
  "forbidden": [
    "force push",
    "overwrite unrelated dirty files",
    "rewrite published tags",
    "official nppPluginList submission without approval"
  ],
  "hard_gates": [
    "clean integration baseline",
    "build pass",
    "security pass",
    "host acceptance pass",
    "canonical release asset hash verified"
  ]
}
```

確認後封存 canonical hash。若 task authority 改變，做明確 revision；不要直接偷偷改既有 sealed contract。

### 4.2 STATE.json

只保存可恢復執行所需的最小 mutable state：

```json
{
  "status": "BLOCKED|IN_PROGRESS|PASS|FAIL",
  "current_gate": "R3",
  "baseline_sha": "<sha>",
  "last_verified": "<timestamp>",
  "blocker": "<reason-or-null>",
  "next_allowed_action": "<single next action>"
}
```

## 5. Agent / 模型分工

模型名稱可能隨產品演進；以下以責任邊界為主。

| 角色 | 建議模型 | 工作 | 權限 |
|---|---|---|---|
| Release Coordinator | GPT-5.6 Terra High | integration strategy、Gate transition、stop/go、最終報告 | 主代理 |
| Independent Reviewer | GPT-5.6 Sol High | Git diff、release safety、hash chain、metadata 審查 | 唯讀優先 |
| Executor | Luna xHigh | C++/resource/script 修改、build、重複驗證 | 僅授權 worktree |
| Second Reviewer | Sol High 或同級 reasoning model | 最終 artifact / upstream diff 第二次審查 | 唯讀 |

Release `PASS` 至少需要：

1. Executor 的實際命令／測試證據；以及
2. 至少一次獨立 reviewer 審查。

不要把同一 Agent 的自我敘述當成第二份獨立證據。

## 6. Release Gates

### R0 — Governance / Git Preflight

```powershell
git fetch origin --prune
git status --short --branch
git branch -vv
git log --graph --oneline --decorate --all -30
git remote -v
```

**PASS**：release worktree clean、branch/remote 正確、無進行中的 Git operation、authority 可追溯。

### R1 — Integration Baseline

Release branch 必須以最新 `origin/main` 為基線，語意整合所需 feature commits。

```powershell
git fetch origin --prune
git log --left-right --cherry-pick --oneline origin/main...origin/<feature>
git diff --stat origin/main...origin/<feature>
```

先模擬：

```powershell
$feature = 'origin/<feature>'
$base = git merge-base HEAD $feature
git merge-tree $base HEAD $feature
```

只有 review 判定可接受才真正 merge：

```powershell
git merge --no-ff $feature
```

Settings resource IDs、provider enum、Notepad++ hooks、prompt/memory/context-template UI 都是語意衝突高風險區。

**PASS**：main 的既有功能與 feature 的目標功能同時存在，resource IDs 唯一，worktree clean，無無關回退。

### R2 — Version Freeze

版本來源：`src/NppAIAssistantVersion.h`。

例如 `0.2.0.0`：

```cpp
#define NPPAI_VERSION_MAJOR 0
#define NPPAI_VERSION_MINOR 2
#define NPPAI_VERSION_PATCH 0
#define NPPAI_VERSION_BUILD 0
#define NPPAI_VERSION_STRING "0.2.0"
#define NPPAI_VERSION_STRING_FULL "0.2.0.0"
```

確認：

```powershell
rg -n "NPPAI_VERSION" src/NppAIAssistantVersion.h
```

**PASS**：source version、release plan、planned tag、package version 一致。

### R3 — Build / Static Verification

優先使用專案可重現路徑。若本機 `.vcxproj` toolset 與已安裝 Visual Studio 不相容，可補做 CMake 隔離驗證，但要在 evidence 記錄 toolchain。

```powershell
cmake -S . -B build/x64/Release -A x64
cmake --build build/x64/Release --config Release --parallel 2
```

執行 release branch 上存在的專案驗證：

```powershell
.\scripts\verify-security-regressions.ps1
git diff --check
```

確認 binary version 與 hash：

```powershell
$dll = 'build\x64\Release\plugins\NppAIAssistant\NppAIAssistant.dll'
(Get-Item $dll).VersionInfo | Format-List FileVersion,ProductVersion
Get-FileHash $dll -Algorithm SHA256
```

**PASS**：x64 Release DLL 實際產出、驗證腳本 PASS、binary version 正確、DLL SHA-256 已記錄。

### R4 — Portable Notepad++ Host Acceptance

只能使用隔離 portable/debug host；不要覆寫正式 Notepad++ 安裝。

最低矩陣：

| Case | Expected |
|---|---|
| Plugin load/unload | 無 crash / hang |
| Settings | 控制項無錯位、無重疊 |
| timeout default | `30` |
| timeout valid | `1–300` 可保存並重開保留 |
| timeout invalid | `0`、`301`、非數值被拒絕 |
| 無選取右鍵 | native menu |
| 選取 + 一般右鍵 | native menu |
| 選取 + Ctrl + mouse right-click | AI menu |
| Shift+F10 | native menu |
| loopback endpoint | 合法 `/v1` 可工作 |
| remote / malformed endpoint | 被拒絕 |
| removed saved model | 明確要求重新選擇，不 silent fallback |

需保存 Notepad++ version、plugin DLL SHA-256、結果與失敗截圖/log。

**FAIL**：修正後重新跑 R3/R4；不能直接往 package/release 前進。

### R5 — Canonical Package

Release 每一個 architecture/version 只能有一份 canonical ZIP bytes。

```powershell
.\scripts\package-npp-ai-plugin.ps1 `
  -Platform x64 `
  -Configuration Release `
  -Version <X.Y.Z.W>
```

然後：

```powershell
.\scripts\smoke-package-install.ps1
.\scripts\verify-release-readiness.ps1 `
  -Platform x64 `
  -Configuration Release `
  -Version <X.Y.Z.W>
Get-FileHash .\dist\NppAIAssistant-<X.Y.Z.W>-x64.zip -Algorithm SHA256
```

ZIP 要求：

- root 有 `NppAIAssistant.dll`。
- 不含 `.pdb`。
- DLL binary version = package version。
- manifest hash = canonical ZIP hash。

### R5A — Current GitHub Actions Canonical Packaging Contract

本專案 release-prep 流程已把責任拆開：

- `.github/workflows/CI_build.yml`：一般 build validation；不建立 GitHub Release。
- `.github/workflows/release.yml`：唯一正式 release package producer；以 `scripts/package-npp-ai-plugin.ps1` 建立 canonical x64 ZIP，跨 job 傳遞同一 artifact，tag 時上傳同一 bytes，發布後重新下載驗 SHA-256。
- `scripts/validate-npp-plugin-entry.ps1`：用當下官方 `pl.schema` 驗 generated entry 的 schema shape；final submission 額外要求 direct HTTPS ZIP。

任何後續 CI 修改都必須維持「package once, promote same bytes」，不得恢復另一個獨立 `7z` release ZIP producer。

### R6 — Release Commit / Tag / GitHub Release

先明確 stage：

```powershell
git status --short
git add <explicit-reviewed-paths>
git diff --cached --check
git diff --cached --name-status
git commit -m "release: prepare NppAIAssistant v<X.Y.Z.W>"
git push -u origin release/v<X.Y.Z.W>-prep
```

只有 R0–R5 review PASS 才允許 tag：

```powershell
git tag -a v<X.Y.Z.W> -m "NppAIAssistant v<X.Y.Z.W>"
git show --no-patch --decorate v<X.Y.Z.W>
git push origin v<X.Y.Z.W>
```

Tag 必須指向 reviewed release commit。Tag push 會啟動 `.github/workflows/release.yml`；因此 tag 本身就是發布動作，不能當成「先試試看」。

### R7 — Post-upload Supply-chain Verification

正式 Plugins Admin hash 必須由實際 GitHub Release asset bytes 驗證。

```powershell
$asset = 'https://github.com/pingqLIN/NppAIAssistant/releases/download/v<X.Y.Z.W>/NppAIAssistant-<X.Y.Z.W>-x64.zip'
$out = Join-Path $env:TEMP 'NppAIAssistant-release-x64.zip'
Invoke-WebRequest -Uri $asset -OutFile $out
Get-FileHash $out -Algorithm SHA256
```

要求：

```text
pre-upload canonical ZIP SHA-256
=
workflow artifact SHA-256
=
downloaded GitHub Release ZIP SHA-256
=
nppPluginList entry id
```

### R8 — Official nppPluginList Validation

正式送出前重新查核官方 repo，不依賴舊抄本：

- `https://github.com/notepad-plus-plus/nppPluginList`
- `https://github.com/notepad-plus-plus/nppPluginList/blob/master/pl.schema`
- `https://github.com/notepad-plus-plus/nppPluginList/blob/master/validator.py`
- `https://npp-user-manual.org/docs/plugins/#plugins-admin`

目前官方驗證包含：schema、下載 ZIP、SHA-256、ZIP validity、root DLL、DLL binary version、uniqueness。

在 `nppPluginList` fork/branch 只更新預期 architecture JSON，例如 x64：`src/pl.x64.json`。

```powershell
python -m pip install -r requirements.txt
python validator.py x64
```

另外依官方說明在 portable/debug Notepad++ 做 Plugins Admin local install/update test。

**PASS**：validator 成功、local install/update 成功、upstream diff 只有預期 JSON entry。

### R9 — Official PR Approval Gate

在使用者核准前只維護 draft text / local fork branch，不建立正式 upstream PR。

送出前確認：

- Release tag immutable。
- direct ZIP URL 可匿名下載。
- downloaded ZIP SHA-256 = `id`。
- DLL version 一致。
- official validator PASS。
- Plugins Admin local install/update PASS。
- upstream diff 只改預期 entry。
- 使用者明確核准。

## 7. Git / GitHub 操作速查

### 比較 main 與 feature

```powershell
git fetch origin --prune
git log --left-right --cherry-pick --oneline origin/main...origin/<feature>
git diff --stat origin/main...origin/<feature>
```

### 更新 release branch

```powershell
git status --short --branch
git fetch origin --prune
git merge --ff-only origin/release/v0.2.0.0-prep
```

只有確認目前 local branch 沒有額外 commit 時才使用 `--ff-only` 更新；有分岔就先 review，不要自動 rebase/reset。

### 顯式 stage

```powershell
git add docs/RELEASE_WORKFLOW.md
git add docs/releases/v0.2.0.0-release-plan.md
git diff --cached --check
git diff --cached --name-status
```

Release 階段避免無差別 `git add .`，除非整個 worktree 內容與所有權已確認。

### 安全 push

```powershell
git push -u origin release/v0.2.0.0-prep
```

如果使用臨時 local branch：

```powershell
git push origin HEAD:release/v0.2.0.0-prep
```

## 8. Rollback / Recovery

### Release 前失敗

不要 tag。修正後從受影響的最早 Gate 重新執行。

### Tag 尚未 push

可在明確記錄事件後刪除錯誤的 local tag，再於正確 commit 重建。

### Tag 已 push / Release 已公開

不要移動 tag 或用同版號替換 binary。發新 patch/build 版本，例如 `0.2.0.1`，重跑完整 Gate。

### Release asset hash 不一致

視為 supply-chain failure。停止 Plugins Admin 提交；先確認 canonical package、workflow artifact、Git tag 與 public asset 的來源鏈。不得只改 JSON hash 來「配合」不明 asset。

## 9. Evidence 最小集合

每個 release 至少保存：

```text
release-plan.md
integration/release commit SHA
build toolchain summary
security/regression result
DLL SHA-256
host acceptance matrix
canonical ZIP filename + SHA-256
Git tag
GitHub Release URL
post-download SHA-256
generated nppPluginList entry
official validator result
Plugins Admin local-test result
final upstream PR URL (only after approval)
```

## 10. Agent Handoff 最小提示詞

```text
Read docs/RELEASE_WORKFLOW.md and the active docs/releases/<version>-release-plan.md first.
Verify repo root, branch, HEAD, worktree cleanliness, remotes, and in-progress Git operations.
Do not reset, clean, stash, rebase, force-push, rewrite tags, or overwrite unrelated changes.
Execute only the current release gate.
Record exact commands, outputs, commit SHA, binary/package hashes, and blockers.
Do not submit the official notepad-plus-plus/nppPluginList PR without explicit user approval.
```

Reviewer 必須獨立確認：Gate 輸入基線、測試是否真的執行、artifact/hash 是否同一 bytes、以及每一個 `PASS` 是否有實際 evidence。

## 11. Final Release Review Checklist

- [ ] R0 Governance / Git preflight PASS
- [ ] R1 Integration baseline PASS
- [ ] R2 Version freeze PASS
- [ ] R3 Build / static verification PASS
- [ ] R4 Notepad++ host acceptance PASS
- [ ] R5 Canonical package PASS
- [ ] R6 Release commit / tag / GitHub Release PASS
- [ ] R7 Downloaded release asset SHA-256 PASS
- [ ] R8 Official validator + Plugins Admin local test PASS
- [ ] R9 User approval obtained before official PR submission

只有全部適用 Gate 都有證據時，Release Coordinator 才能回報 `RELEASE READY`。