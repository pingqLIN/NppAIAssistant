# NppAIAssistant Release Workflow

文件版本：2.0  
最後規則查核：2026-08-23  
適用專案：`pingqLIN/NppAIAssistant`  
預設發布架構：`x64`

> 本文件是專案層級的長期 Release SOP。個別版本的實際狀態、SHA、阻塞條件與驗收證據應寫入 `docs/releases/<version>-release-plan.md`，不要把一次性的進度混入本文件。

## 1. 目標與成功條件

本流程涵蓋：

1. 功能分支整合與乾淨 release baseline。
2. DLL 版本、Git branch、tag、GitHub Release 與 Plugins Admin metadata 的一致性。
3. Release Candidate（RC）建置、靜態驗證、Notepad++ host acceptance、套件與 SHA-256 驗證。
4. GitHub Release asset 的供應鏈驗證。
5. Notepad++ `nppPluginList` x64 entry 更新與正式 PR 前審查。
6. AI Agent / Codex 多代理執行時的 authority、handoff、證據與停止條件。

成功條件不是「程式可編譯」而已，而是下列鏈條可追溯且全部通過：

```text
source commit
  -> DLL binary version
  -> canonical ZIP bytes
  -> ZIP SHA-256
  -> Git tag
  -> GitHub Release asset
  -> downloaded release asset SHA-256
  -> nppPluginList entry
  -> Plugins Admin local install/update test
```

任何一段不一致都必須停止發布。

## 2. 不在本流程內的事項

- 不在 release branch 加入新功能。
- 不把大型 refactor、provider redesign 或 UI redesign 混入 release commit。
- 不 force-push `main`、已發布 tag 或正式 release branch。
- 不以未驗證的本機 ZIP hash 代替正式 GitHub Release asset 的 hash。
- 不在使用者核准前送出 Notepad++ 官方 `nppPluginList` PR。

## 3. Authority 與工作區拓樸

### 3.1 Branch 責任邊界

| Branch 類型 | 用途 | 可接受變更 |
|---|---|---|
| `main` | 已整合的專案基線 | 已審查、可維護變更 |
| feature branch | 功能開發 | 功能、測試、文件 |
| `release/vX.Y.Z.W-prep` | release integration / RC | 整合、版本、發版工具、release docs；禁止新功能 |
| tag `vX.Y.Z.W` | immutable release identity | 不得移動或重寫 |

### 3.2 建議本機 worktree

不要直接在已有未提交變更的主工作目錄做 release integration。

```powershell
git fetch origin --prune

git worktree add `
  Q:\Projects\NppAIAssistant.worktrees\release-v0.2.0.0 `
  -b release/v0.2.0.0-prep `
  origin/main
```

若遠端 release branch 已存在：

```powershell
git worktree add `
  Q:\Projects\NppAIAssistant.worktrees\release-v0.2.0.0 `
  release/v0.2.0.0-prep
```

### 3.3 禁止破壞既有工作

Release Agent 不得使用：

```text
git reset --hard
git clean -fdx
git checkout -- <unowned-file>
git stash
git rebase --onto ...   # 未經明確規劃時
push --force / --force-with-lease
```

如果發現 worktree dirty、merge/rebase/cherry-pick 進行中，先記錄並停止寫入。

## 4. Task Contract / Handoff Governance

長任務建議在 repository 外維護 authority bundle，例如：

```text
C:\Users\miles\TaskContracts\NppAIAssistant-release-v0.2.0.0\
  TASK.json
  CONTRACT.sha256
  STATE.json
  events.jsonl
  artifacts\
