// ME Pro EQ — ME Audio
// Minimal, dependency-free test harness. Records PASS/FAIL per test plus named measurements, and can write
// a Markdown report so that every reported number comes straight from an executed test.
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace metest
{

struct TestCase
{
    std::string name;
    std::string suite;
    std::function<void()> fn;
};

struct Measurement
{
    std::string test;
    std::string key;
    std::string value;
};

struct Context
{
    std::vector<TestCase> tests;
    std::vector<Measurement> measurements;
    std::vector<std::string> failures;
    std::string currentTest;
    int checks = 0;
    int failedChecks = 0;
    bool currentFailed = false;

    static Context& get()
    {
        static Context c;
        return c;
    }
};

struct Registrar
{
    Registrar (const char* suite, const char* name, std::function<void()> fn)
    {
        Context::get().tests.push_back ({ name, suite, std::move (fn) });
    }
};

inline void fail (const std::string& what, const char* file, int line)
{
    auto& c = Context::get();
    c.failedChecks++;
    c.currentFailed = true;
    std::ostringstream os;
    os << c.currentTest << ": " << what << " (" << file << ":" << line << ")";
    c.failures.push_back (os.str());
    if (c.failures.size() < 200)
        std::printf ("    FAILED: %s\n", os.str().c_str());
}

inline void check (bool ok, const std::string& what, const char* file, int line)
{
    Context::get().checks++;
    if (! ok)
        fail (what, file, line);
}

template <typename T>
std::string str (const T& v)
{
    std::ostringstream os;
    os.precision (6);
    os << v;
    return os.str();
}

inline void measure (const std::string& key, const std::string& value)
{
    auto& c = Context::get();
    c.measurements.push_back ({ c.currentTest, key, value });
}

inline void measure (const std::string& key, double value, const char* unit = "")
{
    std::ostringstream os;
    os.precision (6);
    os << value << (unit[0] ? " " : "") << unit;
    measure (key, os.str());
}

} // namespace metest

#define ME_CONCAT2(a, b) a##b
#define ME_CONCAT(a, b) ME_CONCAT2 (a, b)

#define ME_TEST(suite, name)                                                                                    \
    static void ME_CONCAT (test_fn_, name)();                                                                   \
    static metest::Registrar ME_CONCAT (test_reg_, name) (suite, #name, &ME_CONCAT (test_fn_, name));           \
    static void ME_CONCAT (test_fn_, name)()

#define CHECK(cond) metest::check ((cond), #cond, __FILE__, __LINE__)

#define CHECK_MSG(cond, msg)                                                                                    \
    do                                                                                                          \
    {                                                                                                           \
        std::ostringstream os__;                                                                                \
        os__ << msg;                                                                                            \
        metest::check ((cond), std::string (#cond) + " | " + os__.str(), __FILE__, __LINE__);                  \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                                                   \
    do                                                                                                          \
    {                                                                                                           \
        const double va__ = (double) (a), vb__ = (double) (b), t__ = (double) (tol);                            \
        std::ostringstream os__;                                                                                \
        os__ << #a << " = " << va__ << ", " << #b << " = " << vb__ << ", |diff| = " << std::abs (va__ - vb__)  \
             << " > " << t__;                                                                                   \
        metest::check (std::abs (va__ - vb__) <= t__, os__.str(), __FILE__, __LINE__);                          \
    } while (0)
