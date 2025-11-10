/**
 * @file test_hart1.c
 * @brief Hart 1 specific tests
 *
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 */

#include "test_framework.h"
#include "pico2-swd-riscv/rp2350.h"
#include <stdio.h>
#include "pico/stdlib.h"

//==============================================================================
// Test 17: Halt Hart 1
//==============================================================================

static bool test_halt_hart1(swd_target_t *target) {
    printf("# Halting hart 1...\n");

    swd_error_t err = rp2350_halt(target, 1);
    if (err != SWD_OK && err != SWD_ERROR_ALREADY_HALTED) {
        printf("# Failed to halt hart 1: %s\n", swd_error_string(err));
        test_send_response(RESP_FAIL, swd_error_string(err));
        return false;
    }

    printf("# Hart 1 halted successfully\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test 18: Read Hart 1 PC
//==============================================================================

static bool test_read_pc_hart1(swd_target_t *target) {
    printf("# Reading PC from hart 1...\n");

    rp2350_halt(target, 1);  // Ensure halted

    swd_result_t pc = rp2350_read_pc(target, 1);
    if (pc.error != SWD_OK) {
        printf("# Failed to read hart 1 PC: %s\n", swd_error_string(pc.error));
        test_send_response(RESP_FAIL, swd_error_string(pc.error));
        return false;
    }

    printf("# Hart 1 PC = 0x%08lx\n", (unsigned long)pc.value);
    test_send_value(pc.value);
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test 19: Write and Verify Hart 1 Registers
//==============================================================================

static bool test_write_verify_hart1_regs(swd_target_t *target) {
    printf("# Writing and verifying hart 1 registers...\n");

    rp2350_halt(target, 1);

    for (uint8_t reg = 1; reg <= 10; reg++) {
        uint32_t test_val = 0xBADF00D0 | reg;

        swd_error_t err = rp2350_write_reg(target, 1, reg, test_val);
        if (err != SWD_OK) {
            printf("# Failed to write hart 1 x%u: %s\n", reg, swd_error_string(err));
            test_send_response(RESP_FAIL, "Failed to write register");
            return false;
        }

        swd_result_t readback = rp2350_read_reg(target, 1, reg);
        if (readback.error != SWD_OK || readback.value != test_val) {
            printf("# Hart 1 x%u verify failed: wrote 0x%08lx, read 0x%08lx\n",
                   reg, (unsigned long)test_val, (unsigned long)readback.value);
            test_send_response(RESP_FAIL, "Register verification failed");
            return false;
        }
    }

    printf("# Hart 1 register test passed\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test 20: PC Write Verification (Both Harts)
//==============================================================================

static bool test_pc_write_verification(swd_target_t *target) {
    printf("# Testing PC write verification on both harts...\n");
    printf("# This test verifies if PC write actually works on both harts\n");

    // Simple program: sw a0, 0(a1); j 0
    uint32_t program[] = {
        0x00a5a023,  // sw x10, 0(x11) - Store a0 to address in a1
        0x0000006f,  // j 0 - Infinite loop
    };

    uint32_t program_addr = 0x20005000;
    uint32_t h0_result_addr = 0x20006000;
    uint32_t h1_result_addr = 0x20006004;

    // Upload program
    printf("# Uploading test program to 0x%08lx...\n", (unsigned long)program_addr);
    for (uint i = 0; i < sizeof(program)/sizeof(program[0]); i++) {
        swd_error_t err = rp2350_write_mem32(target, program_addr + (i * 4), program[i]);
        if (err != SWD_OK) {
            printf("# Failed to upload program\n");
            test_send_response(RESP_FAIL, "Program upload failed");
            return false;
        }
    }

    // Clear result memory locations
    rp2350_write_mem32(target, h0_result_addr, 0x00000000);
    rp2350_write_mem32(target, h1_result_addr, 0x00000000);

    // Ensure both harts are halted
    rp2350_halt(target, 0);
    rp2350_halt(target, 1);

    // Hart 0: Set a0=0xAAAAAAAA, a1=h0_result_addr
    printf("# Hart 0: Setting a0=0xAAAAAAAA, a1=0x%08lx\n", (unsigned long)h0_result_addr);
    rp2350_write_reg(target, 0, 10, 0xAAAAAAAA);  // a0 = x10
    rp2350_write_reg(target, 0, 11, h0_result_addr);  // a1 = x11

    // Hart 1: Set a0=0x55555555, a1=h1_result_addr
    printf("# Hart 1: Setting a0=0x55555555, a1=0x%08lx\n", (unsigned long)h1_result_addr);
    rp2350_write_reg(target, 1, 10, 0x55555555);  // a0 = x10
    rp2350_write_reg(target, 1, 11, h1_result_addr);  // a1 = x11

    // Set PC on both harts
    printf("# Setting PC to 0x%08lx on both harts...\n", (unsigned long)program_addr);
    swd_error_t err = rp2350_write_pc(target, 0, program_addr);
    if (err != SWD_OK) {
        printf("# Failed to set hart 0 PC: %s\n", swd_error_string(err));
        test_send_response(RESP_FAIL, "Failed to set hart 0 PC");
        return false;
    }

    err = rp2350_write_pc(target, 1, program_addr);
    if (err != SWD_OK) {
        printf("# Failed to set hart 1 PC: %s\n", swd_error_string(err));
        test_send_response(RESP_FAIL, "Failed to set hart 1 PC");
        return false;
    }

    // Resume both harts
    printf("# Resuming both harts...\n");
    rp2350_resume(target, 0);
    rp2350_resume(target, 1);

    // Let them run briefly
    sleep_ms(10);

    // Halt both harts
    printf("# Halting both harts...\n");
    rp2350_halt(target, 0);
    rp2350_halt(target, 1);

    // Read results
    printf("# Reading results...\n");
    swd_result_t h0_result = rp2350_read_mem32(target, h0_result_addr);
    swd_result_t h1_result = rp2350_read_mem32(target, h1_result_addr);

    printf("# Memory at 0x%08lx (hart 0): 0x%08lx (expected 0xAAAAAAAA)\n",
           (unsigned long)h0_result_addr, (unsigned long)h0_result.value);
    printf("# Memory at 0x%08lx (hart 1): 0x%08lx (expected 0x55555555)\n",
           (unsigned long)h1_result_addr, (unsigned long)h1_result.value);

    bool h0_ok = (h0_result.error == SWD_OK && h0_result.value == 0xAAAAAAAA);
    bool h1_ok = (h1_result.error == SWD_OK && h1_result.value == 0x55555555);

    printf("\n# Analysis:\n");
    printf("#   Hart 0 PC write: %s\n", h0_ok ? "✓ WORKS" : "✗ FAILED");
    printf("#   Hart 1 PC write: %s\n", h1_ok ? "✓ WORKS" : "✗ FAILED");

    if (h0_ok && h1_ok) {
        test_send_response(RESP_PASS, NULL);
        return true;
    } else {
        test_send_response(RESP_FAIL, "PC write verification failed");
        return false;
    }
}

//==============================================================================
// Test 21: Read All Hart 1 Registers
//==============================================================================

static bool test_read_all_hart1_regs(swd_target_t *target) {
    printf("# Reading all 32 registers from hart 1...\n");

    rp2350_halt(target, 1);

    uint32_t regs[32];
    swd_error_t err = rp2350_read_all_regs(target, 1, regs);
    if (err != SWD_OK) {
        printf("# Failed to read all hart 1 registers: %s\n", swd_error_string(err));
        test_send_response(RESP_FAIL, swd_error_string(err));
        return false;
    }

    printf("# Successfully read all 32 registers from hart 1\n");
    printf("# Sample: x1=0x%08lx x2=0x%08lx x3=0x%08lx\n",
           (unsigned long)regs[1], (unsigned long)regs[2], (unsigned long)regs[3]);
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test Suite Definition
//==============================================================================

test_case_t hart1_tests[] = {
    { "TEST 17: Halt Hart 1", test_halt_hart1, false, false },
    { "TEST 18: Read Hart 1 PC", test_read_pc_hart1, false, false },
    { "TEST 19: Write/Verify Hart 1 Registers", test_write_verify_hart1_regs, false, false },
    { "TEST 20: PC Write Verification (Both Harts)", test_pc_write_verification, false, false },
    { "TEST 21: Read All Hart 1 Registers", test_read_all_hart1_regs, false, false },
};

const uint32_t hart1_test_count = sizeof(hart1_tests) / sizeof(hart1_tests[0]);
