/*
 * ekt_test.h - Minimal assertion macros, so the test suite has no dependencies
 * beyond a C99 compiler.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef EKT_TEST_H
#define EKT_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ekt_test_failures = 0;
static int ekt_test_checks   = 0;

#define EKT_CHECK(cond)                                                        \
    do {                                                                       \
        ekt_test_checks++;                                                     \
        if (!(cond)) {                                                         \
            ekt_test_failures++;                                               \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                      \
    } while (0)

#define EKT_CHECK_EQ_U32(actual, expected)                                     \
    do {                                                                       \
        unsigned long _a = (unsigned long)(actual);                            \
        unsigned long _e = (unsigned long)(expected);                          \
        ekt_test_checks++;                                                     \
        if (_a != _e) {                                                        \
            ekt_test_failures++;                                               \
            printf("  FAIL %s:%d  %s = %lu, expected %lu\n", __FILE__,          \
                   __LINE__, #actual, _a, _e);                                 \
        }                                                                      \
    } while (0)

#define EKT_CHECK_EQ_STR(actual, expected)                                     \
    do {                                                                       \
        const char *_a = (actual);                                             \
        const char *_e = (expected);                                           \
        ekt_test_checks++;                                                     \
        if ((_a == NULL) || (_e == NULL) || (strcmp(_a, _e) != 0)) {           \
            ekt_test_failures++;                                               \
            printf("  FAIL %s:%d  %s = \"%s\", expected \"%s\"\n", __FILE__,    \
                   __LINE__, #actual, (_a != NULL) ? _a : "(null)",            \
                   (_e != NULL) ? _e : "(null)");                              \
        }                                                                      \
    } while (0)

#define EKT_RUN(fn)                                                            \
    do {                                                                       \
        int before = ekt_test_failures;                                        \
        printf("- %s\n", #fn);                                                 \
        fn();                                                                  \
        if (ekt_test_failures == before) {                                     \
            printf("  ok\n");                                                  \
        }                                                                      \
    } while (0)

#endif /* EKT_TEST_H */
