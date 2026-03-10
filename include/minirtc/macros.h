/**
 * @file macros.h
 * @brief MiniRTC common macros
 */

#ifndef MINIRTC_MACROS_H
#define MINIRTC_MACROS_H

/* Compiler macros */
#define MINIRTC_UNUSED(x) (void)(x)

#ifdef __GNUC__
#define MINIRTC_LIKELY(x)   __builtin_expect(!!(x), 1)
#define MINIRTC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define MINIRTC_LIKELY(x)   (x)
#define MINIRTC_UNLIKELY(x) (x)
#endif

/* Array size */
#define MINIRTC_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Min/Max */
#define MINIRTC_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MINIRTC_MAX(a, b) (((a) > (b)) ? (a) : (b))

/* Stringify */
#define MINIRTC_STRINGIFY(x) #x
#define MINIRTC_TO_STRING(x) MINIRTC_STRINGIFY(x)

/* Concatenate */
#define MINIRTC_CONCAT(a, b) a##b
#define MINIRTC_CONCAT_IMPL(a, b) a##b

/* Alignment */
#define MINIRTC_ALIGN(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define MINIRTC_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)

/* Bit operations */
#define MINIRTC_BIT(n) (1U << (n))
#define MINIRTC_BIT_MASK(n) ((1U << (n)) - 1)

/* Export/Import macros */
#ifdef MINIRTC_STATIC
#define MINIRTC_API
#else
#ifdef _WIN32
#define MINIRTC_API __declspec(dllexport)
#else
#define MINIRTC_API __attribute__((visibility("default")))
#endif
#endif

/* Deprecated */
#ifdef __GNUC__
#define MINIRTC_DEPRECATED __attribute__((deprecated))
#elif defined(_WIN32)
#define MINIRTC_DEPRECATED __declspec(deprecated)
#else
#define MINIRTC_DEPRECATED
#endif

#endif /* MINIRTC_MACROS_H */
