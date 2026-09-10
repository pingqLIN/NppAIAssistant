// This file is part of Notepad++ project
// Copyright (C)2025 Don HO <don.h@free.fr>

#include "StructuredOutput.h"

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <limits>
#include <set>
#include <utility>
#include <vector>

namespace {

enum class JsonType { Null, Boolean, Number, String, Array, Object };

struct JsonValue {
  JsonType type = JsonType::Null;
  bool booleanValue = false;
  std::wstring scalar;
  std::vector<JsonValue> array;
  std::vector<std::pair<std::wstring, JsonValue>> object;
};

struct ParseResult {
  bool success = false;
  JsonValue value;
  std::wstring error;
};

bool isJsonWhitespace(wchar_t ch) {
  return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
}

bool isAsciiDigit(wchar_t ch) { return ch >= L'0' && ch <= L'9'; }

class JsonParser {
public:
  explicit JsonParser(const std::wstring &input) : input_(input) {}

  ParseResult parse() {
    skipWhitespace();
    JsonValue value;
    if (!parseValue(value, 0)) return {false, {}, error_};
    skipWhitespace();
    if (position_ != input_.size()) {
      return {false, {}, L"Unexpected content after JSON value."};
    }
    return {true, std::move(value), L""};
  }

private:
  static constexpr size_t kMaxDepth = 64;
  static constexpr size_t kMaxNodes = 100000;

  void skipWhitespace() {
    while (position_ < input_.size() && isJsonWhitespace(input_[position_])) {
      ++position_;
    }
  }

  bool fail(const wchar_t *message) {
    error_ = message;
    return false;
  }

  bool parseValue(JsonValue &value, size_t depth) {
    if (depth > kMaxDepth) return fail(L"JSON nesting is too deep.");
    if (++nodeCount_ > kMaxNodes) return fail(L"JSON contains too many values.");
    skipWhitespace();
    if (position_ >= input_.size()) return fail(L"Unexpected end of JSON.");
    switch (input_[position_]) {
    case L'{': return parseObject(value, depth + 1);
    case L'[': return parseArray(value, depth + 1);
    case L'"':
      value.type = JsonType::String;
      return parseString(value.scalar);
    case L't': return parseLiteral(L"true", JsonType::Boolean, value, true);
    case L'f': return parseLiteral(L"false", JsonType::Boolean, value, false);
    case L'n': return parseLiteral(L"null", JsonType::Null, value, false);
    default:
      if (input_[position_] == L'-' || isAsciiDigit(input_[position_])) {
        value.type = JsonType::Number;
        return parseNumber(value.scalar);
      }
      return fail(L"Expected a JSON value.");
    }
  }

  bool parseLiteral(const wchar_t *literal, JsonType type, JsonValue &value,
                    bool booleanValue) {
    const size_t length = wcslen(literal);
    if (input_.compare(position_, length, literal) != 0) {
      return fail(L"Invalid JSON literal.");
    }
    position_ += length;
    value.type = type;
    value.booleanValue = booleanValue;
    return true;
  }

  bool parseObject(JsonValue &value, size_t depth) {
    ++position_;
    value.type = JsonType::Object;
    skipWhitespace();
    if (position_ < input_.size() && input_[position_] == L'}') {
      ++position_;
      return true;
    }
    std::set<std::wstring> keys;
    while (true) {
      skipWhitespace();
      std::wstring key;
      if (!parseString(key)) return false;
      if (!keys.insert(key).second) return fail(L"Duplicate JSON object key.");
      skipWhitespace();
      if (position_ >= input_.size() || input_[position_++] != L':') {
        return fail(L"Expected ':' after JSON object key.");
      }
      JsonValue child;
      if (!parseValue(child, depth)) return false;
      value.object.emplace_back(std::move(key), std::move(child));
      skipWhitespace();
      if (position_ >= input_.size()) return fail(L"Unterminated JSON object.");
      if (input_[position_] == L'}') {
        ++position_;
        return true;
      }
      if (input_[position_++] != L',') return fail(L"Expected ',' in JSON object.");
    }
  }

  bool parseArray(JsonValue &value, size_t depth) {
    ++position_;
    value.type = JsonType::Array;
    skipWhitespace();
    if (position_ < input_.size() && input_[position_] == L']') {
      ++position_;
      return true;
    }
    while (true) {
      JsonValue child;
      if (!parseValue(child, depth)) return false;
      value.array.push_back(std::move(child));
      skipWhitespace();
      if (position_ >= input_.size()) return fail(L"Unterminated JSON array.");
      if (input_[position_] == L']') {
        ++position_;
        return true;
      }
      if (input_[position_++] != L',') return fail(L"Expected ',' in JSON array.");
    }
  }

