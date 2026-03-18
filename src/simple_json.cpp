#include "xp2gdl90/simple_json.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace xp2gdl90::json {
namespace {

struct Cursor {
  const char* p = nullptr;
  const char* end = nullptr;
};

void SkipWhitespace(Cursor* cursor) {
  while (cursor->p < cursor->end) {
    const unsigned char ch = static_cast<unsigned char>(*cursor->p);
    if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
      break;
    }
    ++cursor->p;
  }
}

bool Consume(Cursor* cursor, char expected) {
  SkipWhitespace(cursor);
  if (cursor->p >= cursor->end || *cursor->p != expected) {
    return false;
  }
  ++cursor->p;
  return true;
}

bool ParseHexNibble(char ch, uint8_t* out_value) {
  if (!out_value) {
    return false;
  }
  if (ch >= '0' && ch <= '9') {
    *out_value = static_cast<uint8_t>(ch - '0');
    return true;
  }
  if (ch >= 'a' && ch <= 'f') {
    *out_value = static_cast<uint8_t>(10 + (ch - 'a'));
    return true;
  }
  if (ch >= 'A' && ch <= 'F') {
    *out_value = static_cast<uint8_t>(10 + (ch - 'A'));
    return true;
  }
  return false;
}

void AppendUtf8(std::string* out, uint32_t codepoint) {
  if (!out) {
    return;
  }
  if (codepoint <= 0x7F) {
    out->push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FF) {
    out->push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
    out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0xFFFF) {
    out->push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
    out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else {
    out->push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
    out->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
}

bool ParseUnicodeEscape(Cursor* cursor, uint32_t* out_codepoint) {
  if (!out_codepoint || cursor->end - cursor->p < 4) {
    return false;
  }

  uint8_t nibbles[4] = {};
  for (int i = 0; i < 4; ++i) {
    if (!ParseHexNibble(cursor->p[i], &nibbles[i])) {
      return false;
    }
  }
  cursor->p += 4;

  uint32_t codepoint =
      (static_cast<uint32_t>(nibbles[0]) << 12) |
      (static_cast<uint32_t>(nibbles[1]) << 8) |
      (static_cast<uint32_t>(nibbles[2]) << 4) |
      static_cast<uint32_t>(nibbles[3]);

  if (codepoint < 0xD800 || codepoint > 0xDFFF) {
    *out_codepoint = codepoint;
    return true;
  }

  if (codepoint > 0xDBFF || cursor->end - cursor->p < 6 ||
      cursor->p[0] != '\\' || cursor->p[1] != 'u') {
    return false;
  }
  cursor->p += 2;

  uint32_t low_surrogate = 0;
  if (!ParseUnicodeEscape(cursor, &low_surrogate) ||
      low_surrogate < 0xDC00 || low_surrogate > 0xDFFF) {
    return false;
  }

  *out_codepoint =
      0x10000 + (((codepoint - 0xD800) << 10) | (low_surrogate - 0xDC00));
  return true;
}

bool ParseString(Cursor* cursor, std::string* out_string,
                 std::string* out_error) {
  if (!Consume(cursor, '"')) {
    if (out_error) {
      *out_error = "Expected '\"' to start JSON string";
    }
    return false;
  }

  std::string result;
  while (cursor->p < cursor->end) {
    const char ch = *cursor->p++;
    if (ch == '"') {
      *out_string = std::move(result);
      return true;
    }
    if (static_cast<unsigned char>(ch) < 0x20) {
      if (out_error) {
        *out_error = "JSON strings cannot contain control characters";
      }
      return false;
    }
    if (ch != '\\') {
      result.push_back(ch);
      continue;
    }

    if (cursor->p >= cursor->end) {
      if (out_error) {
        *out_error = "Unexpected end of input in JSON string escape";
      }
      return false;
    }

    const char escape = *cursor->p++;
    switch (escape) {
      case '"':
      case '\\':
      case '/':
        result.push_back(escape);
        break;
      case 'b':
        result.push_back('\b');
        break;
      case 'f':
        result.push_back('\f');
        break;
      case 'n':
        result.push_back('\n');
        break;
      case 'r':
        result.push_back('\r');
        break;
      case 't':
        result.push_back('\t');
        break;
      case 'u': {
        uint32_t codepoint = 0;
        if (!ParseUnicodeEscape(cursor, &codepoint)) {
          if (out_error) {
            *out_error = "Invalid JSON unicode escape";
          }
          return false;
        }
        AppendUtf8(&result, codepoint);
        break;
      }
      default:
        if (out_error) {
          *out_error = "Invalid JSON string escape";
        }
        return false;
    }
  }

  if (out_error) {
    *out_error = "Unexpected end of input in JSON string";
  }
  return false;
}

bool ParseNumber(Cursor* cursor, double* out_number, std::string* out_error) {
  SkipWhitespace(cursor);
  if (cursor->p >= cursor->end) {
    if (out_error) {
      *out_error = "Unexpected end of input while parsing JSON number";
    }
    return false;
  }

  const char* start = cursor->p;
  const char* p = start;

  if (*p == '-' || *p == '+') {
    ++p;
  }

  bool saw_digit = false;
  while (p < cursor->end &&
         std::isdigit(static_cast<unsigned char>(*p)) != 0) {
    saw_digit = true;
    ++p;
  }

  if (p < cursor->end && *p == '.') {
    ++p;
    while (p < cursor->end &&
           std::isdigit(static_cast<unsigned char>(*p)) != 0) {
      saw_digit = true;
      ++p;
    }
  }

  if (!saw_digit) {
    if (out_error) {
      *out_error = "Invalid JSON number";
    }
    return false;
  }

  if (p < cursor->end && (*p == 'e' || *p == 'E')) {
    ++p;
    if (p < cursor->end && (*p == '+' || *p == '-')) {
      ++p;
    }
    bool saw_exp_digit = false;
    while (p < cursor->end &&
           std::isdigit(static_cast<unsigned char>(*p)) != 0) {
      saw_exp_digit = true;
      ++p;
    }
    if (!saw_exp_digit) {
      if (out_error) {
        *out_error = "Invalid JSON exponent";
      }
      return false;
    }
  }

  std::string text(start, p);
  char* parse_end = nullptr;
  const double value = std::strtod(text.c_str(), &parse_end);
  if (!parse_end || *parse_end != '\0' || !std::isfinite(value)) {
    if (out_error) {
      *out_error = "Invalid JSON number";
    }
    return false;
  }

  cursor->p = p;
  *out_number = value;
  return true;
}

bool ParseValue(Cursor* cursor, Value* out_value, std::string* out_error);

bool ParseArray(Cursor* cursor, Value* out_value, std::string* out_error) {
  if (!Consume(cursor, '[')) {
    if (out_error) {
      *out_error = "Expected '[' to start JSON array";
    }
    return false;
  }

  Value result;
  result.type = Type::Array;

  SkipWhitespace(cursor);
  if (cursor->p < cursor->end && *cursor->p == ']') {
    ++cursor->p;
    *out_value = std::move(result);
    return true;
  }

  while (true) {
    Value element;
    if (!ParseValue(cursor, &element, out_error)) {
      return false;
    }
    result.array_values.push_back(std::move(element));

    SkipWhitespace(cursor);
    if (cursor->p >= cursor->end) {
      if (out_error) {
        *out_error = "Unexpected end of input in JSON array";
      }
      return false;
    }
    if (*cursor->p == ',') {
      ++cursor->p;
      continue;
    }
    if (*cursor->p == ']') {
      ++cursor->p;
      *out_value = std::move(result);
      return true;
    }
    if (out_error) {
      *out_error = "Expected ',' or ']' in JSON array";
    }
    return false;
  }
}

bool ParseObject(Cursor* cursor, Value* out_value, std::string* out_error) {
  if (!Consume(cursor, '{')) {
    if (out_error) {
      *out_error = "Expected '{' to start JSON object";
    }
    return false;
  }

  Value result;
  result.type = Type::Object;

  SkipWhitespace(cursor);
  if (cursor->p < cursor->end && *cursor->p == '}') {
    ++cursor->p;
    *out_value = std::move(result);
    return true;
  }

  while (true) {
    std::string key;
    if (!ParseString(cursor, &key, out_error)) {
      return false;
    }
    if (!Consume(cursor, ':')) {
      if (out_error) {
        *out_error = "Expected ':' after JSON object key";
      }
      return false;
    }

    Value value;
    if (!ParseValue(cursor, &value, out_error)) {
      return false;
    }
    result.object_values.emplace_back(std::move(key), std::move(value));

    SkipWhitespace(cursor);
    if (cursor->p >= cursor->end) {
      if (out_error) {
        *out_error = "Unexpected end of input in JSON object";
      }
      return false;
    }
    if (*cursor->p == ',') {
      ++cursor->p;
      continue;
    }
    if (*cursor->p == '}') {
      ++cursor->p;
      *out_value = std::move(result);
      return true;
    }
    if (out_error) {
      *out_error = "Expected ',' or '}' in JSON object";
    }
    return false;
  }
}

bool ParseLiteral(Cursor* cursor, const char* literal) {
  const char* p = literal;
  while (*p != '\0') {
    if (cursor->p >= cursor->end || *cursor->p != *p) {
      return false;
    }
    ++cursor->p;
    ++p;
  }
  return true;
}

bool ParseValue(Cursor* cursor, Value* out_value, std::string* out_error) {
  SkipWhitespace(cursor);
  if (cursor->p >= cursor->end) {
    if (out_error) {
      *out_error = "Unexpected end of input while parsing JSON value";
    }
    return false;
  }

  switch (*cursor->p) {
    case '{':
      return ParseObject(cursor, out_value, out_error);
    case '[':
      return ParseArray(cursor, out_value, out_error);
    case '"': {
      std::string value;
      if (!ParseString(cursor, &value, out_error)) {
        return false;
      }
      out_value->type = Type::String;
      out_value->string_value = std::move(value);
      out_value->object_values.clear();
      out_value->array_values.clear();
      return true;
    }
    case 't':
      if (!ParseLiteral(cursor, "true")) {
        break;
      }
      out_value->type = Type::Bool;
      out_value->bool_value = true;
      out_value->object_values.clear();
      out_value->array_values.clear();
      return true;
    case 'f':
      if (!ParseLiteral(cursor, "false")) {
        break;
      }
      out_value->type = Type::Bool;
      out_value->bool_value = false;
      out_value->object_values.clear();
      out_value->array_values.clear();
      return true;
    case 'n':
      if (!ParseLiteral(cursor, "null")) {
        break;
      }
      *out_value = Value{};
      return true;
    default:
      break;
  }

  double number_value = 0.0;
  if (!ParseNumber(cursor, &number_value, out_error)) {
    return false;
  }
  out_value->type = Type::Number;
  out_value->number_value = number_value;
  out_value->object_values.clear();
  out_value->array_values.clear();
  out_value->string_value.clear();
  return true;
}

}  // namespace

const Value* Value::Find(std::string_view key) const {
  if (type != Type::Object) {
    return nullptr;
  }
  for (const auto& entry : object_values) {
    if (entry.first == key) {
      return &entry.second;
    }
  }
  return nullptr;
}

bool Parse(std::string_view text, Value* out_value, std::string* out_error) {
  if (!out_value) {
    if (out_error) {
      *out_error = "Output JSON value is required";
    }
    return false;
  }

  Cursor cursor{text.data(), text.data() + text.size()};
  Value value;
  if (!ParseValue(&cursor, &value, out_error)) {
    return false;
  }

  SkipWhitespace(&cursor);
  if (cursor.p != cursor.end) {
    if (out_error) {
      *out_error = "Unexpected trailing characters after JSON value";
    }
    return false;
  }

  *out_value = std::move(value);
  if (out_error) {
    out_error->clear();
  }
  return true;
}

std::string EscapeString(std::string_view input) {
  std::string output;
  output.reserve(input.size() + 8);

  static const char kHex[] = "0123456789ABCDEF";
  for (const unsigned char ch : input) {
    switch (ch) {
      case '\\':
        output += "\\\\";
        break;
      case '"':
        output += "\\\"";
        break;
      case '\b':
        output += "\\b";
        break;
      case '\f':
        output += "\\f";
        break;
      case '\n':
        output += "\\n";
        break;
      case '\r':
        output += "\\r";
        break;
      case '\t':
        output += "\\t";
        break;
      default:
        if (ch < 0x20) {
          output += "\\u00";
          output.push_back(kHex[(ch >> 4) & 0x0F]);
          output.push_back(kHex[ch & 0x0F]);
        } else {
          output.push_back(static_cast<char>(ch));
        }
        break;
    }
  }

  return output;
}

}  // namespace xp2gdl90::json
