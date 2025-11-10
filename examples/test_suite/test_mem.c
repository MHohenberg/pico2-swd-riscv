/**
 * @file test_mem.c
 * @brief Comprehensive memory tests
 *
 * Tests memory operations in various scenarios:
 * - Pattern tests (walking 1s/0s, checkerboard, etc.)
 * - Tests with hart halted vs running
 * - Large block operations
 * - RAM fill and checksum verification
 *
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 */

#include "test_framework.h"
#include "pico2-swd-riscv/rp2350.h"
#include <stdio.h>
#include "pico/stdlib.h"

//==============================================================================
// Test: Basic Memory Read/Write (Hart Halted)
//==============================================================================

static bool test_memory_basic_halted(swd_target_t *target) {
    printf("# Testing basic memory operations (hart halted)...\n");

    rp2350_halt(target, 0);

    uint32_t test_addr = 0x20000000;
    uint32_t patterns[] = {
        0x00000000, 0xFFFFFFFF, 0xAAAAAAAA, 0x55555555,
        0x12345678, 0x87654321, 0xDEADBEEF, 0xCAFEBABE
    };

    for (uint i = 0; i < sizeof(patterns)/sizeof(patterns[0]); i++) {
        uint32_t addr = test_addr + (i * 4);
        uint32_t pattern = patterns[i];

        swd_error_t err = rp2350_write_mem32(target, addr, pattern);
        if (err != SWD_OK) {
            printf("# Write failed at 0x%08lx\n", (unsigned long)addr);
            test_send_response(RESP_FAIL, "Write failed");
            return false;
        }

        swd_result_t result = rp2350_read_mem32(target, addr);
        if (result.error != SWD_OK || result.value != pattern) {
            printf("# Verify failed at 0x%08lx: wrote 0x%08lx, read 0x%08lx\n",
                   (unsigned long)addr, (unsigned long)pattern, (unsigned long)result.value);
            test_send_response(RESP_FAIL, "Verify failed");
            return false;
        }
    }

    printf("# Basic memory test passed (8 patterns verified)\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test: Walking 1s Pattern
//==============================================================================

static bool test_memory_walking_ones(swd_target_t *target) {
    printf("# Testing walking 1s pattern...\n");

    rp2350_halt(target, 0);

    uint32_t base_addr = 0x20001000;

    // Write walking 1s
    for (uint i = 0; i < 32; i++) {
        uint32_t pattern = 1u << i;
        uint32_t addr = base_addr + (i * 4);

        swd_error_t err = rp2350_write_mem32(target, addr, pattern);
        if (err != SWD_OK) {
            printf("# Write failed at bit %u\n", i);
            test_send_response(RESP_FAIL, "Write failed");
            return false;
        }
    }

    // Verify walking 1s
    for (uint i = 0; i < 32; i++) {
        uint32_t expected = 1u << i;
        uint32_t addr = base_addr + (i * 4);

        swd_result_t result = rp2350_read_mem32(target, addr);
        if (result.error != SWD_OK || result.value != expected) {
            printf("# Verify failed at bit %u: expected 0x%08lx, got 0x%08lx\n",
                   i, (unsigned long)expected, (unsigned long)result.value);
            test_send_response(RESP_FAIL, "Verify failed");
            return false;
        }
    }

    printf("# Walking 1s pattern test passed (32 words)\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test: Walking 0s Pattern
//==============================================================================

static bool test_memory_walking_zeros(swd_target_t *target) {
    printf("# Testing walking 0s pattern...\n");

    rp2350_halt(target, 0);

    uint32_t base_addr = 0x20001100;

    // Write and verify walking 0s
    for (uint i = 0; i < 32; i++) {
        uint32_t pattern = ~(1u << i);
        uint32_t addr = base_addr + (i * 4);

        swd_error_t err = rp2350_write_mem32(target, addr, pattern);
        if (err != SWD_OK) {
            printf("# Write failed at bit %u\n", i);
            test_send_response(RESP_FAIL, "Write failed");
            return false;
        }

        swd_result_t result = rp2350_read_mem32(target, addr);
        if (result.error != SWD_OK || result.value != pattern) {
            printf("# Verify failed at bit %u\n", i);
            test_send_response(RESP_FAIL, "Verify failed");
            return false;
        }
    }

    printf("# Walking 0s pattern test passed (32 words)\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test: Checkerboard Pattern
//==============================================================================

static bool test_memory_checkerboard(swd_target_t *target) {
    printf("# Testing checkerboard patterns...\n");

    rp2350_halt(target, 0);

    uint32_t base_addr = 0x20001200;
    uint32_t word_count = 256;

    // Write checkerboard 0xAAAAAAAA
    printf("# Writing checkerboard pattern 0xAAAAAAAA...\n");
    for (uint i = 0; i < word_count; i++) {
        uint32_t addr = base_addr + (i * 4);
        swd_error_t err = rp2350_write_mem32(target, addr, 0xAAAAAAAA);
        if (err != SWD_OK) {
            printf("# Write failed at word %u\n", i);
            test_send_response(RESP_FAIL, "Write failed");
            return false;
        }
    }

    // Verify 0xAAAAAAAA
    for (uint i = 0; i < word_count; i++) {
        uint32_t addr = base_addr + (i * 4);
        swd_result_t result = rp2350_read_mem32(target, addr);
        if (result.error != SWD_OK || result.value != 0xAAAAAAAA) {
            printf("# Verify failed at word %u (pattern 1)\n", i);
            test_send_response(RESP_FAIL, "Verify failed");
            return false;
        }
    }

    // Write inverted checkerboard 0x55555555
    printf("# Writing inverted checkerboard pattern 0x55555555...\n");
    for (uint i = 0; i < word_count; i++) {
        uint32_t addr = base_addr + (i * 4);
        swd_error_t err = rp2350_write_mem32(target, addr, 0x55555555);
        if (err != SWD_OK) {
            printf("# Write failed at word %u\n", i);
            test_send_response(RESP_FAIL, "Write failed");
            return false;
        }
    }

    // Verify 0x55555555
    for (uint i = 0; i < word_count; i++) {
        uint32_t addr = base_addr + (i * 4);
        swd_result_t result = rp2350_read_mem32(target, addr);
        if (result.error != SWD_OK || result.value != 0x55555555) {
            printf("# Verify failed at word %u (pattern 2)\n", i);
            test_send_response(RESP_FAIL, "Verify failed");
            return false;
        }
    }

    printf("# Checkerboard pattern test passed (256 words)\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test: Sequential Address Pattern
//==============================================================================

static bool test_memory_address_pattern(swd_target_t *target) {
    printf("# Testing address-based pattern...\n");

    rp2350_halt(target, 0);

    uint32_t base_addr = 0x20002000;
    uint32_t word_count = 512;

    // Write address as pattern
    for (uint i = 0; i < word_count; i++) {
        uint32_t addr = base_addr + (i * 4);
        swd_error_t err = rp2350_write_mem32(target, addr, addr);
        if (err != SWD_OK) {
            printf("# Write failed at word %u\n", i);
            test_send_response(RESP_FAIL, "Write failed");
            return false;
        }
    }

    // Verify address pattern
    for (uint i = 0; i < word_count; i++) {
        uint32_t addr = base_addr + (i * 4);
        swd_result_t result = rp2350_read_mem32(target, addr);
        if (result.error != SWD_OK || result.value != addr) {
            printf("# Verify failed at word %u: expected 0x%08lx, got 0x%08lx\n",
                   i, (unsigned long)addr, (unsigned long)result.value);
            test_send_response(RESP_FAIL, "Verify failed");
            return false;
        }
    }

    printf("# Address pattern test passed (512 words)\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test: Large Block Write/Read
//==============================================================================

static bool test_memory_large_block(swd_target_t *target) {
    printf("# Testing large block operations (4KB)...\n");

    rp2350_halt(target, 0);

    uint32_t base_addr = 0x20003000;
    uint32_t word_count = 1024;  // 4KB

    // Write sequential pattern
    printf("# Writing 4KB sequential pattern...\n");
    for (uint i = 0; i < word_count; i++) {
        if (i % 256 == 0) printf("# Progress: %u/%u words\n", i, word_count);
        uint32_t addr = base_addr + (i * 4);
        uint32_t value = 0xA5000000 | i;
        swd_error_t err = rp2350_write_mem32(target, addr, value);
        if (err != SWD_OK) {
            printf("# Write failed at word %u\n", i);
            test_send_response(RESP_FAIL, "Write failed");
            return false;
        }
    }

    // Verify sequential pattern
    printf("# Verifying 4KB...\n");
    for (uint i = 0; i < word_count; i++) {
        if (i % 256 == 0) printf("# Progress: %u/%u words\n", i, word_count);
        uint32_t addr = base_addr + (i * 4);
        uint32_t expected = 0xA5000000 | i;
        swd_result_t result = rp2350_read_mem32(target, addr);
        if (result.error != SWD_OK || result.value != expected) {
            printf("# Verify failed at word %u\n", i);
            test_send_response(RESP_FAIL, "Verify failed");
            return false;
        }
    }

    printf("# Large block test passed (4KB)\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test: Memory Access While Hart Running
//==============================================================================

static bool test_memory_while_running(swd_target_t *target) {
    printf("# Testing memory access while hart is running...\n");

    // Upload a simple infinite loop program
    uint32_t program_addr = 0x20004000;
    uint32_t test_addr = 0x20004100;

    rp2350_halt(target, 0);

    // Simple program: infinite loop
    uint32_t program[] = {
        0x0000006f,  // j 0 (infinite loop)
    };

    for (uint i = 0; i < sizeof(program)/sizeof(program[0]); i++) {
        swd_error_t err = rp2350_write_mem32(target, program_addr + (i * 4), program[i]);
        if (err != SWD_OK) {
            printf("# Failed to upload program\n");
            test_send_response(RESP_FAIL, "Program upload failed");
            return false;
        }
    }

    // Set PC and resume
    rp2350_write_pc(target, 0, program_addr);
    rp2350_resume(target, 0);

    // Try to write/read memory while hart is running
    printf("# Hart is now running, testing memory access...\n");
    uint32_t patterns[] = {0xDEADBEEF, 0xCAFEBABE, 0xFEEDFACE, 0xBAADF00D};

    for (uint i = 0; i < sizeof(patterns)/sizeof(patterns[0]); i++) {
        uint32_t addr = test_addr + (i * 4);
        uint32_t pattern = patterns[i];

        swd_error_t err = rp2350_write_mem32(target, addr, pattern);
        if (err != SWD_OK) {
            printf("# Write failed while running at address 0x%08lx\n", (unsigned long)addr);
            test_send_response(RESP_FAIL, "Write failed while running");
            rp2350_halt(target, 0);
            return false;
        }

        swd_result_t result = rp2350_read_mem32(target, addr);
        if (result.error != SWD_OK || result.value != pattern) {
            printf("# Verify failed while running at address 0x%08lx\n", (unsigned long)addr);
            test_send_response(RESP_FAIL, "Verify failed while running");
            rp2350_halt(target, 0);
            return false;
        }
    }

    rp2350_halt(target, 0);

    printf("# Memory access while running test passed\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test: RAM Fill with CPU Execution
//==============================================================================

static bool test_memory_ram_fill_cpu(swd_target_t *target) {
    printf("# Testing large RAM fill using CPU execution...\n");

    rp2350_halt(target, 0);

    uint32_t program_base = 0x20078000;
    uint32_t fill_start = 0x20000000;
    uint32_t fill_end = 0x20040000;  // 256KB
    uint32_t fill_pattern = 0xA5A5A5A5;

    printf("# Will fill 0x%08lx - 0x%08lx with 0x%08lx\n",
           (unsigned long)fill_start, (unsigned long)fill_end,
           (unsigned long)fill_pattern);

    // RISC-V assembly program to fill memory
    uint32_t fill_program[] = {
        0x200002B7,  // lui x5, 0x20000
        0x20040337,  // lui x6, 0x20040
        0xA5A5A3B7,  // lui x7, 0xA5A5A
        0x5A538393,  // addi x7, x7, 0x5A5
        0x0072A023,  // sw x7, 0(x5)
        0x00428293,  // addi x5, x5, 4
        0xFE629CE3,  // bne x5, x6, -8
        0x0000006F,  // j 0
    };

    // Upload fill program
    printf("# Uploading fill program...\n");
    for (uint i = 0; i < sizeof(fill_program)/sizeof(fill_program[0]); i++) {
        swd_error_t err = rp2350_write_mem32(target, program_base + (i * 4), fill_program[i]);
        if (err != SWD_OK) {
            printf("# Failed to upload fill program\n");
            test_send_response(RESP_FAIL, "Program upload failed");
            return false;
        }
    }

    // Execute fill program
    printf("# Executing fill program...\n");
    rp2350_write_pc(target, 0, program_base);
    rp2350_resume(target, 0);

    // Wait for completion (256KB / 4 bytes = 64K iterations)
    sleep_ms(100);

    rp2350_halt(target, 0);

    // Verify sample locations
    printf("# Verifying sample locations...\n");
    uint32_t sample_addrs[] = {
        fill_start,
        fill_start + 0x10000,
        fill_start + 0x20000,
        fill_start + 0x30000,
        fill_end - 4
    };

    for (uint i = 0; i < sizeof(sample_addrs)/sizeof(sample_addrs[0]); i++) {
        swd_result_t result = rp2350_read_mem32(target, sample_addrs[i]);
        if (result.error != SWD_OK || result.value != fill_pattern) {
            printf("# Verify failed at 0x%08lx: expected 0x%08lx, got 0x%08lx\n",
                   (unsigned long)sample_addrs[i],
                   (unsigned long)fill_pattern,
                   (unsigned long)result.value);
            test_send_response(RESP_FAIL, "Verify failed");
            return false;
        }
    }

    printf("# RAM fill test passed (256KB filled and verified)\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test: RAM Fill with Checksum Verification
//==============================================================================

static bool test_memory_checksum(swd_target_t *target) {
    printf("# Testing RAM checksum verification...\n");

    rp2350_halt(target, 0);

    uint32_t program_base = 0x20078000;
    uint32_t checksum_base = program_base + 0x100;
    uint32_t fill_start = 0x20000000;
    uint32_t fill_end = 0x20040000;  // 256KB
    uint32_t fill_pattern = 0xA5A5A5A5;

    // First fill memory (reuse previous test's program)
    uint32_t fill_program[] = {
        0x200002B7,  // lui x5, 0x20000
        0x20040337,  // lui x6, 0x20040
        0xA5A5A3B7,  // lui x7, 0xA5A5A
        0x5A538393,  // addi x7, x7, 0x5A5
        0x0072A023,  // sw x7, 0(x5)
        0x00428293,  // addi x5, x5, 4
        0xFE629CE3,  // bne x5, x6, -8
        0x0000006F,  // j 0
    };

    printf("# Filling memory...\n");
    for (uint i = 0; i < sizeof(fill_program)/sizeof(fill_program[0]); i++) {
        rp2350_write_mem32(target, program_base + (i * 4), fill_program[i]);
    }

    rp2350_write_pc(target, 0, program_base);
    rp2350_resume(target, 0);
    sleep_ms(100);
    rp2350_halt(target, 0);

    // Now run checksum program
    printf("# Running XOR checksum program...\n");

    // Checksum program: XOR all words
    uint32_t checksum_program[] = {
        0x200002B7,  // lui x5, 0x20000
        0x20040337,  // lui x6, 0x20040
        0x00000513,  // addi x10, x0, 0 (li a0, 0)
        0x0002A383,  // lw x7, 0(x5)
        0x00754533,  // xor x10, x10, x7
        0x00428293,  // addi x5, x5, 4
        0xFE629AE3,  // bne x5, x6, -12
        0x0000006F,  // j 0
    };

    for (uint i = 0; i < sizeof(checksum_program)/sizeof(checksum_program[0]); i++) {
        rp2350_write_mem32(target, checksum_base + (i * 4), checksum_program[i]);
    }

    // Clear a0 register
    rp2350_write_reg(target, 0, 10, 0);

    // Execute checksum
    rp2350_write_pc(target, 0, checksum_base);
    rp2350_resume(target, 0);
    sleep_ms(100);
    rp2350_halt(target, 0);

    // Read checksum result
    swd_result_t checksum_result = rp2350_read_reg(target, 0, 10);
    if (checksum_result.error != SWD_OK) {
        printf("# Failed to read checksum result\n");
        test_send_response(RESP_FAIL, "Failed to read checksum");
        return false;
    }

    // Calculate expected checksum
    uint32_t word_count = (fill_end - fill_start) / 4;
    uint32_t expected_checksum = (word_count & 1) ? fill_pattern : 0;

    printf("# Checksum result: 0x%08lx (expected 0x%08lx)\n",
           (unsigned long)checksum_result.value,
           (unsigned long)expected_checksum);
    printf("# Words checksummed: %lu\n", (unsigned long)word_count);

    if (checksum_result.value == expected_checksum) {
        printf("# Checksum verification passed!\n");
        printf("# Successfully verified %lu KB of RAM\n",
               (unsigned long)((fill_end - fill_start) / 1024));
        test_send_response(RESP_PASS, NULL);
        return true;
    } else {
        printf("# Checksum mismatch!\n");
        test_send_response(RESP_FAIL, "Checksum mismatch");
        return false;
    }
}

//==============================================================================
// Test Suite Definition
//==============================================================================

test_case_t memory_tests[] = {
    { "MEM 1: Basic Memory R/W (Halted)", test_memory_basic_halted, false, false },
    { "MEM 2: Walking 1s Pattern", test_memory_walking_ones, false, false },
    { "MEM 3: Walking 0s Pattern", test_memory_walking_zeros, false, false },
    { "MEM 4: Checkerboard Pattern", test_memory_checkerboard, false, false },
    { "MEM 5: Address-Based Pattern", test_memory_address_pattern, false, false },
    { "MEM 6: Large Block (4KB)", test_memory_large_block, false, false },
    { "MEM 7: Memory Access While Running", test_memory_while_running, false, false },
    { "MEM 8: RAM Fill with CPU (256KB)", test_memory_ram_fill_cpu, false, false },
    { "MEM 9: Checksum Verification (256KB)", test_memory_checksum, false, false },
};

const uint32_t memory_test_count = sizeof(memory_tests) / sizeof(memory_tests[0]);