```

### 4.1 TASK.json 應至少包含

```json
{
  "task_id": "NppAIAssistant-release-v0.2.0.0",
  "objective": "Produce and validate NppAIAssistant v0.2.0.0 x64 release candidate and prepare, but do not submit, the nppPluginList update PR without user approval.",
  "repository": "pingqLIN/NppAIAssistant",
  "release_branch": "release/v0.2.0.0-prep",
  "target_version": "0.2.0.0",
  "allowed": [
    "isolated worktree edits",
    "build and test",
    "release preparation",
    "draft metadata"
  ],
  "forbidden": [
    "force push",
    "rewrite published tags",
    "official nppPluginList submission without approval",
    "overwrite unrelated dirty files"
  ],
  "hard_gates": [
    "clean integration baseline",
    "build pass",
    "security pass",
    "host acceptance pass",
    "canonical asset hash verified"
  ]
}
```

`TASK.json` 確認後以 canonical hash 封存；若 authority 內容改變，必須明確 revision，而不是默默修改。

### 4.2 STATE.json

只保存當前執行狀態：

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

## 5. 建議模型與 Agent 分工

模型名稱會隨環境更新；以下是能力分工，不是硬依賴。

| 角色 | 建議模型 | 工作 | 權限 |
|---|---|---|---|
| Release Coordinator | GPT-5.6 Terra High | 整合策略、Gate、決策、最終回報 | 主代理 |
| Reviewer | GPT-5.6 Sol High | Git diff、release safety、文件與規則審查 | 唯讀優先 |
| Executor | Luna xHigh | 檔案修改、資料蒐集、腳本與重複驗證 | 僅授權工作區 |
| Second Reviewer | Sol High 或同級 reasoning model | 獨立重審 hash / metadata / release evidence | 唯讀 |

原則：規劃者與最終 reviewer 不應只依賴同一條推理鏈。Release 的「PASS」至少要有執行證據與一次獨立審查。

## 6. Release Gate 定義

### R0 — Governance / Git Preflight

執行：

```powershell
git rev-parse --show-toplevel
git branch --show-current
git rev-parse HEAD
git status --short --branch
git worktree list --porcelain
git remote -v
git fetch origin --prune
```

確認沒有 merge/rebase/cherry-pick：

```powershell
$gitDir = git rev-parse --git-dir
@('MERGE_HEAD','CHERRY_PICK_HEAD','REVERT_HEAD','rebase-merge','rebase-apply') |
  ForEach-Object {
    $p = Join-Path $gitDir $_
    if (Test-Path $p) { Write-Error "Git operation in progress: $_" }
  }
```

**PASS 條件**：目標 worktree clean、remote 正確、authority branch 正確、無進行中的 Git operation。

### R1 — Integration Baseline

Release branch 必須以最新 `origin/main` 為基線，再整合目標 feature commits。

```powershell
git fetch origin --prune
git log --graph --oneline --decorate --all -30
git merge-base HEAD origin/main
git log --left-right --cherry-pick --oneline origin/main...<feature-branch>
```

整合前先做 merge simulation：

```powershell
git merge-tree $(git merge-base HEAD <feature-branch>) HEAD <feature-branch>
```

若真正 merge：

```powershell
git merge --no-ff <feature-branch>
```

資源檔、Settings UI、provider enum、resource IDs、Notepad++ message hooks 若衝突，視為語意衝突，不得單純採用 `ours`/`theirs`。

**PASS 條件**：release branch 包含所需 main 與 feature 功能，`git status` clean，且下一 Gate 可重現。

### R2 — Version Freeze

候選版本先在 release plan 宣告；完成 integration 後才修改 binary version。

目前版本來源：

```text
src/NppAIAssistantVersion.h
```

例如：

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
rg -n "NPPAI_VERSION_(MAJOR|MINOR|PATCH|BUILD|STRING)" src/NppAIAssistantVersion.h
```

**PASS 條件**：source version、預計 tag、release plan、package version 全部一致。

### R3 — Build / Static Verification

優先使用專案可重現路徑。若 `.vcxproj` toolset 與本機 VS 不相容，可用 CMake 作隔離驗證，但要在證據中記錄 toolchain。

```powershell
cmake -S . -B build/x64/Release -A x64
cmake --build build/x64/Release --config Release --parallel 2
```

執行專案檢查：

```powershell
.\scripts\verify-security-regressions.ps1
git diff --check
```

**PASS 條件**：x64 Release DLL 實際產出；security/regression script 全數通過；無 whitespace error。

### R4 — Notepad++ Host Acceptance

必須在 portable / isolated Notepad++ 測試，不覆寫正式安裝。

最低矩陣：

| Case | Expected |
|---|---|
| Plugin load | 啟動與 unload 無 crash/hang |
| Settings 開啟 | 控制項完整、無 ID 錯位或重疊 |
| timeout default | `30` |
| timeout valid | `1–300` 可儲存並重開保留 |
| timeout invalid | `0`、`301`、文字輸入被拒絕 |
| 無選取右鍵 | Notepad++ native context menu |
| 有選取、一般右鍵 | native context menu |
| Ctrl + 滑鼠右鍵 + selection | AI context menu |
| Shift+F10 | native context menu |
| local endpoint | `127.0.0.1` / `::1` 合法路徑可用 |
| remote endpoint | 拒絕 |
| redirect / proxy bypass | 不跟隨 redirect、不使用 proxy |
| removed saved model | 不自動切換其他模型 |

