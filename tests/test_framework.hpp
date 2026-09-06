#pragma once

// A minimal, dependency-free unit-test framework for NanoX.
//
// Design note: NanoX is meant to be easy to build on Linux/Windows/macOS with
// only CMake + a C++17 compiler. Rather than pull in GoogleTest/Catch2 (which
// need network access at configure time), we ship this tiny header. It provides
// exactly what the project needs:
//   - NX_TEST_CASE(name)   register a test
//   - NX_CHECK(cond)       fail if cond is false
//   - NX_CHECK_EQ(a, b)    fail if a != b, printing both values (requires <<)
//
// Failures throw an exception that is caught by the runner, so a single
// failing assertion does not abort the whole test binary.

#include <cstdio>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace nanox::testing {

struct TestFailure {
    const char* file;
    int line;
    std::string message;
};

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> fn) {
        registry().push_back(TestCase{std::move(name), std::move(fn)});
    }
};

template <typename A, typename B>
std::string make_comparison_message(const char* ea, const char* eb, const A& a, const B& b) {
    std::ostringstream os;
    os << "expected " << ea << " == " << eb << "  (left=" << a << ", right=" << b << ")";
    return os.str();
}

inline int run_all() {
    int passed = 0;
    int failed = 0;

    for (const TestCase& test : registry()) {
        try {
            test.fn();
            std::printf("[PASS] %s\n", test.name.c_str());
            ++passed;
        } catch (const TestFailure& f) {
            std::printf("[FAIL] %s\n    %s:%d: %s\n",
                        test.name.c_str(), f.file, f.line, f.message.c_str());
            ++failed;
        } catch (const std::exception& e) {
            std::printf("[FAIL] %s\n    unexpected exception: %s\n",
                        test.name.c_str(), e.what());
            ++failed;
        } catch (...) {
            std::printf("[FAIL] %s\n    unknown exception\n", test.name.c_str());
            ++failed;
        }
    }

    std::printf("\n%d test(s) passed, %d test(s) failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}

}  // namespace nanox::testing

#define NX_TEST_CASE(name)                                                  \
    static void nx_test_##name();                                           \
    static ::nanox::testing::Registrar nx_registrar_##name(                 \
        #name, &nx_test_##name);                                            \
    static void nx_test_##name()

#define NX_CHECK(cond)                                                      \
    do {                                                                    \
        if (!(cond)) {                                                      \
            throw ::nanox::testing::TestFailure{                            \
                __FILE__, __LINE__, "check failed: " #cond};                \
        }                                                                   \
    } while (0)

#define NX_CHECK_EQ(a, b)                                                   \
    do {                                                                    \
        auto nx_va = (a);                                                   \
        auto nx_vb = (b);                                                   \
        if (!(nx_va == nx_vb)) {                                            \
            throw ::nanox::testing::TestFailure{                            \
                __FILE__, __LINE__,                                         \
                ::nanox::testing::make_comparison_message(#a, #b,           \
                                                          nx_va, nx_vb)};   \
        }                                                                   \
    } while (0)
