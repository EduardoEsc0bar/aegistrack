#ifndef DOCTEST_DOCTEST_H
#define DOCTEST_DOCTEST_H

#include <cstdlib>
#include <iostream>

namespace doctest {

using TestFunc = void (*)();

struct TestCaseData {
  const char* name;
  TestFunc func;
  TestCaseData* next;
};

inline TestCaseData*& registry_head() {
  static TestCaseData* head = nullptr;
  return head;
}

inline void register_test(TestCaseData* test) {
  test->next = registry_head();
  registry_head() = test;
}

inline int run_tests() {
  int failed = 0;
  for (TestCaseData* test = registry_head(); test != nullptr; test = test->next) {
    try {
      test->func();
    } catch (...) {
      std::cerr << "Unhandled exception in test case: " << test->name << '\n';
      ++failed;
    }
  }

  if (failed > 0) {
    std::cerr << failed << " test case(s) failed\n";
    return 1;
  }
  return 0;
}

}  // namespace doctest

#define DOCTEST_DETAIL_CONCAT_IMPL(a, b) a##b
#define DOCTEST_DETAIL_CONCAT(a, b) DOCTEST_DETAIL_CONCAT_IMPL(a, b)

#define DOCTEST_DETAIL_TEST_CASE_IMPL(name, line)                               \
  static void DOCTEST_DETAIL_CONCAT(doctest_test_case_, line)();                \
  namespace {                                                                    \
  struct DOCTEST_DETAIL_CONCAT(doctest_registrar_, line) {                       \
    DOCTEST_DETAIL_CONCAT(doctest_registrar_, line)() {                          \
      static ::doctest::TestCaseData tc{                                          \
          name, DOCTEST_DETAIL_CONCAT(doctest_test_case_, line), nullptr};       \
      ::doctest::register_test(&tc);                                             \
    }                                                                            \
  } DOCTEST_DETAIL_CONCAT(doctest_registrar_instance_, line);                    \
  }                                                                              \
  static void DOCTEST_DETAIL_CONCAT(doctest_test_case_, line)()

#define TEST_CASE(name) DOCTEST_DETAIL_TEST_CASE_IMPL(name, __LINE__)

#define CHECK(expr)                                                              \
  do {                                                                           \
    if (!(expr)) {                                                               \
      std::cerr << "CHECK failed: " << #expr << " at " << __FILE__ << ":"    \
                << __LINE__ << '\n';                                            \
      std::exit(1);                                                              \
    }                                                                            \
  } while (false)

#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
int main() {
  return ::doctest::run_tests();
}
#endif

#endif  // DOCTEST_DOCTEST_H
