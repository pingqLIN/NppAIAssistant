// This file is part of Notepad++ project
// Copyright (C)2025 Don HO <don.h@free.fr>
//
// Structured-output primitives are deliberately provider-neutral.  Transport
// adapters decide whether a provider can use a schema; this module validates
// the schema and the returned JSON without placing schema text in a prompt.

#pragma once

#include <string>

enum class OutputMode { Text = 0, Markdown = 1, Json = 2, StructuredJson = 3 };

enum class StructuredSchemaPreset {
  GenericStructuredResult = 0,
  DocumentReview = 1,
};

enum class StructuredOutputSupport { Native, Unsupported, Unknown };

enum class StructuredOutputTransport { ChatCompletions, Responses };

enum class ResponseFailure {
  None,
  HttpError,
  ProviderResponseError,
  EmptyResponse,
  JsonParseError,
  SchemaValidationError,
  GenerationIncomplete,
  UnsupportedStructuredOutput,
};

struct StructuredOutputConfig {
  bool enabled = false;
  std::wstring schemaName;
  std::wstring schema;
  bool strict = true;
  bool validateResponse = true;
};

struct StructuredOutputValidationResult {
  bool success = false;
  ResponseFailure failure = ResponseFailure::SchemaValidationError;
  std::wstring errorMessage;
};

struct ProviderEnvelopeResult {
  bool success = false;
  ResponseFailure failure = ResponseFailure::ProviderResponseError;
  std::wstring content;
  std::wstring errorMessage;
  // Older compatible Chat servers may omit completion metadata. Preserve that
  // text compatibility, but let Structured JSON fail closed before validation.
  bool completionStatusKnown = true;
};

struct StructuredOutputFormatResult {
  bool success = false;
  ResponseFailure failure = ResponseFailure::SchemaValidationError;
  // A leading-comma JSON object member ready to append to a request object.
  std::wstring jsonMember;
  std::wstring errorMessage;
};

constexpr size_t kMaxStructuredSchemaChars = 64 * 1024;

OutputMode sanitizeOutputMode(int value);
StructuredSchemaPreset sanitizeStructuredSchemaPreset(int value);
std::wstring getStructuredSchemaName(StructuredSchemaPreset preset);
std::wstring getBuiltInStructuredSchema(StructuredSchemaPreset preset);
StructuredOutputConfig makeBuiltInStructuredOutputConfig(
    OutputMode outputMode, StructuredSchemaPreset preset, bool strict,
    bool validateResponse);

// The subset is intentionally closed: unsupported schema keywords cause a
// configuration error instead of pretending that the plugin enforced them.
StructuredOutputValidationResult
validateStructuredOutputSchema(const std::wstring &schemaText);
StructuredOutputValidationResult validateStructuredOutputContent(
    const std::wstring &content, const StructuredOutputConfig &config);

// Provider adapters choose the endpoint-specific wrapper. This pure builder is
// kept separate from HTTP so exact transport serialization is executable-testable.
StructuredOutputFormatResult buildStructuredOutputFormat(
    const StructuredOutputConfig &config, StructuredOutputTransport transport);

// These extract only documented response fields through the same bounded JSON
// parser used by the validator. They deliberately do not search arbitrary text
// for a matching key.
ProviderEnvelopeResult extractChatCompletionEnvelope(const std::wstring &body);
ProviderEnvelopeResult extractResponsesEnvelope(const std::wstring &body);
ProviderEnvelopeResult extractGeminiEnvelope(const std::wstring &body);
ProviderEnvelopeResult extractClaudeEnvelope(const std::wstring &body);

std::wstring responseFailureLabel(ResponseFailure failure);

// Validates before indenting, retaining number and string lexemes exactly.
bool prettyPrintJson(const std::wstring &input, std::wstring &output);
