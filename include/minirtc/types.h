/**
 * @file types.h
 * @brief MiniRTC basic type definitions
 */

#ifndef MINIRTC_TYPES_H
#define MINIRTC_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Signed integers */
typedef int8_t      int8;
typedef int16_t     int16;
typedef int32_t     int32;
typedef int64_t     int64;

/* Unsigned integers */
typedef uint8_t     uint8;
typedef uint16_t    uint16;
typedef uint32_t    uint32;
typedef uint64_t    uint64;

/* Common defines */
#ifndef NULL
#define NULL ((void*)0)
#endif

#define MINIRTC_VERSION_MAJOR 1
#define MINIRTC_VERSION_MINOR 0
#define MINIRTC_VERSION_PATCH 0

#define MINIRTC_VERSION_STRING "1.0.0"

/* Status codes */
typedef enum {
    MINIRTC_OK = 0,
    MINIRTC_ERROR = -1,
    MINIRTC_NULL_PTR = -2,
    MINIRTC_INVALID_PARAM = -3,
    MINIRTC_NO_MEMORY = -4,
    MINIRTC_TIMEOUT = -5,
    MINIRTC_NOT_INITIALIZED = -6,
    MINIRTC_ALREADY_INITIALIZED = -7
} minirtc_status_t;

#endif /* MINIRTC_TYPES_H */