  bool parseString(std::wstring &value) {
    if (position_ >= input_.size() || input_[position_++] != L'"') {
      return fail(L"Expected JSON string.");
    }
    value.clear();
    while (position_ < input_.size()) {
      const wchar_t ch = input_[position_++];
      if (ch == L'"') return true;
      if (ch < 0x20) return fail(L"JSON string contains a control character.");
      if (ch != L'\\') {
        if (ch >= 0xD800 && ch <= 0xDBFF) {
          if (position_ >= input_.size() || input_[position_] < 0xDC00 ||
              input_[position_] > 0xDFFF) {
            return fail(L"High surrogate is not followed by a low surrogate.");
          }
          value.push_back(ch);
          value.push_back(input_[position_++]);
          continue;
        }
        if (ch >= 0xDC00 && ch <= 0xDFFF) {
          return fail(L"Low surrogate has no preceding high surrogate.");
        }
        value.push_back(ch);
        continue;
      }
      if (position_ >= input_.size()) return fail(L"Invalid JSON string escape.");
      switch (input_[position_++]) {
      case L'"': value.push_back(L'"'); break;
      case L'\\': value.push_back(L'\\'); break;
      case L'/': value.push_back(L'/'); break;
      case L'b': value.push_back(L'\b'); break;
      case L'f': value.push_back(L'\f'); break;
      case L'n': value.push_back(L'\n'); break;
      case L'r': value.push_back(L'\r'); break;
      case L't': value.push_back(L'\t'); break;
      case L'u': {
        if (position_ + 4 > input_.size()) return fail(L"Invalid Unicode escape.");
        unsigned int codeUnit = 0;
        for (size_t i = 0; i < 4; ++i) {
          const wchar_t digit = input_[position_++];
          const int valueDigit = digit >= L'0' && digit <= L'9'
                                     ? digit - L'0'
                                     : digit >= L'a' && digit <= L'f'
                                           ? digit - L'a' + 10
                                           : digit >= L'A' && digit <= L'F'
                                                 ? digit - L'A' + 10
                                                 : -1;
          if (valueDigit < 0) return fail(L"Invalid Unicode escape.");
          codeUnit = (codeUnit << 4) | static_cast<unsigned int>(valueDigit);
        }
        if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF) {
          if (position_ + 6 > input_.size() || input_[position_] != L'\\' ||
              input_[position_ + 1] != L'u') {
            return fail(L"High surrogate is not followed by a low surrogate.");
          }
          position_ += 2;
          unsigned int lowSurrogate = 0;
          for (size_t i = 0; i < 4; ++i) {
            const wchar_t digit = input_[position_++];
            const int valueDigit = digit >= L'0' && digit <= L'9'
                                       ? digit - L'0'
                                       : digit >= L'a' && digit <= L'f'
                                             ? digit - L'a' + 10
                                             : digit >= L'A' && digit <= L'F'
                                                   ? digit - L'A' + 10
                                                   : -1;
            if (valueDigit < 0) return fail(L"Invalid low surrogate escape.");
            lowSurrogate = (lowSurrogate << 4) | static_cast<unsigned int>(valueDigit);
          }
          if (lowSurrogate < 0xDC00 || lowSurrogate > 0xDFFF) {
            return fail(L"High surrogate is not followed by a low surrogate.");
          }
          value.push_back(static_cast<wchar_t>(codeUnit));
          value.push_back(static_cast<wchar_t>(lowSurrogate));
          break;
        }
        if (codeUnit >= 0xDC00 && codeUnit <= 0xDFFF) {
          return fail(L"Low surrogate has no preceding high surrogate.");
        }
        value.push_back(static_cast<wchar_t>(codeUnit));
        break;
      }
      default: return fail(L"Invalid JSON string escape.");
      }
    }
    return fail(L"Unterminated JSON string.");
  }

  bool parseNumber(std::wstring &value) {
    const size_t start = position_;
    if (input_[position_] == L'-') ++position_;
    if (position_ >= input_.size()) return fail(L"Invalid JSON number.");
    if (input_[position_] == L'0') {
      ++position_;
    } else if (input_[position_] >= L'1' && input_[position_] <= L'9') {
      while (position_ < input_.size() && isAsciiDigit(input_[position_])) ++position_;
    } else {
      return fail(L"Invalid JSON number.");
    }
    if (position_ < input_.size() && input_[position_] == L'.') {
      ++position_;
      const size_t fractionStart = position_;
      while (position_ < input_.size() && isAsciiDigit(input_[position_])) ++position_;
      if (fractionStart == position_) return fail(L"Invalid JSON number.");
    }
    if (position_ < input_.size() && (input_[position_] == L'e' || input_[position_] == L'E')) {
      ++position_;
      if (position_ < input_.size() && (input_[position_] == L'+' || input_[position_] == L'-')) ++position_;
      const size_t exponentStart = position_;
      while (position_ < input_.size() && isAsciiDigit(input_[position_])) ++position_;
      if (exponentStart == position_) return fail(L"Invalid JSON number.");
    }
    value = input_.substr(start, position_ - start);
    return true;
  }

  const std::wstring &input_;
  size_t position_ = 0;
  std::wstring error_;
  size_t nodeCount_ = 0;
};

const JsonValue *findProperty(const JsonValue &value, const std::wstring &key) {
  for (const auto &entry : value.object) {
    if (entry.first == key) return &entry.second;
  }
  return nullptr;
}

struct ExactNumber {
  bool negative = false;
  std::wstring digits = L"0";
  // Value is digits * 10^scale. Leading and trailing zeroes are removed.
  std::int64_t scale = 0;
};

