#include "test_harness.h"

int main() {
  int failures = 0;
  const auto& tests = test_harness::Registry();

  std::cout << "Running " << tests.size() << " tests\n";
  for (const auto& test : tests) {
    try {
      test.fn();
      std::cout << "[PASS] " << test.name << "\n";
    } catch (const test_harness::AssertionFailure& failure) {
      ++failures;
      std::cout << "[FAIL] " << test.name << "\n";
      std::cout << "  " << failure.what() << "\n";
    } catch (const std::exception& ex) {
      ++failures;
      std::cout << "[FAIL] " << test.name << "\n";
      std::cout << "  Unexpected exception: " << ex.what() << "\n";
    } catch (...) {
      ++failures;
      std::cout << "[FAIL] " << test.name << "\n";
      std::cout << "  Unknown exception\n";
    }
  }

  if (failures > 0) {
    std::cout << failures << " test(s) failed\n";
    return 1;
  }

  std::cout << "All tests passed\n";
  return 0;
}
