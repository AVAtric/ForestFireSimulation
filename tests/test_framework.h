#pragma once

#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace tf {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase> &registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> fn) {
        registry().push_back({std::move(name), std::move(fn)});
    }
};

struct AssertionFailure : public std::exception {
    std::string msg;
    explicit AssertionFailure(std::string m) : msg(std::move(m)) {}
    const char *what() const noexcept override { return msg.c_str(); }
};

inline int run() {
    int failed = 0;
    for (const auto &tc: registry()) {
        try {
            tc.fn();
            std::cout << "[ PASS ] " << tc.name << "\n";
        } catch (const std::exception &e) {
            std::cout << "[ FAIL ] " << tc.name << ": " << e.what() << "\n";
            ++failed;
        }
    }
    std::cout << "\n" << (registry().size() - failed) << "/" << registry().size() << " tests passed\n";
    return failed == 0 ? 0 : 1;
}

}

#define TF_CONCAT_IMPL(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT_IMPL(a, b)

#define TEST(name) \
    static void name(); \
    static const ::tf::Registrar TF_CONCAT(_tf_reg_, name){#name, name}; \
    static void name()

#define EXPECT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::ostringstream _tf_oss; \
            _tf_oss << __FILE__ << ":" << __LINE__ << " EXPECT_TRUE failed: " #cond; \
            throw ::tf::AssertionFailure(_tf_oss.str()); \
        } \
    } while (0)

#define EXPECT_EQ(a, b) \
    do { \
        auto _tf_a = (a); \
        auto _tf_b = (b); \
        if (!(_tf_a == _tf_b)) { \
            std::ostringstream _tf_oss; \
            _tf_oss << __FILE__ << ":" << __LINE__ << " EXPECT_EQ failed: " \
                    << static_cast<long long>(_tf_a) << " != " << static_cast<long long>(_tf_b); \
            throw ::tf::AssertionFailure(_tf_oss.str()); \
        } \
    } while (0)