bool readExactNumber(const JsonValue &value, ExactNumber &number) {
  if (value.type != JsonType::Number || value.scalar.empty()) return false;
  constexpr std::int64_t kMaxAbsExponent = 1000000;
  const std::wstring &text = value.scalar;
  size_t position = 0;
  number = {};
  if (text[position] == L'-') {
    number.negative = true;
    ++position;
  }
  const size_t exponentMarker = text.find_first_of(L"eE", position);
  const size_t mantissaEnd = exponentMarker == std::wstring::npos
                                 ? text.size()
                                 : exponentMarker;
  const size_t decimalPoint = text.find(L'.', position);
  const size_t fractionDigits =
      decimalPoint != std::wstring::npos && decimalPoint < mantissaEnd
          ? mantissaEnd - decimalPoint - 1
          : 0;
  number.digits.clear();
  for (size_t index = position; index < mantissaEnd; ++index) {
    if (text[index] != L'.') number.digits.push_back(text[index]);
  }

  std::int64_t exponent = 0;
  if (exponentMarker != std::wstring::npos) {
    size_t exponentPosition = exponentMarker + 1;
    bool exponentNegative = false;
    if (text[exponentPosition] == L'+' || text[exponentPosition] == L'-') {
      exponentNegative = text[exponentPosition] == L'-';
      ++exponentPosition;
    }
    for (; exponentPosition < text.size(); ++exponentPosition) {
      const int digit = text[exponentPosition] - L'0';
      if (exponent > (kMaxAbsExponent - digit) / 10) return false;
      exponent = exponent * 10 + digit;
    }
    if (exponentNegative) exponent = -exponent;
  }
  if (fractionDigits > static_cast<size_t>(std::numeric_limits<std::int64_t>::max())) {
    return false;
  }
  number.scale = exponent - static_cast<std::int64_t>(fractionDigits);

  const size_t firstNonZero = number.digits.find_first_not_of(L'0');
  if (firstNonZero == std::wstring::npos) {
    number = {};
    return true;
  }
  number.digits.erase(0, firstNonZero);
  while (number.digits.size() > 1 && number.digits.back() == L'0') {
    number.digits.pop_back();
    ++number.scale;
  }
  return true;
}

bool exactNumberIsInteger(const ExactNumber &number) {
  return number.digits == L"0" || number.scale >= 0;
}

int compareExactMagnitude(const ExactNumber &left, const ExactNumber &right) {
  const std::int64_t leftPosition =
      static_cast<std::int64_t>(left.digits.size()) + left.scale;
  const std::int64_t rightPosition =
      static_cast<std::int64_t>(right.digits.size()) + right.scale;
  if (leftPosition != rightPosition) return leftPosition < rightPosition ? -1 : 1;
  const size_t width = std::max(left.digits.size(), right.digits.size());
  for (size_t index = 0; index < width; ++index) {
    const wchar_t leftDigit =
        index < left.digits.size() ? left.digits[index] : L'0';
    const wchar_t rightDigit =
        index < right.digits.size() ? right.digits[index] : L'0';
    if (leftDigit != rightDigit) return leftDigit < rightDigit ? -1 : 1;
  }
  return 0;
}

int compareExactNumber(const ExactNumber &left, const ExactNumber &right) {
  const bool leftZero = left.digits == L"0";
  const bool rightZero = right.digits == L"0";
  if (leftZero && rightZero) return 0;
  if (left.negative != right.negative) return left.negative ? -1 : 1;
  const int magnitude = compareExactMagnitude(left, right);
  return left.negative ? -magnitude : magnitude;
}

bool unicodeScalarLength(const std::wstring &value, size_t &length) {
  length = 0;
  for (size_t index = 0; index < value.size(); ++index) {
    const wchar_t ch = value[index];
    if (ch >= 0xD800 && ch <= 0xDBFF) {
      if (index + 1 >= value.size() || value[index + 1] < 0xDC00 ||
          value[index + 1] > 0xDFFF) return false;
      ++index;
    } else if (ch >= 0xDC00 && ch <= 0xDFFF) {
      return false;
    }
    ++length;
  }
  return true;
}

bool readNonNegativeInteger(const JsonValue &value, size_t &number) {
  ExactNumber parsed;
  if (!readExactNumber(value, parsed) || parsed.negative ||
      !exactNumberIsInteger(parsed)) return false;
  if (parsed.digits == L"0") {
    number = 0;
    return true;
  }
  if (parsed.scale > 0 &&
      static_cast<std::uint64_t>(parsed.scale) >
          std::numeric_limits<size_t>::max() - parsed.digits.size()) return false;
  const size_t totalDigits = parsed.digits.size() + static_cast<size_t>(parsed.scale);
  if (totalDigits > std::numeric_limits<size_t>::digits10 + 1) return false;
  size_t result = 0;
  for (size_t index = 0; index < totalDigits; ++index) {
    const size_t digit = index < parsed.digits.size()
                             ? static_cast<size_t>(parsed.digits[index] - L'0')
                             : 0;
    if (result > (std::numeric_limits<size_t>::max() - digit) / 10) return false;
    result = result * 10 + digit;
  }
  number = result;
  return true;
}

bool jsonEqual(const JsonValue &left, const JsonValue &right) {
  if (left.type != right.type) return false;
  switch (left.type) {
  case JsonType::Null: return true;
  case JsonType::Boolean: return left.booleanValue == right.booleanValue;
  case JsonType::Number: {
    ExactNumber a, b;
    return readExactNumber(left, a) && readExactNumber(right, b) &&
           compareExactNumber(a, b) == 0;
  }
  case JsonType::String: return left.scalar == right.scalar;
  case JsonType::Array:
    if (left.array.size() != right.array.size()) return false;
    for (size_t i = 0; i < left.array.size(); ++i) {
      if (!jsonEqual(left.array[i], right.array[i])) return false;
    }
    return true;
  case JsonType::Object:
    if (left.object.size() != right.object.size()) return false;
    for (const auto &entry : left.object) {
      const JsonValue *other = findProperty(right, entry.first);
      if (!other || !jsonEqual(entry.second, *other)) return false;
    }
    return true;
  }
  return false;
}

