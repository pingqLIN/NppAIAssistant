#pragma once

#include "StructuredOutput.h"

#include <cstddef>
#include <string>
#include <vector>

// SettingsStorage reads profile values with a 65535 UTF-16 code-unit buffer.
// A template can double in size when INI escaping backslashes/newlines, so this
// limit preserves round-trip headroom for every prompt-section write path.
constexpr size_t kMaxPromptSectionTemplateChars = 30000;

enum class TaskProfile {
  General,
  Review,
  Transform,
  CodeChange,
  Documentation,
};

enum class PromptInteractionMode { SingleTurn };
enum class PromptToolAccess { None };
enum class PromptSourceTrust { RuntimePolicy, UserInstruction, UntrustedData };
enum class PromptEvidenceMode { VisibleRequestOnly };

// The editor exposes only these built-in policy/template sections as
// user-lockable. Runtime, task, memory, and source data remain governed by
// their dedicated request paths and are never made editable by this state.
enum class PromptSectionLockTarget {
  PromptPolicy,
  Identity,
  Rules,
  Assignment,
  ScenarioModules,
  OutputContract,
  RuntimeContext,
  TaskInstruction,
  SourceContext,
  Memory,
};

struct PromptSectionLockState {
  bool promptPolicyLocked = true;
  bool identityLocked = true;
  bool rulesLocked = true;
  bool assignmentLocked = true;
  bool scenarioModulesLocked = true;
  bool outputContractLocked = true;
};

// These are deliberately constrained layouts, not arbitrary prompt injection
// positions. Both preserve Runtime Policy, Prompt Policy, Task Profile, and
// Output Contract before every request-data section.
enum class PromptCompositionOrder {
  CacheStablePolicyFirst,
  TaskBeforeMemory,
};

enum class PromptCompositionLayer {
  RuntimePolicy,
  PromptPolicy,
  TaskProfile,
  OutputContract,
  MemoryData,
  TaskData,
  SourceData,
};

// This is descriptive metadata for a request, not a substitute for runtime
// enforcement. Provider transport, structured validation, and editor writes
// remain enforced by their respective runtime code paths.
struct GovernanceProfile {
  int policyVersion = 1;
  TaskProfile taskProfile = TaskProfile::General;
  PromptInteractionMode interactionMode = PromptInteractionMode::SingleTurn;
  PromptToolAccess toolAccess = PromptToolAccess::None;
  PromptSourceTrust sourceTrust = PromptSourceTrust::UntrustedData;
  PromptEvidenceMode evidenceMode = PromptEvidenceMode::VisibleRequestOnly;
  OutputMode outputMode = OutputMode::Text;
  bool structuredValidationEnabled = false;
};

struct PromptManifestInput {
  std::wstring sectionId;
  size_t characterCount = 0;
  size_t estimatedTokens = 0;
  bool included = false;
  PromptSourceTrust trust = PromptSourceTrust::RuntimePolicy;
};

struct PromptAssemblyManifestEntry {
  std::wstring sectionId;
  size_t characterCount = 0;
  size_t estimatedTokens = 0;
  bool included = false;
  PromptSourceTrust trust = PromptSourceTrust::RuntimePolicy;
};

struct PromptAssemblyManifest {
  int policyVersion = 1;
  TaskProfile taskProfile = TaskProfile::General;
  OutputMode outputMode = OutputMode::Text;
  bool structuredValidationEnabled = false;
  std::vector<PromptAssemblyManifestEntry> entries;
};

TaskProfile taskProfileForPromptPreset(int promptPreset, bool forceTransform);
std::wstring taskProfileName(TaskProfile profile);
std::wstring promptSourceTrustName(PromptSourceTrust trust);
bool isPromptSectionLockable(PromptSectionLockTarget target);
bool isPromptSectionLocked(const PromptSectionLockState &state,
                           PromptSectionLockTarget target);
void setPromptSectionLocked(PromptSectionLockState &state,
                            PromptSectionLockTarget target, bool locked);
bool canEditPromptSection(const PromptSectionLockState &state,
                          PromptSectionLockTarget target);
bool isPromptSectionTemplateStorageSafe(const std::wstring &templateText);
std::wstring buildPromptPolicyText(const GovernanceProfile &profile);
std::wstring buildTaskProfileText(TaskProfile profile);
std::wstring buildRuntimeDisclosureText(const GovernanceProfile &profile);
PromptCompositionOrder sanitizePromptCompositionOrder(int rawValue);
std::wstring promptCompositionOrderName(PromptCompositionOrder order);
bool promptCompositionOrderPrefersStablePrefix(PromptCompositionOrder order);
std::vector<PromptCompositionLayer>
promptCompositionLayers(PromptCompositionOrder order);
bool isSafePromptCompositionLayerSequence(
    const std::vector<PromptCompositionLayer> &layers);
std::wstring wrapUntrustedPromptData(const std::wstring &elementName,
                                     const std::wstring &content);
size_t estimatePromptTokens(const std::wstring &value);
PromptAssemblyManifest buildPromptAssemblyManifest(
    const GovernanceProfile &profile,
    const std::vector<PromptManifestInput> &inputs);
std::wstring formatPromptAssemblyManifestSummary(
    const PromptAssemblyManifest &manifest);
