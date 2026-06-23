/**
 * @file    unity.h
 * @brief   Unity Test Framework - 轻量级 C 语言单元测试框架
 *
 * Unity 是嵌入式 C 开发中最流行的测试框架之一。
 * 特点: 单头文件、无依赖、适合在 PC 上测试 MCU 代码逻辑。
 *
 * 基本用法:
 *   TEST_ASSERT_EQUAL(expected, actual)      // 断言相等
 *   TEST_ASSERT_FLOAT_WITHIN(delta, exp, act) // 浮点数近似断言
 *   TEST_ASSERT_TRUE(condition)              // 断言为真
 *   TEST_ASSERT_FALSE(condition)             // 断言为假
 *
 * 来源: https://github.com/ThrowTheSwitch/Unity (精简版)
 */

#ifndef UNITY_H
#define UNITY_H

#include <stdio.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 测试统计 ======================== */
static int _unity_total   = 0;   // 总测试数
static int _unity_passed  = 0;   // 通过数
static int _unity_failed  = 0;   // 失败数

/* ======================== 断言宏 ======================== */

#define UNITY_TEST_START(name) \
    _current_test = name; \
    printf("  [TEST] %s ... ", name)

#define UNITY_TEST_END() \
    _unity_total++; \
    _unity_passed++; \
    printf("PASSED\n")

#define UNITY_TEST_FAIL(msg) do { \
    _unity_total++; \
    _unity_failed++; \
    printf("FAILED\n"); \
    printf("         %s:%d: %s\n", __FILE__, __LINE__, msg); \
} while(0)

/* 整数相等 */
#define TEST_ASSERT_EQUAL(expected, actual) do { \
    int _exp = (expected); \
    int _act = (actual); \
    if (_exp != _act) { \
        printf("FAILED\n"); \
        printf("         %s:%d: 期望=%d, 实际=%d\n", __FILE__, __LINE__, _exp, _act); \
        _unity_total++; _unity_failed++; \
    } else { _unity_total++; _unity_passed++; printf(".\n"); } \
} while(0)

/* 整数不等 */
#define TEST_ASSERT_NOT_EQUAL(expected, actual) do { \
    int _exp = (expected); \
    int _act = (actual); \
    if (_exp == _act) { \
        printf("FAILED\n"); \
        printf("         %s:%d: 不应该等于 %d\n", __FILE__, __LINE__, _exp); \
        _unity_total++; _unity_failed++; \
    } else { _unity_total++; _unity_passed++; printf(".\n"); } \
} while(0)

/* 条件为真 */
#define TEST_ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        printf("FAILED\n"); \
        printf("         %s:%d: 条件为假\n", __FILE__, __LINE__); \
        _unity_total++; _unity_failed++; \
    } else { _unity_total++; _unity_passed++; printf(".\n"); } \
} while(0)

/* 条件为假 */
#define TEST_ASSERT_FALSE(condition) do { \
    if ((condition)) { \
        printf("FAILED\n"); \
        printf("         %s:%d: 条件应为假\n", __FILE__, __LINE__); \
        _unity_total++; _unity_failed++; \
    } else { _unity_total++; _unity_passed++; printf(".\n"); } \
} while(0)

/* 浮点数近似相等 (考虑浮点精度误差) */
#define TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual) do { \
    float _exp = (expected); \
    float _act = (actual); \
    float _delta = (delta); \
    if (fabsf(_exp - _act) > _delta) { \
        printf("FAILED\n"); \
        printf("         %s:%d: 期望=%.2f, 实际=%.2f (误差>%.4f)\n", \
               __FILE__, __LINE__, _exp, _act, _delta); \
        _unity_total++; _unity_failed++; \
    } else { _unity_total++; _unity_passed++; printf(".\n"); } \
} while(0)

/* 指针非空 */
#define TEST_ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        printf("FAILED\n"); \
        printf("         %s:%d: 指针为NULL\n", __FILE__, __LINE__); \
        _unity_total++; _unity_failed++; \
    } else { _unity_total++; _unity_passed++; printf(".\n"); } \
} while(0)

/* 字符串相等 */
#define TEST_ASSERT_EQUAL_STRING(expected, actual) do { \
    const char *_exp = (expected); \
    const char *_act = (actual); \
    if (strcmp(_exp, _act) != 0) { \
        printf("FAILED\n"); \
        printf("         %s:%d: 期望=\"%s\", 实际=\"%s\"\n", __FILE__, __LINE__, _exp, _act); \
        _unity_total++; _unity_failed++; \
    } else { _unity_total++; _unity_passed++; printf(".\n"); } \
} while(0)

/* ======================== 测试结果输出 ======================== */
static void UnityPrintSummary(void)
{
    printf("\n");
    printf("============================================\n");
    printf("  测试结果汇总\n");
    printf("============================================\n");
    printf("  总计: %d\n", _unity_total);
    printf("  通过: %d  ✅\n", _unity_passed);
    printf("  失败: %d  %s\n", _unity_failed, _unity_failed ? "❌" : "");
    printf("============================================\n");

    if (_unity_failed == 0) {
        printf("  🎉 全部测试通过!\n\n");
    } else {
        printf("  🔴 存在失败用例，请检查上述错误\n\n");
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UNITY_H */