bool typeMatches(const JsonValue &value, const std::wstring &type) {
  return (type == L"null" && value.type == JsonType::Null) ||
         (type == L"boolean" && value.type == JsonType::Boolean) ||
         (type == L"number" && value.type == JsonType::Number &&
          [&]() { ExactNumber number; return readExactNumber(value, number); }()) ||
         (type == L"integer" && value.type == JsonType::Number && [&]() {
           ExactNumber number;
           return readExactNumber(value, number) && exactNumberIsInteger(number);
         }()) ||
         (type == L"string" && value.type == JsonType::String) ||
         (type == L"array" && value.type == JsonType::Array) ||
         (type == L"object" && value.type == JsonType::Object);
}

bool supportedTypeName(const std::wstring &type) {
  return type == L"null" || type == L"boolean" || type == L"number" ||
         type == L"integer" || type == L"string" || type == L"array" ||
         type == L"object";
}

bool containsOnlySupportedNumbers(const JsonValue &value) {
  if (value.type == JsonType::Number) {
    ExactNumber number;
    return readExactNumber(value, number);
  }
  if (value.type == JsonType::Array) {
    return std::all_of(value.array.begin(), value.array.end(),
                       containsOnlySupportedNumbers);
  }
  if (value.type == JsonType::Object) {
    return std::all_of(value.object.begin(), value.object.end(),
                       [](const auto &entry) {
                         return containsOnlySupportedNumbers(entry.second);
                       });
  }
  return true;
}

bool validateSchemaNode(const JsonValue &schema, std::wstring &error, size_t depth) {
  if (depth > 32) { error = L"Schema nesting is too deep."; return false; }
  if (schema.type != JsonType::Object) { error = L"A schema must be a JSON object."; return false; }
  static const std::set<std::wstring> allowed = {
      L"type", L"properties", L"required", L"additionalProperties", L"items",
      L"enum", L"const", L"minimum", L"maximum", L"minLength", L"maxLength"};
  for (const auto &entry : schema.object) {
    if (!allowed.contains(entry.first)) {
      error = L"Unsupported schema keyword: " + entry.first;
      return false;
    }
  }
  if (const JsonValue *type = findProperty(schema, L"type")) {
    if (type->type != JsonType::String || !supportedTypeName(type->scalar)) {
      error = L"Schema type must be one supported JSON type.";
      return false;
    }
  }
  if (const JsonValue *properties = findProperty(schema, L"properties")) {
    if (properties->type != JsonType::Object) { error = L"properties must be an object."; return false; }
    for (const auto &property : properties->object) {
      if (!validateSchemaNode(property.second, error, depth + 1)) return false;
    }
  }
  if (const JsonValue *required = findProperty(schema, L"required")) {
    if (required->type != JsonType::Array) { error = L"required must be an array."; return false; }
    std::set<std::wstring> names;
    for (const JsonValue &name : required->array) {
      if (name.type != JsonType::String || !names.insert(name.scalar).second) {
        error = L"required must contain unique string property names.";
        return false;
      }
    }
  }
  if (const JsonValue *additional = findProperty(schema, L"additionalProperties")) {
    if (additional->type != JsonType::Boolean) { error = L"additionalProperties must be boolean."; return false; }
  }
  if (const JsonValue *items = findProperty(schema, L"items")) {
    if (!validateSchemaNode(*items, error, depth + 1)) return false;
  }
  if (const JsonValue *enumeration = findProperty(schema, L"enum")) {
    if (enumeration->type != JsonType::Array || enumeration->array.empty()) {
      error = L"enum must be a non-empty array.";
      return false;
    }
    if (!containsOnlySupportedNumbers(*enumeration)) {
      error = L"enum contains an unsupported numeric magnitude.";
      return false;
    }
  }
  if (const JsonValue *constant = findProperty(schema, L"const")) {
    if (!containsOnlySupportedNumbers(*constant)) {
      error = L"const contains an unsupported numeric magnitude.";
      return false;
    }
  }
  for (const wchar_t *keyword : {L"minimum", L"maximum"}) {
    if (const JsonValue *bound = findProperty(schema, keyword)) {
      ExactNumber ignored;
      if (!readExactNumber(*bound, ignored)) { error = std::wstring(keyword) + L" must be a supported exact decimal number."; return false; }
    }
  }
  for (const wchar_t *keyword : {L"minLength", L"maxLength"}) {
    if (const JsonValue *length = findProperty(schema, keyword)) {
      size_t ignored = 0;
      if (!readNonNegativeInteger(*length, ignored)) { error = std::wstring(keyword) + L" must be a non-negative integer."; return false; }
    }
  }
  return true;
}

