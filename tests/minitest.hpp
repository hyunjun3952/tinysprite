/* minitest.hpp - dependency-free unit test helpers for tinysprite.
 *
 * Usage:
 *   TEST(name) { ASSERT_EQ_INT(1, 1); }
 *   int main(void) { RUN(name); return TEST_SUMMARY(); }
 */
#ifndef MINITEST_H
#define MINITEST_H

#include <stdio.h>
#include <stdlib.h>

#include <optional>
#include <utility>

static int mt_failures = 0;
static int mt_run = 0;
static int mt_current_failed = 0;

/* The value of an optional that the test needs to be set: the run ends if it is not. */
template <class T> T must_have(std::optional<T> value)
{
    if (!value) {
        printf("  expected a value\n");
        abort();
    }
    return std::move(*value);
}

#define TEST(name) static void test_##name(void)

#define RUN(name)                                                                                  \
    do {                                                                                           \
        mt_current_failed = 0;                                                                     \
        mt_run++;                                                                                  \
        test_##name();                                                                             \
        if (mt_current_failed)                                                                     \
            mt_failures++;                                                                         \
        printf("[%s] %s\n", mt_current_failed ? "fail" : "ok", #name);                             \
    } while (0)

#define MT_FAIL(...)                                                                               \
    do {                                                                                           \
        printf("  %s:%d: ", __FILE__, __LINE__);                                                   \
        printf(__VA_ARGS__);                                                                       \
        printf("\n");                                                                              \
        mt_current_failed = 1;                                                                     \
    } while (0)

#define ASSERT_TRUE(cond)                                                                          \
    do {                                                                                           \
        if (!(cond))                                                                               \
            MT_FAIL("expected true: %s", #cond);                                                   \
    } while (0)

#define ASSERT_EQ_INT(expected, actual)                                                            \
    do {                                                                                           \
        long long mt_e = (long long)(expected), mt_a = (long long)(actual);                        \
        if (mt_e != mt_a)                                                                          \
            MT_FAIL("%s == %s: expected %lld, got %lld", #expected, #actual, mt_e, mt_a);          \
    } while (0)

/* Character comparison, widened through unsigned char (what clang-tidy wants for char values). */
#define ASSERT_EQ_CHAR(expected, actual)                                                           \
    ASSERT_EQ_INT(static_cast<unsigned char>(expected), static_cast<unsigned char>(actual))

/* Passes if evaluating `expr` throws an exception of type `type`. */
#define ASSERT_THROWS(expr, type)                                                                  \
    do {                                                                                           \
        try {                                                                                      \
            (void)(expr);                                                                          \
            MT_FAIL("expected %s to throw", #expr);                                                \
        } catch (const type &) {                                                                   \
        }                                                                                          \
    } while (0)

#define TEST_SUMMARY() (printf("%d run, %d failed\n", mt_run, mt_failures), mt_failures ? 1 : 0)

#endif