需保存：Notepad++ version、plugin DLL SHA-256、測試結果、失敗截圖或 log。

### R5 — Canonical Package

**Release 只能有一份 canonical ZIP bytes。**

目前專案的 package script：

```powershell
.\scripts\package-npp-ai-plugin.ps1 -Platform x64 -Configuration Release
```

驗證：

```powershell
.\scripts\smoke-package-install.ps1
.\scripts\verify-release-readiness.ps1 -Platform x64 -Configuration Release
```

ZIP 必須：

- 根目錄直接包含 `NppAIAssistant.dll`。
- DLL filename 與 `folder-name` 相同。
- 不含 `.pdb`。
- DLL binary version = package version。
- manifest SHA-256 = ZIP bytes SHA-256。

**重要：不可在上傳 GitHub Release 後重新壓縮同版本 ZIP，再拿新 hash 填入 Plugins Admin。**

### R6 — Commit / Tag / GitHub Release

Release commit 建議：

```powershell
git add <explicit-paths>
git diff --cached --check
git diff --cached --stat
git commit -m "release: prepare NppAIAssistant v0.2.0.0"
```

先 push branch：

```powershell
git push -u origin release/v0.2.0.0-prep
```

Tag 必須指向已驗證的 release commit：

```powershell
git tag -a v0.2.0.0 -m "NppAIAssistant v0.2.0.0"
git show --no-patch --decorate v0.2.0.0
git push origin v0.2.0.0
```

若專案採 GitHub Actions 自動 Release，tag push 前必須先確認 workflow 使用的 package 路徑與 R5 canonical ZIP 相同。

### R7 — Post-upload Supply-chain Verification

這是正式 Plugins Admin hash 的唯一可信來源：**實際從 GitHub Release URL 下載回來的 asset bytes**。

```powershell
$asset = 'https://github.com/pingqLIN/NppAIAssistant/releases/download/v0.2.0.0/NppAIAssistant-0.2.0.0-x64.zip'
$out = Join-Path $env:TEMP 'NppAIAssistant-0.2.0.0-x64.release.zip'
Invoke-WebRequest -Uri $asset -OutFile $out
Get-FileHash $out -Algorithm SHA256
```

比較：

1. 本機 canonical ZIP hash。
2. GitHub Release 下載後 hash。
3. plugin entry `id`。

三者必須完全一致。

### R8 — Official nppPluginList Validation

Notepad++ 官方目前規則要求：

- x64 更新 `src/pl.x64.json`。
- `folder-name` 唯一，且等於 DLL basename。
- `id` 是下載 ZIP SHA-256。
- `version` 與 DLL binary version 一致。
- `repository` 是可直接下載的 `.zip` URL。
- ZIP root 含同名 DLL。
- required fields：`folder-name`、`display-name`、`version`、`id`、`repository`、`description`、`author`、`homepage`。

官方來源：

- https://github.com/notepad-plus-plus/nppPluginList
- https://github.com/notepad-plus-plus/nppPluginList/blob/master/pl.schema
- https://github.com/notepad-plus-plus/nppPluginList/blob/master/validator.py
- https://npp-user-manual.org/docs/plugins/#plugins-admin

正式 PR 前，在 nppPluginList fork/branch 內只修改相應 JSON，並執行官方 validator。官方 validator 不只做 JSON schema：還會下載 ZIP、比較 SHA-256、確認 ZIP 內 DLL、讀 DLL version 並比對版本。

建議：

```powershell
python -m pip install -r requirements.txt
python validator.py x64
```

**PASS 條件**：官方 validator 成功，且 portable Plugins Admin local-test 能安裝 / 更新本外掛。

### R9 — Official PR Approval Gate

PR 只能修改官方要求的 JSON file，不將本專案文件或 binary 放入 `nppPluginList` PR。

在使用者核准前，只建立草稿內容，不送出 PR。

送出前至少確認：

- Release tag immutable。
- direct ZIP URL 可匿名下載。
- SHA-256 與下載 asset 一致。
- DLL version 一致。
- official validator PASS。
- Plugins Admin local install/update PASS。
- PR diff 只修改預期 JSON entry。

## 7. Canonical Package 與 GitHub Actions 設計

### 7.1 目前必須避免的雙套件問題