bool validateValue(const JsonValue &value, const JsonValue &schema,
                   std::wstring &error, const std::wstring &path, size_t depth) {
  if (depth > 32) { error = path + L": validation nesting is too deep."; return false; }
  if (const JsonValue *type = findProperty(schema, L"type")) {
    if (!typeMatches(value, type->scalar)) { error = path + L": type does not match schema."; return false; }
  }
  if (const JsonValue *enumeration = findProperty(schema, L"enum")) {
    bool matched = false;
    for (const JsonValue &candidate : enumeration->array) matched = matched || jsonEqual(value, candidate);
    if (!matched) { error = path + L": value is not in enum."; return false; }
  }
  if (const JsonValue *constant = findProperty(schema, L"const")) {
    if (!jsonEqual(value, *constant)) { error = path + L": value does not match const."; return false; }
  }
  if (value.type == JsonType::Number) {
    ExactNumber number;
    if (!readExactNumber(value, number)) { error = path + L": unsupported numeric magnitude."; return false; }
    if (const JsonValue *minimum = findProperty(schema, L"minimum")) {
      ExactNumber bound; readExactNumber(*minimum, bound);
      if (compareExactNumber(number, bound) < 0) { error = path + L": below minimum."; return false; }
    }
    if (const JsonValue *maximum = findProperty(schema, L"maximum")) {
      ExactNumber bound; readExactNumber(*maximum, bound);
      if (compareExactNumber(number, bound) > 0) { error = path + L": above maximum."; return false; }
    }
  }
  if (value.type == JsonType::String) {
    size_t scalarLength = 0;
    if (!unicodeScalarLength(value.scalar, scalarLength)) {
      error = path + L": invalid Unicode surrogate sequence.";
      return false;
    }
    if (const JsonValue *minimum = findProperty(schema, L"minLength")) {
      size_t length = 0; readNonNegativeInteger(*minimum, length);
      if (scalarLength < length) { error = path + L": shorter than minLength."; return false; }
    }
    if (const JsonValue *maximum = findProperty(schema, L"maxLength")) {
      size_t length = 0; readNonNegativeInteger(*maximum, length);
      if (scalarLength > length) { error = path + L": longer than maxLength."; return false; }
    }
  }
  if (value.type == JsonType::Array) {
    if (const JsonValue *items = findProperty(schema, L"items")) {
      for (size_t index = 0; index < value.array.size(); ++index) {
        if (!validateValue(value.array[index], *items, error,
                           path + L"[" + std::to_wstring(index) + L"]", depth + 1)) return false;
      }
    }
  }
  if (value.type == JsonType::Object) {
    const JsonValue *properties = findProperty(schema, L"properties");
    if (const JsonValue *required = findProperty(schema, L"required")) {
      for (const JsonValue &name : required->array) {
        if (!findProperty(value, name.scalar)) { error = path + L": missing required property '" + name.scalar + L"'."; return false; }
      }
    }
    const JsonValue *additional = findProperty(schema, L"additionalProperties");
    for (const auto &entry : value.object) {
      const JsonValue *propertySchema = properties ? findProperty(*properties, entry.first) : nullptr;
      if (!propertySchema) {
        if (additional && !additional->booleanValue) { error = path + L": unexpected property '" + entry.first + L"'."; return false; }
        continue;
      }
      if (!validateValue(entry.second, *propertySchema, error,
                         path + L"." + entry.first, depth + 1)) return false;
    }
  }
  return true;
}

StructuredOutputValidationResult parseSchema(const std::wstring &schemaText,
                                             JsonValue &schema) {
  if (schemaText.empty()) return {false, ResponseFailure::SchemaValidationError, L"Structured schema is empty."};
  if (schemaText.size() > kMaxStructuredSchemaChars) return {false, ResponseFailure::SchemaValidationError, L"Structured schema exceeds the size limit."};
  ParseResult parsed = JsonParser(schemaText).parse();
  if (!parsed.success) return {false, ResponseFailure::SchemaValidationError, L"Invalid structured schema JSON: " + parsed.error};
  std::wstring error;
  if (!validateSchemaNode(parsed.value, error, 0)) return {false, ResponseFailure::SchemaValidationError, error};
  schema = std::move(parsed.value);
  return {true, ResponseFailure::None, L""};
}

std::wstring stringProperty(const JsonValue &object, const wchar_t *name) {
  const JsonValue *value = findProperty(object, name);
  return value && value->type == JsonType::String ? value->scalar : L"";
}

ProviderEnvelopeResult malformedEnvelope(const std::wstring &prefix,
                                         const ParseResult &parsed) {
  return {false, ResponseFailure::ProviderResponseError, L"",
          prefix + parsed.error};
}

} // namespace

OutputMode sanitizeOutputMode(int value) {
  return value >= static_cast<int>(OutputMode::Text) && value <= static_cast<int>(OutputMode::StructuredJson)
             ? static_cast<OutputMode>(value) : OutputMode::Text;
}

StructuredSchemaPreset sanitizeStructuredSchemaPreset(int value) {
  return value >= static_cast<int>(StructuredSchemaPreset::GenericStructuredResult) &&
                 value <= static_cast<int>(StructuredSchemaPreset::DocumentReview)
             ? static_cast<StructuredSchemaPreset>(value)
             : StructuredSchemaPreset::GenericStructuredResult;
}

std::wstring getStructuredSchemaName(StructuredSchemaPreset preset) {
  return preset == StructuredSchemaPreset::DocumentReview ? L"document_review" : L"structured_result";
}

std::wstring getBuiltInStructuredSchema(StructuredSchemaPreset preset) {
  if (preset == StructuredSchemaPreset::DocumentReview) {
    return LR"({"type":"object","properties":{"summary":{"type":"string"},"issues":{"type":"array","items":{"type":"object","properties":{"severity":{"type":"string","enum":["low","medium","high"]},"description":{"type":"string"}},"required":["severity","description"],"additionalProperties":false}}},"required":["summary","issues"],"additionalProperties":false})";
  }
  return LR"({"type":"object","properties":{"result":{"type":"string"}},"required":["result"],"additionalProperties":false})";
}

