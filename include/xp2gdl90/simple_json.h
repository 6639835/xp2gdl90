#ifndef XP2GDL90_SIMPLE_JSON_H
#define XP2GDL90_SIMPLE_JSON_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xp2gdl90::json {

enum class Type {
  Null,
  Bool,
  Number,
  String,
  Object,
  Array,
};

struct Value {
  Type type = Type::Null;
  bool bool_value = false;
  double number_value = 0.0;
  std::string string_value;
  std::vector<std::pair<std::string, Value>> object_values;
  std::vector<Value> array_values;

  bool IsNull() const { return type == Type::Null; }
  bool IsBool() const { return type == Type::Bool; }
  bool IsNumber() const { return type == Type::Number; }
  bool IsString() const { return type == Type::String; }
  bool IsObject() const { return type == Type::Object; }
  bool IsArray() const { return type == Type::Array; }

  const Value* Find(std::string_view key) const;
};

bool Parse(std::string_view text, Value* out_value, std::string* out_error);
std::string EscapeString(std::string_view input);

}  // namespace xp2gdl90::json

#endif  // XP2GDL90_SIMPLE_JSON_H
