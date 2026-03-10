/**
 * @file test_minirtc.c
 * @brief MiniRTC basic test
 */

#include <stdio.h>
#include <assert.h>
#include "minirtc/minirtc.h"

static void test_version(void)
{
    const char* version = minirtc_version();
    printf("Test: Version = %s\n", version);
    assert(version != NULL);
}

static void test_init_shutdown(void)
{
    printf("Test: Init/Shutdown\n");
    assert(minirtc_is_initialized() == false);
    
    minirtc_status_t status = minirtc_init();
    assert(status == MINIRTC_OK);
    assert(minirtc_is_initialized() == true);
    
    minirtc_shutdown();
    assert(minirtc_is_initialized() == false);
}

static void test_logger(void)
{
    minirtc_init();
    
    printf("Test: Logger\n");
    minirtc_logger_set_level(MINIRTC_LOG_LEVEL_DEBUG);
    assert(minirtc_logger_get_level() == MINIRTC_LOG_LEVEL_DEBUG);
    
    MINIRTC_LOG_INFO("Logger test message");
    
    minirtc_shutdown();
}

static void test_types(void)
{
    printf("Test: Types\n");
    assert(sizeof(int8)  == 1);
    assert(sizeof(int16) == 2);
    assert(sizeof(int32) == 4);
    assert(sizeof(int64) == 8);
    assert(sizeof(uint8)  == 1);
    assert(sizeof(uint16) == 2);
    assert(sizeof(uint32) == 4);
    assert(sizeof(uint64) == 8);
}

int main(void)
{
    printf("=== MiniRTC Test Suite ===\n\n");
    
    test_version();
    test_types();
    test_init_shutdown();
    test_logger();
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
