/**
 * @file test_framework.c
 * @brief Test framework implementation
 *
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 */

#include "test_framework.h"
#include "pico2-swd-riscv/rp2350.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

//==============================================================================
// Global State
//==============================================================================

static swd_target_t *g_target = NULL;
static bool g_connected = false;
static bool g_initialized = false;

//==============================================================================
// Initialization
//==============================================================================

void test_framework_init(swd_target_t *target) {
    g_target = target;
    g_connected = false;
    g_initialized = false;
}

swd_target_t* test_get_target(void) {
    return g_target;
}

//==============================================================================
// Response Helpers
//==============================================================================

void test_send_response(const char *status, const char *message) {
    printf("%s", status);
    if (message) {
        printf(":%s", message);
    }
    printf("\n");
    fflush(stdout);
}

void test_send_value(uint32_t value) {
    printf("%s:%08lx\n", RESP_VALUE, (unsigned long)value);
    fflush(stdout);
}

//==============================================================================
// Setup and Cleanup
//==============================================================================

swd_error_t test_setup(void) {
    if (!g_target) {
        printf("# ERROR: No target available\n");
        return SWD_ERROR_INVALID_PARAM;
    }

    // Connect and initialize only on first test
    if (!g_connected) {
        printf("# Connecting to target...\n");
        swd_error_t err = swd_connect(g_target);
        if (err != SWD_OK) {
            printf("# Failed to connect: %s\n", swd_error_string(err));
            return err;
        }
        g_connected = true;
        printf("# Connected to target\n");

        printf("# Initializing RP2350 debug module...\n");
        err = rp2350_init(g_target);
        if (err != SWD_OK) {
            printf("# Failed to initialize: %s\n", swd_error_string(err));
            swd_disconnect(g_target);
            g_connected = false;
            return err;
        }
        g_initialized = true;
        printf("# RP2350 debug module initialized\n");
    } else {
        // Between tests: just halt both harts to ensure clean state
        // Don't reset - that breaks PC write functionality
        printf("# Halting harts for clean state...\n");
        rp2350_halt(g_target, 0);
        rp2350_halt(g_target, 1);
    }

    return SWD_OK;
}

void test_cleanup(void) {
    // This is called after each test - do minimal cleanup
    // Just resume harts so they're not stuck halted
    if (g_target && g_initialized) {
        rp2350_resume(g_target, 0);
        rp2350_resume(g_target, 1);
    }
}

void test_final_cleanup(void) {
    // This disconnects everything - only called at end of test suite
    if (!g_target) {
        return;
    }

    printf("# Final cleanup - disconnecting...\n");

    // Resume both harts
    if (g_initialized) {
        rp2350_resume(g_target, 0);
        rp2350_resume(g_target, 1);
    }

    // Disconnect
    if (g_connected) {
        swd_disconnect(g_target);
        g_connected = false;
        printf("# Disconnected from target\n");
    }

    g_initialized = false;
}

//==============================================================================
// Test Execution
//==============================================================================

bool test_run_single(test_case_t *test_case) {
    if (!test_case || !test_case->test_func) {
        printf("# ERROR: Invalid test case\n");
        return false;
    }

    printf("\n========================================\n");
    printf("%s\n", test_case->name);
    printf("========================================\n");

    // Setup (connect on first test, reset between tests)
    swd_error_t err = test_setup();
    if (err != SWD_OK) {
        printf("# Setup failed: %s\n", swd_error_string(err));
        test_case->passed = false;
        test_case->ran = true;
        return false;
    }

    // Run test
    bool passed = test_case->test_func(g_target);
    test_case->passed = passed;
    test_case->ran = true;

    // Cleanup (just resume harts)
    test_cleanup();

    // Report result
    if (passed) {
        printf("# RESULT: PASS\n");
    } else {
        printf("# RESULT: FAIL\n");
    }

    return passed;
}

test_stats_t test_run_suite(test_case_t *tests, uint32_t count) {
    test_stats_t stats = {0};

    if (!tests || count == 0) {
        return stats;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (test_run_single(&tests[i])) {
            stats.passed++;
        } else {
            stats.failed++;
        }
        stats.total++;
    }

    return stats;
}

void test_print_stats(const test_stats_t *stats) {
    if (!stats) return;

    printf("\n");
    printf("====================================\n");
    printf("  Test Results\n");
    printf("====================================\n");
    printf("Total:   %lu\n", (unsigned long)stats->total);
    printf("Passed:  %lu\n", (unsigned long)stats->passed);
    printf("Failed:  %lu\n", (unsigned long)stats->failed);
    printf("Skipped: %lu\n", (unsigned long)stats->skipped);
    printf("====================================\n");

    if (stats->failed == 0) {
        printf("ALL TESTS PASSED!\n");
    } else {
        printf("SOME TESTS FAILED\n");
    }
}