如果本機 `package-npp-ai-plugin.ps1` 建立 ZIP A，而 tag workflow 又用 `7z` 建立 ZIP B，即使兩者包含同一 DLL，ZIP bytes 與 SHA-256 仍通常不同。

因此 release workflow 必須選一種策略：

**建議策略：Package Once, Promote Same Bytes**

1. Build。
2. 執行 `package-npp-ai-plugin.ps1` 一次，得到 canonical ZIP。
3. 驗證 canonical ZIP。
4. GitHub Actions 將同一 artifact 上傳到 workflow artifact。
5. tag release job 下載同一 artifact，不重新壓縮。
6. GitHub Release 上傳同一 ZIP bytes。
7. 下載 release asset 再驗 hash。

不要使用兩個不同壓縮命令分別製作「測試 ZIP」與「Release ZIP」。

### 7.2 建議 CI 分層

```text
build-test (matrix)
  x64 Release -> canonical package
  optional Win32/ARM64 build verification
       |
       v
release-candidate gate
       |
       v
release job (tag only)
  downloads canonical artifact
  uploads exact bytes to GitHub Release
       |
       v
post-release verify
  download release asset
  SHA-256 compare
```

正式發版前，CI workflow 本身的修改也必須先經普通 branch / PR 執行驗證。

## 8. GitHub / Git 指令速查

### 比較 main 與 feature

```powershell
git fetch origin --prune
git log --left-right --cherry-pick --oneline origin/main...origin/<feature>
git diff --stat origin/main...origin/<feature>
```

### 建立 release integration branch

```powershell
git switch --create release/v0.2.0.0-prep origin/main
```

### 顯式 stage

```powershell
git add docs/RELEASE_WORKFLOW.md
git add docs/releases/v0.2.0.0-release-plan.md
```

避免 release 階段使用無差別 `git add .`，除非已確認整個 worktree 所有權。

### 檢查提交

```powershell
git diff --cached --check
git diff --cached --name-status
git show --stat --oneline HEAD
```

### 推送而不改寫歷史

```powershell
git push -u origin release/v0.2.0.0-prep
```

## 9. Rollback / Recovery

### Release 前失敗

不要 tag。修正後重新跑 R0–R5。

### Tag 已建立但未 push

若確認 tag 錯誤，可在本機刪除並重建；必須記錄事件。

### Tag 已 push / Release 已公開

不要移動 tag 去假裝同一版本是另一個 binary。採新 patch/build 版本，例如 `0.2.0.1`，重新走完整流程。

### GitHub Release asset hash 不一致

視為供應鏈失敗：停止 Plugins Admin 提交。不得只修改 JSON hash 來配合一個來源不明的 asset；先確認真正 canonical artifact 與 tag source。

## 10. Evidence / Artifact 最小集合

每個 release 至少保存：

```text
release-plan.md
source commit SHA
build toolchain summary
security/regression result
host acceptance matrix
canonical ZIP filename
canonical ZIP SHA-256
Git tag
GitHub Release URL
post-download SHA-256
final nppPluginList entry
validator result
Plugins Admin local-test result
final PR URL (submitted only after approval)
```

## 11. Agent Handoff 最小提示詞

執行代理每次接手先做：

```text
Read docs/RELEASE_WORKFLOW.md and the active docs/releases/<version>-release-plan.md.
Verify repo root, branch, HEAD, worktree cleanliness, remotes, and any in-progress Git operation.
Do not reset, clean, stash, rebase, force-push, rewrite tags, or overwrite unrelated changes.
Execute only the current release gate. Record exact commands, outputs, commit SHA, artifact hashes, and blockers.
Do not submit the official nppPluginList PR without explicit user approval.
```

Reviewer 必須獨立確認：

- gate 的輸入基線。
- 測試是否真的執行。
- artifact/hash 是否指向同一 bytes。
- 所有 PASS 是否有證據。

## 12. Release Review Checklist

- [ ] R0 Governance / Git preflight PASS
- [ ] R1 Integration baseline PASS
- [ ] R2 Version freeze PASS
- [ ] R3 Build / static verification PASS
- [ ] R4 Notepad++ host acceptance PASS
- [ ] R5 Canonical package PASS
- [ ] R6 Release commit/tag/asset PASS
- [ ] R7 Downloaded release asset SHA-256 PASS
- [ ] R8 Official validator + Plugins Admin local test PASS
- [ ] R9 User approval obtained before official PR submission

只有全部適用 Gate 都有證據時，Release Coordinator 才能回報 `RELEASE READY`。