#include "test_harness.h"

#include <limits>
#include <string>

#include "xp2gdl90/simple_json.h"

namespace {

std::string MakeControlString(char ch) {
  std::string text;
  text.push_back('"');
  text.push_back(ch);
  text.push_back('"');
  return text;
}

}  // namespace

TEST_CASE("Simple JSON parses composite values and supports object lookup") {
  xp2gdl90::json::Value value;
  std::string error;
  const std::string text =
      " { \"number\": -12.5e1, \"truth\": true, \"lie\": false,"
      " \"nothing\": null, \"array\": [1, \"two\", {\"nested\": 3}],"
      " \"object\": {\"child\": 4} } ";

  ASSERT_TRUE(xp2gdl90::json::Parse(text, &value, &error));
  ASSERT_EQ(std::string(""), error);
  ASSERT_TRUE(value.IsObject());
  ASSERT_TRUE(value.Find("missing") == nullptr);
  ASSERT_EQ(-125.0, value.Find("number")->number_value);
  ASSERT_TRUE(value.Find("truth")->bool_value);
  ASSERT_TRUE(!value.Find("lie")->bool_value);
  ASSERT_TRUE(value.Find("nothing")->IsNull());
  ASSERT_TRUE(value.Find("array")->IsArray());
  ASSERT_EQ(3.0, value.Find("array")->array_values[2].Find("nested")->number_value);
  ASSERT_EQ(4.0, value.Find("object")->Find("child")->number_value);

  const xp2gdl90::json::Value scalar;
  ASSERT_TRUE(scalar.Find("anything") == nullptr);

  ASSERT_TRUE(xp2gdl90::json::Parse("[]", &value, &error));
  ASSERT_TRUE(value.IsArray());
  ASSERT_EQ(static_cast<size_t>(0), value.array_values.size());

  ASSERT_TRUE(xp2gdl90::json::Parse("{}", &value, &error));
  ASSERT_TRUE(value.IsObject());
  ASSERT_EQ(static_cast<size_t>(0), value.object_values.size());
}

TEST_CASE("Simple JSON parses string escapes and unicode sequences") {
  xp2gdl90::json::Value value;
  std::string error;
  const std::string text =
      "\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u0041\\u03A9\\uD83D\\uDE80\"";

  ASSERT_TRUE(xp2gdl90::json::Parse(text, &value, &error));
  ASSERT_TRUE(value.IsString());

  std::string expected;
  expected.push_back('"');
  expected.push_back('\\');
  expected.push_back('/');
  expected.push_back('\b');
  expected.push_back('\f');
  expected.push_back('\n');
  expected.push_back('\r');
  expected.push_back('\t');
  expected += "A";
  expected += "\xCE\xA9";
  expected += "\xF0\x9F\x9A\x80";
  ASSERT_EQ(expected, value.string_value);

  const std::string euro_text = R"("\u20ac")";
  ASSERT_TRUE(xp2gdl90::json::Parse(euro_text, &value, &error));
  ASSERT_EQ(std::string("\xE2\x82\xAC"), value.string_value);
}

TEST_CASE("Simple JSON parses signed numeric formats") {
  xp2gdl90::json::Value value;
  std::string error;

  ASSERT_TRUE(xp2gdl90::json::Parse("+42", &value, &error));
  ASSERT_EQ(42.0, value.number_value);
  ASSERT_TRUE(xp2gdl90::json::Parse("-0.25", &value, &error));
  ASSERT_EQ(-0.25, value.number_value);
  ASSERT_TRUE(xp2gdl90::json::Parse("6.02E2", &value, &error));
  ASSERT_EQ(602.0, value.number_value);
}

TEST_CASE("Simple JSON escape helper covers control characters") {
  const std::string input =
      std::string("\\\"\b\f\n\r\t") + std::string(1, static_cast<char>(0x01)) +
      "A";
  ASSERT_EQ(std::string("\\\\\\\"\\b\\f\\n\\r\\t\\u0001A"),
            xp2gdl90::json::EscapeString(input));
}

