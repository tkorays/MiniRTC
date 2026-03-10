/**
 * @file minirtc.h
 * @brief MiniRTC - Lightweight Real-Time Communications Library
 */

#ifndef MINIRTC_H
#define MINIRTC_H

#include "types.h"
#include "macros.h"
#include "logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Library version */
#define MINIRTC_VERSION_MINOR MINIRTC_VERSION_MAJOR
#define MINIRTC_VERSION       MINIRTC_VERSION_STRING

/**
 * @brief Initialize MiniRTC library
 * @return MINIRTC_OK on success
 */
MINIRTC_API minirtc_status_t minirtc_init(void);

/**
 * @brief Shutdown MiniRTC library
 */
MINIRTC_API void minirtc_shutdown(void);

/**
 * @brief Get library version string
 * @return Version string (e.g., "1.0.0")
 */
MINIRTC_API const char* minirtc_version(void);

/**
 * @brief Check if library is initialized
 * @return true if initialized
 */
MINIRTC_API bool minirtc_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* MINIRTC_H */
