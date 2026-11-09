// SPDX-License-Identifier: MIT
/*
 * Leid 2026.11
 */
#ifndef _TEST_FRAMEWORK_HH_
#define _TEST_FRAMEWORK_HH_

// Minimal assertion helper for the test executables: no dependencies, C++11.
//
//   TEST_CASE("name") { CHECK(cond); CHECK_EQ(1, 1); }
//   int main() { return testfw::run_all(); }

#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace testfw {

struct Case {
  const char *name;
  std::function<void()> body;
};

inline std::vector<Case> &cases() {
  static std::vector<Case> all;
  return all;
}

inline int &failures() {
  static int count = 0;
  return count;
}

inline int &checks() {
  static int count = 0;
  return count;
}

struct Registrar {
  Registrar(const char *name, std::function<void()> body) {
    Case item;
    item.name = name;
    item.body = body;
    cases().push_back(item);
  }
};

inline void report_failure(const char *file, int line, const char *expression, const char *detail) {
  ++failures();
  if (detail && detail[0]) {
    std::fprintf(stderr, "FAIL %s:%d: %s [%s]\n", file, line, expression, detail);
  } else {
    std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expression);
  }
}

template <typename T>
std::string to_text(const T &value) {
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

inline std::string to_text(const std::string &value) {
  return value;
}

inline std::string to_text(const char *value) {
  return value ? std::string(value) : std::string("<null>");
}

inline std::string to_text(bool value) {
  return value ? "true" : "false";
}

inline int run_all() {
  std::printf("running %zu test case(s)\n", cases().size());
  for (std::size_t i = 0; i < cases().size(); ++i) {
    int before = failures();
    std::printf("[ RUN      ] %s\n", cases()[i].name);
    cases()[i].body();
    if (failures() == before) {
      std::printf("[       OK ] %s\n", cases()[i].name);
    } else {
      std::printf("[   FAILED ] %s\n", cases()[i].name);
    }
  }

  if (failures() == 0) {
    std::printf("all %d check(s) passed\n", checks());
    return 0;
  }
  std::printf("%d of %d check(s) failed\n", failures(), checks());
  return 1;
}

}  // namespace testfw

#define TEST_FW_CAT_(a, b) a##b
#define TEST_FW_CAT(a, b) TEST_FW_CAT_(a, b)

#define TEST_CASE(name)                                                     \
  static void TEST_FW_CAT(test_case_body_, __LINE__)();                     \
  static ::testfw::Registrar TEST_FW_CAT(test_case_reg_, __LINE__)(         \
      name, &TEST_FW_CAT(test_case_body_, __LINE__));                       \
  static void TEST_FW_CAT(test_case_body_, __LINE__)()

#define CHECK(cond)                                                          \
  do {                                                                       \
    ++::testfw::checks();                                                    \
    if (!(cond)) {                                                           \
      ::testfw::report_failure(__FILE__, __LINE__, #cond, "");               \
    }                                                                        \
  } while (0)

#define CHECK_EQ(expected, actual)                                           \
  do {                                                                       \
    ++::testfw::checks();                                                    \
    const std::string test_fw_e = ::testfw::to_text(expected);                \
    const std::string test_fw_a = ::testfw::to_text(actual);                  \
    if (!(test_fw_e == test_fw_a)) {                                         \
      const std::string test_fw_msg =                                        \
          test_fw_e + " != " + test_fw_a;                                    \
      ::testfw::report_failure(__FILE__, __LINE__, #expected " == " #actual, \
                               test_fw_msg.c_str());                         \
    }                                                                        \
  } while (0)

#endif  // !_TEST_FRAMEWORK_HH_