TEST_CASE("Simple JSON reports required output and trailing text") {
  std::string error;
  ASSERT_TRUE(!xp2gdl90::json::Parse("null", nullptr, &error));
  ASSERT_TRUE(error.find("Output JSON value is required") != std::string::npos);

  xp2gdl90::json::Value value;
  ASSERT_TRUE(!xp2gdl90::json::Parse("null x", &value, &error));
  ASSERT_TRUE(
      error.find("Unexpected trailing characters after JSON value") !=
      std::string::npos);
}

TEST_CASE("Simple JSON reports string parsing failures") {
  xp2gdl90::json::Value value;
  std::string error;

  ASSERT_TRUE(!xp2gdl90::json::Parse(MakeControlString('\x01'), &value, &error));
  ASSERT_TRUE(
      error.find("JSON strings cannot contain control characters") !=
      std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("\"unterminated", &value, &error));
  ASSERT_TRUE(error.find("Unexpected end of input in JSON string") !=
              std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("\"\\", &value, &error));
  ASSERT_TRUE(
      error.find("Unexpected end of input in JSON string escape") !=
      std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("\"\\x\"", &value, &error));
  ASSERT_TRUE(error.find("Invalid JSON string escape") != std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("\"\\u12G4\"", &value, &error));
  ASSERT_TRUE(error.find("Invalid JSON unicode escape") != std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("\"\\uD800\"", &value, &error));
  ASSERT_TRUE(error.find("Invalid JSON unicode escape") != std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("\"\\uDC00\"", &value, &error));
  ASSERT_TRUE(error.find("Invalid JSON unicode escape") != std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("\"\\uD800\\u0041\"", &value, &error));
  ASSERT_TRUE(error.find("Invalid JSON unicode escape") != std::string::npos);
}

TEST_CASE("Simple JSON reports number parsing failures") {
  xp2gdl90::json::Value value;
  std::string error;

  ASSERT_TRUE(!xp2gdl90::json::Parse("", &value, &error));
  ASSERT_TRUE(
      error.find("Unexpected end of input while parsing JSON value") !=
      std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("+", &value, &error));
  ASSERT_TRUE(error.find("Invalid JSON number") != std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("1e+", &value, &error));
  ASSERT_TRUE(error.find("Invalid JSON exponent") != std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("1e999", &value, &error));
  ASSERT_TRUE(error.find("Invalid JSON number") != std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("tru", &value, &error));
  ASSERT_TRUE(
      error.find("Unexpected end of input while parsing JSON number") !=
      std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("tx", &value, &error));
  ASSERT_TRUE(error.find("Invalid JSON number") != std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("fx", &value, &error));
  ASSERT_TRUE(error.find("Invalid JSON number") != std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("nx", &value, &error));
  ASSERT_TRUE(error.find("Invalid JSON number") != std::string::npos);
}

TEST_CASE("Simple JSON reports array parsing failures") {
  xp2gdl90::json::Value value;
  std::string error;

  ASSERT_TRUE(!xp2gdl90::json::Parse("[ ", &value, &error));
  ASSERT_TRUE(
      error.find("Unexpected end of input while parsing JSON value") !=
      std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("[1", &value, &error));
  ASSERT_TRUE(error.find("Unexpected end of input in JSON array") !=
              std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("[1 2]", &value, &error));
  ASSERT_TRUE(error.find("Expected ',' or ']' in JSON array") !=
              std::string::npos);
}

TEST_CASE("Simple JSON reports object parsing failures") {
  xp2gdl90::json::Value value;
  std::string error;

  ASSERT_TRUE(!xp2gdl90::json::Parse("{", &value, &error));
  ASSERT_TRUE(error.find("Expected '\"' to start JSON string") !=
              std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("{\"a\" 1}", &value, &error));
  ASSERT_TRUE(error.find("Expected ':' after JSON object key") !=
              std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("{\"a\":1", &value, &error));
  ASSERT_TRUE(error.find("Unexpected end of input in JSON object") !=
              std::string::npos);

  ASSERT_TRUE(!xp2gdl90::json::Parse("{\"a\":1 \"b\":2}", &value, &error));
  ASSERT_TRUE(error.find("Expected ',' or '}' in JSON object") !=
              std::string::npos);
}
