#ifndef FLUX_TEST_H
#define FLUX_TEST_H

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Flux {
namespace Test {

// Test result status
enum class TestStatus {
    Passed,
    Failed,
    Skipped,
    Error
};

// Single test result
struct TestResult {
    std::string name;
    std::string suite;
    TestStatus status;
    std::string message;
    double durationMs;
    std::string file;
    int line;
    
    TestResult() : status(TestStatus::Passed), durationMs(0.0), line(0) {}
};

// Test suite result
struct TestSuiteResult {
    std::string name;
    std::vector<TestResult> tests;
    int passed;
    int failed;
    int skipped;
    int errors;
    double totalDurationMs;
    
    TestSuiteResult() : passed(0), failed(0), skipped(0), errors(0), totalDurationMs(0.0) {}
    
    void addResult(const TestResult& result) {
        tests.push_back(result);
        totalDurationMs += result.durationMs;
        
        switch (result.status) {
            case TestStatus::Passed: ++passed; break;
            case TestStatus::Failed: ++failed; break;
            case TestStatus::Skipped: ++skipped; break;
            case TestStatus::Error: ++errors; break;
        }
    }
};

// Full test run result
struct TestRunResult {
    std::vector<TestSuiteResult> suites;
    int totalTests;
    int totalPassed;
    int totalFailed;
    int totalSkipped;
    int totalErrors;
    double totalDurationMs;
    bool success;
    
    TestRunResult() : totalTests(0), totalPassed(0), totalFailed(0), 
                      totalSkipped(0), totalErrors(0), totalDurationMs(0.0),
                      success(true) {}
    
    void addSuite(const TestSuiteResult& suite) {
        suites.push_back(suite);
        totalTests += suite.tests.size();
        totalPassed += suite.passed;
        totalFailed += suite.failed;
        totalSkipped += suite.skipped;
        totalErrors += suite.errors;
        totalDurationMs += suite.totalDurationMs;
        
        if (suite.failed > 0 || suite.errors > 0) {
            success = false;
        }
    }
};

// Assertion macros helpers
inline std::string formatValue(double value) {
    std::ostringstream oss;
    oss << std::setprecision(10) << value;
    return oss.str();
}

inline std::string formatValue(const std::string& value) {
    return "\"" + value + "\"";
}

inline std::string formatValue(bool value) {
    return value ? "true" : "false";
}

template<typename T>
inline std::string formatValue(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// Assertion failure exception
class AssertionFailure {
public:
    std::string message;
    std::string file;
    int line;
    
    AssertionFailure(const std::string& msg, const std::string& f, int l)
        : message(msg), file(f), line(l) {}
};

// Test context
class TestContext {
public:
    static TestContext& instance() {
        static TestContext ctx;
        return ctx;
    }
    
    void setCurrentTest(const std::string& name, const std::string& suite, 
                        const std::string& file, int line) {
        m_currentTest = name;
        m_currentSuite = suite;
        m_currentFile = file;
        m_currentLine = line;
    }
    
    const std::string& currentTest() const { return m_currentTest; }
    const std::string& currentSuite() const { return m_currentSuite; }
    const std::string& currentFile() const { return m_currentFile; }
    int currentLine() const { return m_currentLine; }
    
    void setVerbose(bool v) { m_verbose = v; }
    bool isVerbose() const { return m_verbose; }
    
private:
    TestContext() : m_verbose(false) {}
    
    std::string m_currentTest;
    std::string m_currentSuite;
    std::string m_currentFile;
    int m_currentLine = 0;
    bool m_verbose;
};

// Assertion functions
namespace Assert {

inline void assertTrue(bool condition, const std::string& msg = "", 
                       const char* file = __FILE__, int line = __LINE__) {
    if (!condition) {
        std::string message = "Expected true";
        if (!msg.empty()) message += ": " + msg;
        throw AssertionFailure(message, file, line);
    }
}

inline void assertFalse(bool condition, const std::string& msg = "",
                        const char* file = __FILE__, int line = __LINE__) {
    if (condition) {
        std::string message = "Expected false";
        if (!msg.empty()) message += ": " + msg;
        throw AssertionFailure(message, file, line);
    }
}

inline void assertEquals(double expected, double actual, double tolerance = 1e-10,
                         const std::string& msg = "",
                         const char* file = __FILE__, int line = __LINE__) {
    if (std::abs(expected - actual) > tolerance) {
        std::ostringstream oss;
        oss << "Expected " << formatValue(expected) 
            << " but got " << formatValue(actual);
        if (!msg.empty()) oss << ": " << msg;
        throw AssertionFailure(oss.str(), file, line);
    }
}

inline void assertEquals(int expected, int actual,
                         const std::string& msg = "",
                         const char* file = __FILE__, int line = __LINE__) {
    if (expected != actual) {
        std::ostringstream oss;
        oss << "Expected " << expected << " but got " << actual;
        if (!msg.empty()) oss << ": " << msg;
        throw AssertionFailure(oss.str(), file, line);
    }
}

template<typename T>
inline void assertEquals(const T& expected, const T& actual,
                         const std::string& msg = "",
                         const char* file = __FILE__, int line = __LINE__) {
    if (expected != actual) {
        std::ostringstream oss;
        oss << "Expected " << formatValue(expected) 
            << " but got " << formatValue(actual);
        if (!msg.empty()) oss << ": " << msg;
        throw AssertionFailure(oss.str(), file, line);
    }
}

inline void assertNotEquals(double a, double b, double tolerance = 1e-10,
                            const std::string& msg = "",
                            const char* file = __FILE__, int line = __LINE__) {
    if (std::abs(a - b) <= tolerance) {
        std::ostringstream oss;
        oss << "Expected values to differ, but both are " << formatValue(a);
        if (!msg.empty()) oss << ": " << msg;
        throw AssertionFailure(oss.str(), file, line);
    }
}

inline void assertLessThan(double a, double b,
                           const std::string& msg = "",
                           const char* file = __FILE__, int line = __LINE__) {
    if (a >= b) {
        std::ostringstream oss;
        oss << "Expected " << formatValue(a) << " < " << formatValue(b);
        if (!msg.empty()) oss << ": " << msg;
        throw AssertionFailure(oss.str(), file, line);
    }
}

inline void assertGreaterThan(double a, double b,
                              const std::string& msg = "",
                              const char* file = __FILE__, int line = __LINE__) {
    if (a <= b) {
        std::ostringstream oss;
        oss << "Expected " << formatValue(a) << " > " << formatValue(b);
        if (!msg.empty()) oss << ": " << msg;
        throw AssertionFailure(oss.str(), file, line);
    }
}

inline void assertNear(double expected, double actual, double tolerance = 1e-6,
                       const std::string& msg = "",
                       const char* file = __FILE__, int line = __LINE__) {
    if (std::abs(expected - actual) > tolerance) {
        std::ostringstream oss;
        oss << "Expected " << formatValue(expected) << "  " << tolerance
            << " but got " << formatValue(actual);
        if (!msg.empty()) oss << ": " << msg;
        throw AssertionFailure(oss.str(), file, line);
    }
}

inline void assertThrows(std::function<void()> func,
                         const std::string& msg = "",
                         const char* file = __FILE__, int line = __LINE__) {
    try {
        func();
        std::ostringstream oss;
        oss << "Expected exception";
        if (!msg.empty()) oss << ": " << msg;
        throw AssertionFailure(oss.str(), file, line);
    } catch (...) {
        // Expected
    }
}

} // namespace Assert

// Test registration
using TestFunction = std::function<void()>;

struct TestRegistration {
    std::string name;
    std::string suite;
    TestFunction func;
    std::string file;
    int line;
    bool enabled;
    
    TestRegistration(const std::string& n, const std::string& s, TestFunction f,
                     const std::string& fpath, int l)
        : name(n), suite(s), func(f), file(fpath), line(l), enabled(true) {}
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry registry;
        return registry;
    }
    
    void registerTest(const std::string& name, const std::string& suite,
                      TestFunction func, const std::string& file, int line) {
        m_tests.emplace_back(name, suite, func, file, line);
    }
    
    const std::vector<TestRegistration>& tests() const { return m_tests; }
    
    void setFilter(const std::string& pattern) { m_filter = pattern; }
    const std::string& filter() const { return m_filter; }
    
private:
    TestRegistry() = default;
    std::vector<TestRegistration> m_tests;
    std::string m_filter;
};

// Test runner
class TestRunner {
public:
    static TestRunResult runAll() {
        return runFiltered("");
    }
    
    static TestRunResult runFiltered(const std::string& filter) {
        TestRunResult result;
        auto& registry = TestRegistry::instance();
        
        // Group tests by suite
        std::map<std::string, std::vector<const TestRegistration*>> suites;
        
        for (const auto& test : registry.tests()) {
            if (!test.enabled) continue;
            
            if (!filter.empty()) {
                if (test.name.find(filter) == std::string::npos &&
                    test.suite.find(filter) == std::string::npos) {
                    continue;
                }
            }
            
            suites[test.suite].push_back(&test);
        }
        
        // Run each suite
        for (auto& [suiteName, tests] : suites) {
            TestSuiteResult suiteResult;
            suiteResult.name = suiteName;
            
            for (const auto* test : tests) {
                TestResult testResult = runTest(*test);
                suiteResult.addResult(testResult);
            }
            
            result.addSuite(suiteResult);
        }
        
        return result;
    }
    
    static TestResult runTest(const TestRegistration& test) {
        TestResult result;
        result.name = test.name;
        result.suite = test.suite;
        result.file = test.file;
        result.line = test.line;
        
        auto& ctx = TestContext::instance();
        ctx.setCurrentTest(test.name, test.suite, test.file, test.line);
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        try {
            test.func();
            result.status = TestStatus::Passed;
            result.message = "PASSED";
        } catch (const AssertionFailure& e) {
            result.status = TestStatus::Failed;
            std::ostringstream oss;
            oss << "FAILED at " << e.file << ":" << e.line << ": " << e.message;
            result.message = oss.str();
        } catch (const std::exception& e) {
            result.status = TestStatus::Error;
            result.message = std::string("ERROR: ") + e.what();
        } catch (...) {
            result.status = TestStatus::Error;
            result.message = "ERROR: Unknown exception";
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.durationMs = std::chrono::duration<double, std::milli>(
            endTime - startTime).count();
        
        // Print result if verbose
        if (ctx.isVerbose()) {
            std::cout << "  [" << (result.status == TestStatus::Passed ? "" : "") << "] "
                      << test.name << " (" << std::fixed << std::setprecision(2) 
                      << result.durationMs << "ms)" << std::endl;
            if (result.status != TestStatus::Passed) {
                std::cout << "      " << result.message << std::endl;
            }
        }
        
        return result;
    }
};

// Report generation
class TestReporter {
public:
    static void printSummary(const TestRunResult& result) {
        std::cout << "\n";
        std::cout << "\n";
        std::cout << "                    TEST SUMMARY\n";
        std::cout << "\n\n";
        
        for (const auto& suite : result.suites) {
            std::cout << "Suite: " << suite.name << "\n";
            std::cout << "  Passed:  " << suite.passed << "\n";
            std::cout << "  Failed:  " << suite.failed << "\n";
            std::cout << "  Skipped: " << suite.skipped << "\n";
            std::cout << "  Errors:  " << suite.errors << "\n";
            std::cout << "  Time:    " << std::fixed << std::setprecision(2) 
                      << suite.totalDurationMs << "ms\n\n";
            
            // Print failed tests
            for (const auto& test : suite.tests) {
                if (test.status == TestStatus::Failed || test.status == TestStatus::Error) {
                    std::cout << "   " << test.name << "\n";
                    std::cout << "    " << test.message << "\n";
                }
            }
        }
        
        std::cout << "\n";
        std::cout << "Total:  " << result.totalTests << " tests, "
                  << result.totalPassed << " passed, "
                  << result.totalFailed << " failed, "
                  << result.totalErrors << " errors\n";
        std::cout << "Time:   " << std::fixed << std::setprecision(2) 
                  << result.totalDurationMs << "ms\n";
        
        if (result.success) {
            std::cout << "\n All tests passed!\n";
        } else {
            std::cout << "\n Some tests failed!\n";
        }
        std::cout << "\n";
    }
    
    static void printJUnitXML(const TestRunResult& result, const std::string& outputPath) {
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "Cannot open " << outputPath << " for writing\n";
            return;
        }
        
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        file << "<testsuites tests=\"" << result.totalTests << "\" "
             << "failures=\"" << result.totalFailed << "\" "
             << "errors=\"" << result.totalErrors << "\" "
             << "time=\"" << (result.totalDurationMs / 1000.0) << "\">\n";
        
        for (const auto& suite : result.suites) {
            file << "  <testsuite name=\"" << suite.name << "\" "
                 << "tests=\"" << suite.tests.size() << "\" "
                 << "failures=\"" << suite.failed << "\" "
                 << "errors=\"" << suite.errors << "\" "
                 << "time=\"" << (suite.totalDurationMs / 1000.0) << "\">\n";
            
            for (const auto& test : suite.tests) {
                file << "    <testcase name=\"" << test.name << "\" "
                     << "classname=\"" << test.suite << "\" "
                     << "time=\"" << (test.durationMs / 1000.0) << "\"";
                
                if (test.status == TestStatus::Failed) {
                    file << ">\n      <failure message=\"" << test.message << "\"/>\n    </testcase>\n";
                } else if (test.status == TestStatus::Error) {
                    file << ">\n      <error message=\"" << test.message << "\"/>\n    </testcase>\n";
                } else if (test.status == TestStatus::Skipped) {
                    file << ">\n      <skipped/>\n    </testcase>\n";
                } else {
                    file << "/>\n";
                }
            }
            
            file << "  </testsuite>\n";
        }
        
        file << "</testsuites>\n";
        file.close();
        
        std::cout << "JUnit XML written to " << outputPath << "\n";
    }
};

} // namespace Test
} // namespace Flux