StructuredOutputConfig makeBuiltInStructuredOutputConfig(
    OutputMode outputMode, StructuredSchemaPreset preset, bool strict,
    bool validateResponse) {
  preset = sanitizeStructuredSchemaPreset(static_cast<int>(preset));
  return {outputMode == OutputMode::StructuredJson, getStructuredSchemaName(preset),
          getBuiltInStructuredSchema(preset), strict, validateResponse};
}

StructuredOutputValidationResult validateStructuredOutputSchema(const std::wstring &schemaText) {
  JsonValue schema;
  return parseSchema(schemaText, schema);
}

StructuredOutputValidationResult validateStructuredOutputContent(
    const std::wstring &content, const StructuredOutputConfig &config) {
  if (!config.enabled) return {true, ResponseFailure::None, L""};
  // Structured JSON always requires syntactically valid JSON. The setting only
  // controls the additional local schema-validation step.
  ParseResult parsed = JsonParser(content).parse();
  if (!parsed.success) return {false, ResponseFailure::JsonParseError, L"Invalid JSON response: " + parsed.error};
  if (!config.validateResponse) return {true, ResponseFailure::None, L""};
  JsonValue schema;
  StructuredOutputValidationResult schemaResult = parseSchema(config.schema, schema);
  if (!schemaResult.success) return schemaResult;
  std::wstring error;
  if (!validateValue(parsed.value, schema, error, L"$", 0)) {
    return {false, ResponseFailure::SchemaValidationError, error};
  }
  return {true, ResponseFailure::None, L""};
}

StructuredOutputFormatResult buildStructuredOutputFormat(
    const StructuredOutputConfig &config, StructuredOutputTransport transport) {
  if (!config.enabled) return {true, ResponseFailure::None, L"", L""};
  const bool safeName = !config.schemaName.empty() && config.schemaName.size() <= 64 &&
      std::all_of(config.schemaName.begin(), config.schemaName.end(), [](wchar_t ch) {
        return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
               isAsciiDigit(ch) || ch == L'_' || ch == L'-';
      });
  if (!safeName) {
    return {false, ResponseFailure::SchemaValidationError, L"",
            L"Structured schema name must contain only letters, digits, '_' or '-'."};
  }
  const StructuredOutputValidationResult validation =
      validateStructuredOutputSchema(config.schema);
  if (!validation.success) {
    return {false, validation.failure, L"",
            L"Structured schema is invalid: " + validation.errorMessage};
  }
  const std::wstring strict = config.strict ? L"true" : L"false";
  const std::wstring escapedName = config.schemaName;
  if (transport == StructuredOutputTransport::Responses) {
    return {true, ResponseFailure::None,
            L",\"text\":{\"format\":{\"type\":\"json_schema\",\"name\":\"" +
                escapedName + L"\",\"strict\":" + strict + L",\"schema\":" +
                config.schema + L"}}",
            L""};
  }
  return {true, ResponseFailure::None,
          L",\"response_format\":{\"type\":\"json_schema\",\"json_schema\":{\"name\":\"" +
              escapedName + L"\",\"strict\":" + strict + L",\"schema\":" +
              config.schema + L"}}",
          L""};
}

ProviderEnvelopeResult extractChatCompletionEnvelope(const std::wstring &body) {
  const ParseResult parsed = JsonParser(body).parse();
  if (!parsed.success || parsed.value.type != JsonType::Object) {
    return malformedEnvelope(L"Invalid Chat Completions response: ", parsed);
  }
  const JsonValue *choices = findProperty(parsed.value, L"choices");
  if (!choices || choices->type != JsonType::Array || choices->array.empty() ||
      choices->array.front().type != JsonType::Object) {
    return {false, ResponseFailure::ProviderResponseError, L"",
            L"Chat Completions response has no choice."};
  }
  const JsonValue &choice = choices->array.front();
  const JsonValue *finishReasonValue = findProperty(choice, L"finish_reason");
  if (finishReasonValue && finishReasonValue->type != JsonType::String) {
    return {false, ResponseFailure::ProviderResponseError, L"",
            L"Chat Completions finish_reason is not a string."};
  }
  const std::wstring finishReason =
      finishReasonValue ? finishReasonValue->scalar : L"";
  const bool completionStatusKnown =
      finishReasonValue != nullptr && !finishReason.empty();
  const bool incomplete = finishReason == L"length";
  if (!finishReason.empty() && finishReason != L"stop" && !incomplete) {
    return {false, ResponseFailure::ProviderResponseError, L"",
            L"Chat Completions stopped with unsupported finish reason: " +
                finishReason};
  }
  const JsonValue *message = findProperty(choice, L"message");
  if (!message || message->type != JsonType::Object) {
    // Some older OpenAI-compatible servers return choices[0].text. Keep this
    // bounded compatibility path after validating the enclosing JSON document.
    const std::wstring legacyText = stringProperty(choice, L"text");
    if (!legacyText.empty()) {
      return incomplete
                 ? ProviderEnvelopeResult{false, ResponseFailure::GenerationIncomplete,
                                          legacyText, L"Generation stopped because the output token limit was reached."}
                 : ProviderEnvelopeResult{true, ResponseFailure::None, legacyText, L"",
                                          completionStatusKnown};
    }
    if (incomplete) return {false, ResponseFailure::GenerationIncomplete, L"",
                            L"Generation stopped because the output token limit was reached."};
    return {false, ResponseFailure::ProviderResponseError, L"",
            L"Chat Completions response has no assistant message."};
  }
  const std::wstring refusal = stringProperty(*message, L"refusal");
  if (!refusal.empty()) {
    return {false, ResponseFailure::ProviderResponseError, refusal,
            L"The provider refused the structured response."};
  }
  const JsonValue *content = findProperty(*message, L"content");
  if (!content || content->type != JsonType::String || content->scalar.empty()) {
    if (incomplete) {
      return {false, ResponseFailure::GenerationIncomplete, L"",
              L"Generation stopped because the output token limit was reached."};
    }
    return {false, ResponseFailure::EmptyResponse, L"",
            L"Chat Completions response has no assistant content."};
  }
  if (incomplete) {
    return {false, ResponseFailure::GenerationIncomplete, content->scalar,
            L"Generation stopped because the output token limit was reached."};
  }
  return {true, ResponseFailure::None, content->scalar, L"",
          completionStatusKnown};
}

