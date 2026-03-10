/**
 * @file logger.h
 * @brief MiniRTC logging interface
 */

#ifndef MINIRTC_LOGGER_H
#define MINIRTC_LOGGER_H

#include "types.h"
#include "macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Log levels */
typedef enum {
    MINIRTC_LOG_LEVEL_DEBUG = 0,
    MINIRTC_LOG_LEVEL_INFO,
    MINIRTC_LOG_LEVEL_WARN,
    MINIRTC_LOG_LEVEL_ERROR,
    MINIRTC_LOG_LEVEL_FATAL
} minirtc_log_level_t;

/* Log callback type */
typedef void (*minirtc_log_cb_t)(minirtc_log_level_t level, const char* file, int line, const char* fmt, ...);

/**
 * @brief Initialize the logger
 * @return MINIRTC_OK on success
 */
MINIRTC_API minirtc_status_t minirtc_logger_init(void);

/**
 * @brief Set log level
 * @param level Minimum log level to output
 */
MINIRTC_API void minirtc_logger_set_level(minirtc_log_level_t level);

/**
 * @brief Set log callback
 * @param cb Log callback function (NULL to use default)
 */
MINIRTC_API void minirtc_logger_set_callback(minirtc_log_cb_t cb);

/**
 * @brief Get current log level
 * @return Current log level
 */
MINIRTC_API minirtc_log_level_t minirtc_logger_get_level(void);

/**
 * @brief Log a message
 * @param level Log level
 * @param file Source file name
 * @param line Source line number
 * @param fmt Format string
 */
MINIRTC_API void minirtc_logger_log(minirtc_log_level_t level, const char* file, int line, const char* fmt, ...);

/**
 * @brief Shutdown the logger
 */
MINIRTC_API void minirtc_logger_shutdown(void);

/* Convenience macros */
#define MINIRTC_LOG_DEBUG(fmt, ...) minirtc_logger_log(MINIRTC_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MINIRTC_LOG_INFO(fmt, ...)  minirtc_logger_log(MINIRTC_LOG_LEVEL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MINIRTC_LOG_WARN(fmt, ...)  minirtc_logger_log(MINIRTC_LOG_LEVEL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MINIRTC_LOG_ERROR(fmt, ...) minirtc_logger_log(MINIRTC_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define MINIRTC_LOG_FATAL(fmt, ...) minirtc_logger_log(MINIRTC_LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* MINIRTC_LOGGER_H */
