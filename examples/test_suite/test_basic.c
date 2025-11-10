/**
 * @file test_basic.c
 * @brief Basic connection and initialization tests
 *
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 */

#include "test_framework.h"
#include "pico2-swd-riscv/rp2350.h"
#include <stdio.h>

//==============================================================================
// Test 1: Connection Verification
//==============================================================================

static bool test_connection_verify(swd_target_t *target) {
    printf("# Verifying connection...\n");

    // Read IDCODE to verify connection
    swd_result_t idcode = swd_read_idcode(target);
    if (idcode.error != SWD_OK) {
        printf("# Failed to read IDCODE: %s\n", swd_error_string(idcode.error));
        test_send_response(RESP_FAIL, "Failed to read IDCODE");
        return false;
    }

    printf("# IDCODE: 0x%08lx\n", (unsigned long)idcode.value);
    test_send_value(idcode.value);
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test 2: Debug Module Status Check
//==============================================================================

static bool test_debug_module_status(swd_target_t *target) {
    printf("# Checking debug module status...\n");

    // Try to read hart 0 PC to verify debug module is working
    swd_result_t pc = rp2350_read_pc(target, 0);
    if (pc.error != SWD_OK) {
        printf("# Failed to read PC: %s\n", swd_error_string(pc.error));
        test_send_response(RESP_FAIL, "Debug module not responding");
        return false;
    }

    printf("# Debug module operational\n");
    printf("# Hart 0 PC: 0x%08lx\n", (unsigned long)pc.value);
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test Suite Definition
//==============================================================================

test_case_t basic_tests[] = {
    { "TEST 1: Connection Verification", test_connection_verify, false, false },
    { "TEST 2: Debug Module Status", test_debug_module_status, false, false },
};

const uint32_t basic_test_count = sizeof(basic_tests) / sizeof(basic_tests[0]);