ProviderEnvelopeResult extractGeminiEnvelope(const std::wstring &body) {
  const ParseResult parsed = JsonParser(body).parse();
  if (!parsed.success || parsed.value.type != JsonType::Object) {
    return malformedEnvelope(L"Invalid Gemini response: ", parsed);
  }
  const JsonValue *candidates = findProperty(parsed.value, L"candidates");
  if (!candidates || candidates->type != JsonType::Array ||
      candidates->array.empty() ||
      candidates->array.front().type != JsonType::Object) {
    return {false, ResponseFailure::ProviderResponseError, L"",
            L"Gemini response has no candidate."};
  }
  const JsonValue &candidate = candidates->array.front();
  const JsonValue *finishReasonValue = findProperty(candidate, L"finishReason");
  if (!finishReasonValue || finishReasonValue->type != JsonType::String ||
      finishReasonValue->scalar.empty()) {
    return {false, ResponseFailure::ProviderResponseError, L"",
            L"Gemini response has no valid finishReason."};
  }
  const bool incomplete = finishReasonValue->scalar == L"MAX_TOKENS";
  const bool complete = finishReasonValue->scalar == L"STOP";
  const JsonValue *content = findProperty(candidate, L"content");
  const JsonValue *parts =
      content && content->type == JsonType::Object
          ? findProperty(*content, L"parts")
          : nullptr;
  if (!parts || parts->type != JsonType::Array || parts->array.empty() ||
      parts->array.front().type != JsonType::Object) {
    return {false,
            incomplete ? ResponseFailure::GenerationIncomplete
                       : ResponseFailure::ProviderResponseError,
            L"", incomplete ? L"Gemini generation reached its token limit."
                              : L"Gemini response has no text part."};
  }
  const std::wstring text = stringProperty(parts->array.front(), L"text");
  if (text.empty()) {
    return {false,
            incomplete ? ResponseFailure::GenerationIncomplete
                       : ResponseFailure::EmptyResponse,
            L"", incomplete ? L"Gemini generation reached its token limit."
                              : L"Gemini response text is empty."};
  }
  if (incomplete) {
    return {false, ResponseFailure::GenerationIncomplete, text,
            L"Gemini generation reached its token limit."};
  }
  if (!complete) {
    return {false, ResponseFailure::ProviderResponseError, text,
            L"Gemini generation stopped with unsupported finishReason: " +
                finishReasonValue->scalar};
  }
  return {true, ResponseFailure::None, text, L""};
}

ProviderEnvelopeResult extractClaudeEnvelope(const std::wstring &body) {
  const ParseResult parsed = JsonParser(body).parse();
  if (!parsed.success || parsed.value.type != JsonType::Object) {
    return malformedEnvelope(L"Invalid Claude response: ", parsed);
  }
  const JsonValue *stopReasonValue = findProperty(parsed.value, L"stop_reason");
  if (!stopReasonValue || stopReasonValue->type != JsonType::String ||
      stopReasonValue->scalar.empty()) {
    return {false, ResponseFailure::ProviderResponseError, L"",
            L"Claude response has no valid stop_reason."};
  }
  const bool incomplete = stopReasonValue->scalar == L"max_tokens";
  const bool complete = stopReasonValue->scalar == L"end_turn" ||
                        stopReasonValue->scalar == L"stop_sequence";
  const JsonValue *content = findProperty(parsed.value, L"content");
  if (!content || content->type != JsonType::Array) {
    return {false,
            incomplete ? ResponseFailure::GenerationIncomplete
                       : ResponseFailure::ProviderResponseError,
            L"", incomplete ? L"Claude generation reached its token limit."
                              : L"Claude response has no content array."};
  }
  for (const JsonValue &block : content->array) {
    if (block.type != JsonType::Object ||
        stringProperty(block, L"type") != L"text") {
      continue;
    }
    const std::wstring text = stringProperty(block, L"text");
    if (text.empty()) continue;
    if (incomplete) {
      return {false, ResponseFailure::GenerationIncomplete, text,
              L"Claude generation reached its token limit."};
    }
    if (!complete) {
      return {false, ResponseFailure::ProviderResponseError, text,
              L"Claude generation stopped with unsupported stop_reason: " +
                  stopReasonValue->scalar};
    }
    return {true, ResponseFailure::None, text, L""};
  }
  return {false,
          incomplete ? ResponseFailure::GenerationIncomplete
                     : ResponseFailure::EmptyResponse,
          L"", incomplete ? L"Claude generation reached its token limit."
                            : L"Claude response text is empty."};
}

