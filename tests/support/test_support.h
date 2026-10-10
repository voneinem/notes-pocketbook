#pragma once
// Minimal test support: registration, assertions, skip. No external framework.

#include <cstdio>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace pbtest {

struct Failure {
    std::string message;
};
struct Skip {
    std::string reason;
};

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

template <typename T>
std::string show(const T& v) {
    std::ostringstream os;
    os << v;
    return os.str();
}
inline std::string show(const std::string& v) { return "\"" + v + "\""; }
inline std::string show(const char* v) { return std::string("\"") + v + "\""; }
inline std::string show(bool v) { return v ? "true" : "false"; }

// Runs every registered case. Exit code: 0 all passed, 1 any failed, 77 everything skipped
// (CTest SKIP_RETURN_CODE).
inline int run_all() {
    int failed = 0, skipped = 0, passed = 0;
    for (const auto& c : registry()) {
        try {
            c.fn();
            std::printf("[ OK   ] %s\n", c.name);
            ++passed;
        } catch (const Skip& s) {
            std::printf("[ SKIP ] %s: %s\n", c.name, s.reason.c_str());
            ++skipped;
        } catch (const Failure& f) {
            std::printf("[ FAIL ] %s\n         %s\n", c.name, f.message.c_str());
            ++failed;
        } catch (const std::exception& e) {
            std::printf("[ FAIL ] %s\n         unexpected exception: %s\n", c.name, e.what());
            ++failed;
        } catch (...) {
            std::printf("[ FAIL ] %s\n         unexpected exception\n", c.name);
            ++failed;
        }
    }
    std::printf("%d passed, %d failed, %d skipped\n", passed, failed, skipped);
    if (failed) return 1;
    if (passed == 0 && skipped > 0) return 77;
    return 0;
}

}  // namespace pbtest

#define PB_TEST(name)                                                    \
    static void name();                                                  \
    static ::pbtest::Registrar pbtest_registrar_##name(#name, name);     \
    static void name()

#define PB_FAIL_AT(msg)                                                  \
    throw ::pbtest::Failure { std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " + (msg) }

#define PB_CHECK(cond)                                                   \
    do {                                                                 \
        if (!(cond)) PB_FAIL_AT(std::string("CHECK failed: ") + #cond);  \
    } while (0)

#define PB_CHECK_EQ(a, b)                                                                  \
    do {                                                                                   \
        const auto pb_a = (a);                                                             \
        const auto pb_b = (b);                                                             \
        if (!(pb_a == pb_b))                                                               \
            PB_FAIL_AT(std::string("CHECK_EQ failed: ") + #a + " == " + #b + "  (" +       \
                       ::pbtest::show(pb_a) + " vs " + ::pbtest::show(pb_b) + ")");        \
    } while (0)

#define PB_SKIP(reason) throw ::pbtest::Skip { (reason) }

#define PB_TEST_MAIN() \
    int main() { return ::pbtest::run_all(); }