// Test macros
#define FLUX_TEST(suite, name) \
    static void flux_test_##suite##_##name(); \
    static struct flux_test_reg_##suite##_##name { \
        flux_test_reg_##suite##_##name() { \
            Flux::Test::TestRegistry::instance().registerTest( \
                #name, #suite, flux_test_##suite##_##name, __FILE__, __LINE__); \
        } \
    } flux_test_instance_##suite##_##name; \
    static void flux_test_##suite##_##name()

#define FLUX_TEST_DISABLED(suite, name) \
    static void flux_test_##suite##_##name(); \
    static struct flux_test_reg_##suite##_##name { \
        flux_test_reg_##suite##_##name() { \
            auto& reg = Flux::Test::TestRegistry::instance(); \
            Flux::Test::TestRegistration test(#name, #suite, flux_test_##suite##_##name, __FILE__, __LINE__); \
            test.enabled = false; \
            /* Would need to modify registry to support disabled */ \
        } \
    } flux_test_instance_##suite##_##name; \
    static void flux_test_##suite##_##name()

#define FLUX_TEST_FIXTURE(suite, name, fixture) \
    FLUX_TEST(suite, name)

// Assertion macros
#define FLUX_ASSERT_TRUE(cond) \
    Flux::Test::Assert::assertTrue(cond, #cond, __FILE__, __LINE__)

#define FLUX_ASSERT_FALSE(cond) \
    Flux::Test::Assert::assertFalse(cond, #cond, __FILE__, __LINE__)

#define FLUX_ASSERT_EQ(expected, actual) \
    Flux::Test::Assert::assertEquals(expected, actual, "", __FILE__, __LINE__)

#define FLUX_ASSERT_NEAR(expected, actual, tol) \
    Flux::Test::Assert::assertNear(expected, actual, tol, "", __FILE__, __LINE__)

#define FLUX_ASSERT_THROWS(expr) \
    Flux::Test::Assert::assertThrows([&]() { expr; }, "", __FILE__, __LINE__)

#define FLUX_FAIL(msg) \
    throw Flux::Test::AssertionFailure(msg, __FILE__, __LINE__)

#endif // FLUX_TEST_H