ProviderEnvelopeResult extractResponsesEnvelope(const std::wstring &body) {
  const ParseResult parsed = JsonParser(body).parse();
  if (!parsed.success || parsed.value.type != JsonType::Object) {
    return malformedEnvelope(L"Invalid Responses response: ", parsed);
  }
  const JsonValue *statusValue = findProperty(parsed.value, L"status");
  const std::wstring status = statusValue && statusValue->type == JsonType::String
                                  ? statusValue->scalar
                                  : L"";
  if (statusValue && statusValue->type != JsonType::String) {
    return {false, ResponseFailure::ProviderResponseError, L"",
            L"Responses status is not a string."};
  }
  const bool incomplete = status == L"incomplete" || status == L"queued" ||
                          status == L"in_progress" || status == L"cancelled";
  std::wstring incompleteReason;
  if (incomplete) {
    if (const JsonValue *details = findProperty(parsed.value, L"incomplete_details")) {
      if (details->type == JsonType::Object) incompleteReason = stringProperty(*details, L"reason");
    }
  }
  if (status == L"failed") {
    return {false, ResponseFailure::ProviderResponseError, L"", L"Responses request failed."};
  }
  if (statusValue && !incomplete && status != L"completed") {
    return {false, ResponseFailure::ProviderResponseError, L"",
            L"Responses request has an unknown status: " + status};
  }
  if (const JsonValue *providerError = findProperty(parsed.value, L"error")) {
    if (providerError->type != JsonType::Null) {
      return {false, ResponseFailure::ProviderResponseError, L"",
              L"Responses response contains a provider error."};
    }
  }
  const JsonValue *output = findProperty(parsed.value, L"output");
  if (!output || output->type != JsonType::Array) {
    if (incomplete) {
      return {false, ResponseFailure::GenerationIncomplete, L"",
              L"Generation is incomplete" +
                  (incompleteReason.empty() ? L"."
                                            : L": " + incompleteReason)};
    }
    return {false, ResponseFailure::ProviderResponseError, L"",
            L"Responses response has no output array."};
  }
  std::wstring combined;
  for (const JsonValue &item : output->array) {
    if (item.type != JsonType::Object) continue;
    if (stringProperty(item, L"type") == L"refusal") {
      return {false, ResponseFailure::ProviderResponseError, L"",
              L"The provider refused the structured response."};
    }
    const JsonValue *content = findProperty(item, L"content");
    if (!content || content->type != JsonType::Array) continue;
    for (const JsonValue &part : content->array) {
      if (part.type == JsonType::Object && stringProperty(part, L"type") == L"refusal") {
        return {false, ResponseFailure::ProviderResponseError,
                stringProperty(part, L"refusal"),
                L"The provider refused the structured response."};
      }
      if (part.type != JsonType::Object || stringProperty(part, L"type") != L"output_text") continue;
      const std::wstring text = stringProperty(part, L"text");
      if (combined.size() + text.size() > 4 * 1024 * 1024) {
        return {false, ResponseFailure::ProviderResponseError, L"",
                L"Responses output exceeds the size limit."};
      }
      combined += text;
    }
  }
  if (combined.empty()) {
    if (incomplete) return {false, ResponseFailure::GenerationIncomplete, L"",
                            L"Generation is incomplete" +
                                (incompleteReason.empty() ? L"." : L": " + incompleteReason)};
    return {false, ResponseFailure::EmptyResponse, L"",
            L"Responses response has no output text."};
  }
  if (incomplete) {
    return {false, ResponseFailure::GenerationIncomplete, combined,
            L"Generation is incomplete" +
                (incompleteReason.empty() ? L"." : L": " + incompleteReason)};
  }
  return {true, ResponseFailure::None, combined, L"", statusValue != nullptr};
}

std::wstring responseFailureLabel(ResponseFailure failure) {
  switch (failure) {
  case ResponseFailure::None: return L"None";
  case ResponseFailure::HttpError: return L"HTTP error";
  case ResponseFailure::ProviderResponseError: return L"Provider response error";
  case ResponseFailure::EmptyResponse: return L"Empty response";
  case ResponseFailure::JsonParseError: return L"JSON parse error";
  case ResponseFailure::SchemaValidationError: return L"Schema validation error";
  case ResponseFailure::GenerationIncomplete: return L"Generation incomplete";
  case ResponseFailure::UnsupportedStructuredOutput: return L"Unsupported structured output";
  }
  return L"Provider response error";
}


bool prettyPrintJson(const std::wstring &input, std::wstring &output) {
  output.clear();
  if (input.size() > 1024 * 1024 || !JsonParser(input).parse().success) return false;
  int depth = 0;
  bool quoted = false, escaped = false;
  const auto newline = [&] { output += L"\r"; output.append(static_cast<size_t>(depth) * 2, L' '); };
  for (wchar_t ch : input) {
    if (quoted) {
      output += ch;
      if (escaped) escaped = false;
      else if (ch == L'\\') escaped = true;
      else if (ch == L'"') quoted = false;
    } else if (ch == L'"') { quoted = true; output += ch; }
    else if (ch == L'{' || ch == L'[') { output += ch; ++depth; newline(); }
    else if (ch == L'}' || ch == L']') { --depth; newline(); output += ch; }
    else if (ch == L',') { output += ch; newline(); }
    else if (ch == L':') output += L": ";
    else if (!isJsonWhitespace(ch)) output += ch;
  }
  return true;
}
