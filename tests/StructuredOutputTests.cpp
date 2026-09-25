#include "StructuredOutput.h"

#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char *name) {
  if (!condition) {
    std::cerr << "FAILED: " << name << '\n';
    ++failures;
  }
}

void expectFailure(const StructuredOutputValidationResult &result,
                   ResponseFailure failure, const char *name) {
  expect(!result.success && result.failure == failure, name);
}

} // namespace

int main() {
  for (OutputMode mode : {OutputMode::Text, OutputMode::Markdown,
                          OutputMode::Json}) {
    const StructuredOutputConfig unchanged = makeBuiltInStructuredOutputConfig(
        mode, StructuredSchemaPreset::GenericStructuredResult, true, true);
    const StructuredOutputFormatResult format = buildStructuredOutputFormat(
        unchanged, StructuredOutputTransport::ChatCompletions);
    expect(!unchanged.enabled && format.success && format.jsonMember.empty(),
           "non-structured modes do not add schema transport");
  }
  const StructuredOutputConfig generic = makeBuiltInStructuredOutputConfig(
      OutputMode::StructuredJson,
      StructuredSchemaPreset::GenericStructuredResult, true, true);
  const StructuredOutputConfig review = makeBuiltInStructuredOutputConfig(
      OutputMode::StructuredJson, StructuredSchemaPreset::DocumentReview, true,
      true);

  expect(validateStructuredOutputSchema(generic.schema).success,
         "generic schema preflight");
  expect(validateStructuredOutputSchema(review.schema).success,
         "document review schema preflight");
  expect(validateStructuredOutputContent(L"{\"result\":\"ok\"}", generic).success,
         "generic valid result");
  expectFailure(validateStructuredOutputContent(L"{\"result\":2}", generic),
                ResponseFailure::SchemaValidationError, "wrong primitive type");
  expectFailure(validateStructuredOutputContent(L"{}", generic),
                ResponseFailure::SchemaValidationError, "missing required property");
  expectFailure(validateStructuredOutputContent(L"{\"result\":\"ok\",\"extra\":true}", generic),
                ResponseFailure::SchemaValidationError, "additional property rejected");
  expectFailure(validateStructuredOutputContent(L"{\"result\":", generic),
                ResponseFailure::JsonParseError, "malformed response JSON");
  StructuredOutputConfig parseOnly = generic;
  parseOnly.validateResponse = false;
  expect(validateStructuredOutputContent(L"{\"anything\":true}", parseOnly).success,
         "structured JSON parse-only mode");
  expectFailure(validateStructuredOutputContent(L"not json", parseOnly),
                ResponseFailure::JsonParseError,
                "structured JSON never accepts malformed JSON");
  expect(validateStructuredOutputContent(
             L"{\"summary\":\"One issue\",\"issues\":[{\"severity\":\"high\",\"description\":\"Missing bounds check\"}]}",
             review).success,
         "document review valid result");
  expectFailure(validateStructuredOutputContent(
                    L"{\"summary\":\"One issue\",\"issues\":[{\"severity\":\"critical\",\"description\":\"bad\"}]}",
                    review),
                ResponseFailure::SchemaValidationError, "enum mismatch");
  expectFailure(validateStructuredOutputSchema(
                    L"{\"type\":\"object\",\"patternProperties\":{}}"),
                ResponseFailure::SchemaValidationError, "unsupported keyword fails closed");
  expectFailure(validateStructuredOutputSchema(L"{\"type\":\"bogus\"}"),
                ResponseFailure::SchemaValidationError, "unsupported type fails closed");

  StructuredOutputConfig constraints;
  constraints.enabled = true;
  constraints.schemaName = L"constraints";
  constraints.schema =
      L"{\"type\":\"object\",\"properties\":{\"score\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":5},\"label\":{\"type\":\"string\",\"minLength\":2,\"maxLength\":4},\"kind\":{\"const\":\"fixed\"}},\"required\":[\"score\",\"label\",\"kind\"],\"additionalProperties\":false}";
  expect(validateStructuredOutputContent(
             L"{\"score\":3,\"label\":\"good\",\"kind\":\"fixed\"}", constraints)
             .success,
         "numeric string and const constraints");
  expectFailure(validateStructuredOutputContent(
                    L"{\"score\":6,\"label\":\"good\",\"kind\":\"fixed\"}", constraints),
                ResponseFailure::SchemaValidationError, "maximum constraint");
  expectFailure(validateStructuredOutputContent(
                    L"{\"score\":0,\"label\":\"good\",\"kind\":\"fixed\"}", constraints),
                ResponseFailure::SchemaValidationError, "minimum constraint");
  expectFailure(validateStructuredOutputContent(
                    L"{\"score\":3,\"label\":\"x\",\"kind\":\"fixed\"}", constraints),
                ResponseFailure::SchemaValidationError, "minLength constraint");
  expectFailure(validateStructuredOutputContent(
                    L"{\"score\":3,\"label\":\"oversized\",\"kind\":\"fixed\"}", constraints),
                ResponseFailure::SchemaValidationError, "maxLength constraint");
  expectFailure(validateStructuredOutputContent(
                    L"{\"score\":3,\"label\":\"good\",\"kind\":\"other\"}", constraints),
                ResponseFailure::SchemaValidationError, "const constraint");
  expectFailure(validateStructuredOutputContent(
                    L"{\"score\":1.00000000000000001,\"label\":\"good\",\"kind\":\"fixed\"}", constraints),
                ResponseFailure::SchemaValidationError,
                "fraction does not round into integer");
  StructuredOutputConfig exactNumeric = constraints;
  exactNumeric.schema =
      L"{\"type\":\"object\",\"properties\":{\"value\":{\"const\":9007199254740993}},\"required\":[\"value\"],\"additionalProperties\":false}";
  expect(validateStructuredOutputContent(L"{\"value\":9007199254740993}", exactNumeric).success,
         "large exact numeric const matches");
  expectFailure(validateStructuredOutputContent(L"{\"value\":9007199254740992}", exactNumeric),
                ResponseFailure::SchemaValidationError,
                "distinct large numeric const does not collide");
  expectFailure(validateStructuredOutputContent(L"\u00a0{\"result\":\"ok\"}", generic),
                ResponseFailure::JsonParseError,
                "non-JSON whitespace is rejected");
  expectFailure(validateStructuredOutputContent(
                    L"{\"result\":\"first\",\"result\":\"second\"}", generic),
                ResponseFailure::JsonParseError,
                "duplicate response property is rejected");
  std::wstring rawLoneSurrogate = L"{\"result\":\"";
  rawLoneSurrogate.push_back(static_cast<wchar_t>(0xD83D));
  rawLoneSurrogate += L"\"}";
  expectFailure(validateStructuredOutputContent(rawLoneSurrogate, generic),
                ResponseFailure::JsonParseError,
                "raw lone surrogate is rejected");
  expectFailure(validateStructuredOutputSchema(
                    L"{\"type\":\"string\",\"enum\":[]}"),
                ResponseFailure::SchemaValidationError,
                "empty enum schema is rejected");
  expectFailure(validateStructuredOutputSchema(
                    L"{\"type\":[\"string\",\"null\"]}"),
                ResponseFailure::SchemaValidationError,
                "type union is outside the supported subset");
  expectFailure(validateStructuredOutputSchema(L"{\"const\":1e1000001}"),
                ResponseFailure::SchemaValidationError,
                "unsupported const numeric magnitude fails preflight");
  StructuredOutputConfig arrayItems;
  arrayItems.enabled = true;
  arrayItems.schemaName = L"array_items";
  arrayItems.schema = L"{\"type\":\"array\",\"items\":{\"type\":\"integer\"}}";
  expectFailure(validateStructuredOutputContent(L"[1,2.5]", arrayItems),
                ResponseFailure::SchemaValidationError,
                "array item schema is enforced");
  StructuredOutputConfig additionalAllowed;
  additionalAllowed.enabled = true;
  additionalAllowed.schemaName = L"additional_allowed";
  additionalAllowed.schema = L"{\"type\":\"object\"}";
  expect(validateStructuredOutputContent(L"{\"extra\":true}", additionalAllowed).success,
         "additional properties default to allowed within the subset");
  StructuredOutputConfig unicodeLength;
  unicodeLength.enabled = true;
  unicodeLength.schemaName = L"unicode_length";
  unicodeLength.schema =
      L"{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\",\"maxLength\":1}},\"required\":[\"value\"],\"additionalProperties\":false}";
  expect(validateStructuredOutputContent(
             L"{\"value\":\"\\uD83D\\uDE00\"}", unicodeLength).success,
         "surrogate pair counts as one Unicode scalar");
  expectFailure(validateStructuredOutputContent(
                    L"{\"value\":\"\\uD83D\"}", unicodeLength),
                ResponseFailure::JsonParseError, "lone surrogate fails closed");

  const ProviderEnvelopeResult completeChat = extractChatCompletionEnvelope(
      L"{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"content\":\"{\\\"result\\\":\\\"ok\\\"}\"}}]}");
  expect(completeChat.success && completeChat.content == L"{\"result\":\"ok\"}",
         "chat envelope extraction");
  const ProviderEnvelopeResult missingChatStatus = extractChatCompletionEnvelope(
      L"{\"choices\":[{\"message\":{\"content\":\"{\\\"result\\\":\\\"ok\\\"}\"}}]}");
  expect(missingChatStatus.success && !missingChatStatus.completionStatusKnown,
         "legacy chat remains extractable but marks status unknown");
  expect(extractChatCompletionEnvelope(
             L"{\"choices\":[{\"finish_reason\":\"stop\",\"text\":\"legacy text\"}]}")
             .success,
         "legacy chat text fallback");
  expect(extractChatCompletionEnvelope(
             L"{\"choices\":[{\"finish_reason\":\"length\",\"message\":{\"content\":\"{\"}}]}")
             .failure == ResponseFailure::GenerationIncomplete,
         "chat length is incomplete");
  expect(extractChatCompletionEnvelope(
             L"{\"choices\":[{\"finish_reason\":\"length\",\"message\":{\"content\":\"\"}}]}")
             .failure == ResponseFailure::GenerationIncomplete,
         "chat length without partial text remains incomplete");
  const ProviderEnvelopeResult emptyChatStatus = extractChatCompletionEnvelope(
      L"{\"choices\":[{\"finish_reason\":\"\",\"message\":{\"content\":\"{}\"}}]}");
  expect(emptyChatStatus.success && !emptyChatStatus.completionStatusKnown,
         "empty chat finish reason is not known-complete");
  const ProviderEnvelopeResult partialChat = extractChatCompletionEnvelope(
      L"{\"choices\":[{\"finish_reason\":\"length\",\"message\":{\"content\":\"{\\\"result\\\":\"}}]}");
  expect(partialChat.failure == ResponseFailure::GenerationIncomplete &&
             partialChat.content == L"{\"result\":",
         "incomplete chat retains partial content");
  expect(extractChatCompletionEnvelope(
             L"{\"choices\":[{\"finish_reason\":\"content_filter\",\"message\":{\"content\":\"{\\\"result\\\":\\\"ok\\\"}\"}}]}")
             .failure == ResponseFailure::ProviderResponseError,
         "unsupported chat finish reason is blocked");
  expect(extractResponsesEnvelope(
             L"{\"status\":\"incomplete\",\"incomplete_details\":{\"reason\":\"max_output_tokens\"},\"output\":[]}")
             .failure == ResponseFailure::GenerationIncomplete,
         "responses incomplete is typed");
  expect(extractResponsesEnvelope(
             L"{\"status\":\"incomplete\",\"incomplete_details\":{\"reason\":\"max_output_tokens\"}}")
             .failure == ResponseFailure::GenerationIncomplete,
         "responses incomplete without output is typed");
  expect(extractResponsesEnvelope(
             L"{\"status\":\"completed\",\"output\":[{\"type\":\"message\",\"content\":[{\"type\":\"output_text\",\"text\":\"{\\\"result\\\":\\\"ok\\\"}\"}]}]}")
             .success,
         "responses envelope extraction");
  const ProviderEnvelopeResult missingResponsesStatus = extractResponsesEnvelope(
      L"{\"output\":[{\"type\":\"message\",\"content\":[{\"type\":\"output_text\",\"text\":\"{}\"}]}]}");
  expect(missingResponsesStatus.success &&
             !missingResponsesStatus.completionStatusKnown,
         "legacy responses output marks missing status unknown");
  expect(extractResponsesEnvelope(
             L"{\"status\":\"cancelled\",\"output\":[{\"type\":\"message\",\"content\":[{\"type\":\"output_text\",\"text\":\"partial\"}]}]}")
             .failure == ResponseFailure::GenerationIncomplete,
         "cancelled responses are incomplete");
  const ProviderEnvelopeResult refusal = extractResponsesEnvelope(
      L"{\"status\":\"completed\",\"output\":[{\"type\":\"message\",\"content\":[{\"type\":\"refusal\",\"refusal\":\"cannot comply\"}]}]}");
  expect(refusal.failure == ResponseFailure::ProviderResponseError &&
             refusal.content == L"cannot comply",
         "nested responses refusal is typed and retained");
  expect(extractGeminiEnvelope(
             L"{\"candidates\":[{\"finishReason\":\"STOP\",\"content\":{\"parts\":[{\"text\":\"gemini text\"}]}}]}")
             .success,
         "Gemini envelope extraction is scoped");
  expect(extractGeminiEnvelope(
             L"{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"text\"}]}}]}")
             .failure == ResponseFailure::ProviderResponseError,
         "Gemini missing finish reason fails closed");
  expect(extractGeminiEnvelope(
             L"{\"candidates\":[{\"finishReason\":\"SAFETY\",\"content\":{\"parts\":[{\"text\":\"partial\"}]}}]}")
             .failure == ResponseFailure::ProviderResponseError,
         "Gemini unsupported finish reason fails closed");
  const ProviderEnvelopeResult malformedGemini = extractGeminiEnvelope(
      L"{\"decoy\":\"\\\"text\\\":\\\"wrong\\\"\",\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"partial\"}]}}]");
  expect(!malformedGemini.success &&
             malformedGemini.failure == ResponseFailure::ProviderResponseError,
         "truncated Gemini envelope is rejected as a whole document");
  const ProviderEnvelopeResult claude = extractClaudeEnvelope(
      L"{\"stop_reason\":\"end_turn\",\"content\":[{\"type\":\"text\",\"text\":\"claude text\"}]}");
  expect(claude.success && claude.content == L"claude text",
         "Claude envelope extraction is scoped");
  expect(extractClaudeEnvelope(
             L"{\"content\":[{\"type\":\"text\",\"text\":\"text\"}]}")
             .failure == ResponseFailure::ProviderResponseError,
         "Claude missing stop reason fails closed");
  expect(extractClaudeEnvelope(
             L"{\"stop_reason\":\"tool_use\",\"content\":[{\"type\":\"text\",\"text\":\"partial\"}]}")
             .failure == ResponseFailure::ProviderResponseError,
         "Claude unsupported stop reason fails closed");
  expect(extractClaudeEnvelope(
             L"{\"stop_reason\":\"max_tokens\",\"content\":[{\"type\":\"text\",\"text\":\"partial\"}]}")
             .failure == ResponseFailure::GenerationIncomplete,
         "Claude token limit is incomplete");

  const StructuredOutputFormatResult chatFormat = buildStructuredOutputFormat(
      generic, StructuredOutputTransport::ChatCompletions);
  expect(chatFormat.success &&
             chatFormat.jsonMember.find(L"\"response_format\":{\"type\":\"json_schema\"") !=
                 std::wstring::npos &&
             chatFormat.jsonMember.find(L"\"strict\":true") != std::wstring::npos &&
             chatFormat.jsonMember.find(generic.schema) != std::wstring::npos,
         "chat structured transport shape and boolean strict");
  expect(chatFormat.jsonMember.find(generic.schema) ==
             chatFormat.jsonMember.rfind(generic.schema),
         "schema serialized exactly once");
  StructuredOutputConfig nonStrict = generic;
  nonStrict.strict = false;
  const StructuredOutputFormatResult responsesFormat = buildStructuredOutputFormat(
      nonStrict, StructuredOutputTransport::Responses);
  expect(responsesFormat.success &&
             responsesFormat.jsonMember.find(L"\"text\":{\"format\":{\"type\":\"json_schema\"") !=
                 std::wstring::npos &&
             responsesFormat.jsonMember.find(L"\"strict\":false") != std::wstring::npos &&
             responsesFormat.jsonMember.find(L"response_format") == std::wstring::npos,
         "responses transport uses distinct text format wrapper");
  StructuredOutputConfig unsafeName = generic;
  unsafeName.schemaName = L"bad\\r\\nAuthorization";
  expect(!buildStructuredOutputFormat(
              unsafeName, StructuredOutputTransport::ChatCompletions).success,
         "schema name cannot inject request structure or headers");
  StructuredOutputConfig malformedSchema = generic;
  malformedSchema.schema = L"{\"type\":";
  expect(!buildStructuredOutputFormat(
              malformedSchema, StructuredOutputTransport::ChatCompletions).success,
         "malformed schema blocks transport serialization");

  if (failures == 0) {
    std::cout << "StructuredOutputTests: PASS\n";
    return 0;
  }
  std::cerr << "StructuredOutputTests: " << failures << " failure(s)\n";
  return 1;
}
