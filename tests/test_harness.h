#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace test_harness {

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<TestCase>& Registry() {
  static std::vector<TestCase> tests;
  return tests;
}

struct Registrar {
  Registrar(std::string name, std::function<void()> fn) {
    Registry().push_back(TestCase{std::move(name), std::move(fn)});
  }
};

class AssertionFailure : public std::runtime_error {
 public:
  explicit AssertionFailure(const std::string& message)
      : std::runtime_error(message) {}
};

inline std::string FormatMessage(const char* file, int line,
                                 const std::string& message) {
  std::ostringstream oss;
  oss << file << ":" << line << ": " << message;
  return oss.str();
}

}  // namespace test_harness

#define TEST_CONCAT_INTERNAL(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INTERNAL(a, b)
#define TEST_CASE_IMPL(name, id)                                             \
  static void TEST_CONCAT(test_fn_, id)();                                   \
  static test_harness::Registrar                                             \
      TEST_CONCAT(test_reg_, id)(name, TEST_CONCAT(test_fn_, id));           \
  static void TEST_CONCAT(test_fn_, id)()
#define TEST_CASE(name) TEST_CASE_IMPL(name, __COUNTER__)

#define ASSERT_TRUE(condition)                                              \
  do {                                                                      \
    if (!(condition)) {                                                     \
      throw test_harness::AssertionFailure(                                 \
          test_harness::FormatMessage(__FILE__, __LINE__,                   \
                                      "ASSERT_TRUE failed: " #condition)); \
    }                                                                       \
  } while (0)

#define ASSERT_EQ(expected, actual)                                         \
  do {                                                                      \
    if (!((expected) == (actual))) {                                        \
      std::ostringstream _assert_eq_oss;                                    \
      _assert_eq_oss << "ASSERT_EQ failed: expected '" << (expected)        \
                      << "' got '" << (actual) << "'";                      \
      throw test_harness::AssertionFailure(                                 \
          test_harness::FormatMessage(__FILE__, __LINE__,                   \
                                      _assert_eq_oss.str()));               \
    }                                                                       \
  } while (0)

#define ASSERT_NE(expected, actual)                                         \
  do {                                                                      \
    if ((expected) == (actual)) {                                           \
      std::ostringstream _assert_ne_oss;                                    \
      _assert_ne_oss << "ASSERT_NE failed: did not expect '" << (expected)   \
                      << "'";                                               \
      throw test_harness::AssertionFailure(                                 \
          test_harness::FormatMessage(__FILE__, __LINE__,                   \
                                      _assert_ne_oss.str()));               \
    }                                                                       \
  } while (0)
