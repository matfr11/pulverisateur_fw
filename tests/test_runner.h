#pragma once
#include <stdio.h>
#include <string.h>

static int _pass = 0;
static int _fail = 0;
static const char *_suite = "";

#define SUITE(name) do { _suite = name; printf("\n\033[1;37m%s\033[0m\n", name); } while(0)

#define TEST_ASSERT(expr) do { \
    if (!(expr)) { \
        printf("  \033[31m[FAIL]\033[0m %s:%d : %s\n", __func__, __LINE__, #expr); \
        _fail++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  \033[31m[FAIL]\033[0m %s:%d : %s == %s  (got %lld vs %lld)\n", \
               __func__, __LINE__, #a, #b, (long long)(a), (long long)(b)); \
        _fail++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_FLOAT_EQ(a, b, tol) do { \
    float _a = (a), _b = (b), _t = (tol); \
    float _d = _a - _b; if (_d < 0) _d = -_d; \
    if (_d > _t) { \
        printf("  \033[31m[FAIL]\033[0m %s:%d : |%s - %s| = %.6f > %.6f\n", \
               __func__, __LINE__, #a, #b, _d, _t); \
        _fail++; \
        return; \
    } \
} while(0)

#define RUN(fn) do { \
    _pass++; \
    fn(); \
    if (_fail == 0 || /* already counted */ 1) { \
        /* check if fn incremented _fail */ \
    } \
    printf("  \033[32m[PASS]\033[0m %s\n", #fn); \
} while(0)

/* Version correcte : on compte le nombre d'échecs avant et après */
#undef RUN
#define RUN(fn) do { \
    int _before = _fail; \
    fn(); \
    if (_fail == _before) { \
        printf("  \033[32m[PASS]\033[0m %s\n", #fn); \
        _pass++; \
    } \
    /* _fail déjà incrémenté dans TEST_ASSERT si échec */ \
} while(0)

#define SUMMARY() do { \
    printf("\n\033[1;37m══════════════════════════════\033[0m\n"); \
    printf("  Total  : %d\n", _pass + _fail); \
    printf("  \033[32mPassés  : %d\033[0m\n", _pass); \
    if (_fail > 0) \
        printf("  \033[31mÉchecs  : %d\033[0m\n", _fail); \
    else \
        printf("  \033[32mÉchecs  : 0\033[0m\n"); \
    printf("\033[1;37m══════════════════════════════\033[0m\n"); \
} while(0)

#define EXIT_CODE() (_fail > 0 ? 1 : 0)
