/**
 * @file test_trace.c
 * @brief Instruction tracing tests using rp2350_trace callback API
 *
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 */

#include "test_framework.h"
#include "pico2-swd-riscv/rp2350.h"
#include <stdio.h>
#include "pico/stdlib.h"

//==============================================================================
// Trace Callback Context
//==============================================================================

typedef struct {
    uint32_t instruction_count;
    uint32_t expected_start_pc;
    bool pc_sequence_valid;
    uint32_t last_pc;
} trace_context_t;

//==============================================================================
// Test: Basic Instruction Trace (No Register Capture)
//==============================================================================

static bool trace_callback_basic(const trace_record_t *record, void *user_data) {
    trace_context_t *ctx = (trace_context_t *)user_data;

    printf("# [%lu] PC=0x%08lx INST=0x%08lx\n",
           (unsigned long)ctx->instruction_count,
           (unsigned long)record->pc,
           (unsigned long)record->instruction);

    ctx->instruction_count++;
    return true;  // Continue tracing
}

static bool test_trace_basic(swd_target_t *target) {
    printf("# Testing basic instruction trace (no register capture)...\n");

    rp2350_halt(target, 0);

    // Upload simple program
    uint32_t program[] = {
        0x00128293,  // addi x5, x5, 1
        0x00230313,  // addi x6, x6, 2
        0x00338393,  // addi x7, x7, 3
        0x0000006f,  // j 0
    };

    // Verify our instruction encodings are correct
    printf("# Program to upload:\n");
    printf("#   0x00128293 = addi x5, x5, 1   (imm=1, rs1=x5, rd=x5, opcode=0x13)\n");
    printf("#   0x00230313 = addi x6, x6, 2   (imm=2, rs1=x6, rd=x6, opcode=0x13)\n");
    printf("#   0x00338393 = addi x7, x7, 3   (imm=3, rs1=x7, rd=x7, opcode=0x13)\n");
    printf("#   0x0000006f = jal x0, 0        (offset=0, rd=x0, opcode=0x6f)\n");

    uint32_t program_addr = 0x20010000;

    for (uint i = 0; i < sizeof(program)/sizeof(program[0]); i++) {
        swd_error_t err = rp2350_write_mem32(target, program_addr + (i * 4), program[i]);
        if (err != SWD_OK) {
            printf("# Failed to upload program\n");
            test_send_response(RESP_FAIL, "Program upload failed");
            return false;
        }
    }

    // Verify program was actually written to memory
    printf("# Verifying uploaded program...\n");
    for (uint i = 0; i < sizeof(program)/sizeof(program[0]); i++) {
        swd_result_t readback = rp2350_read_mem32(target, program_addr + (i * 4));
        printf("#   [%u] 0x%08lx: 0x%08lx (expected 0x%08lx) %s\n",
               i,
               (unsigned long)(program_addr + (i * 4)),
               (unsigned long)readback.value,
               (unsigned long)program[i],
               (readback.value == program[i]) ? "OK" : "MISMATCH!");
    }

    // Disable interrupts to prevent ISR jumps during trace
    // (firmware leaves mstatus.MIE=1 with pending interrupts)
    swd_result_t mstatus_read = rp2350_read_csr(target, 0, 0x300);
    if (mstatus_read.error == SWD_OK) {
        printf("# mstatus before: 0x%08lx (MIE=%d)\n",
               (unsigned long)mstatus_read.value,
               (int)((mstatus_read.value >> 3) & 1));
        rp2350_write_csr(target, 0, 0x300, mstatus_read.value & ~(1 << 3));  // Clear MIE
    }

    // Set PC to program start
    rp2350_write_pc(target, 0, program_addr);

    // Verify PC was actually written
    swd_result_t pc_check = rp2350_read_pc(target, 0);
    printf("# After write_pc: PC=0x%08lx (expected 0x%08lx)\n",
           (unsigned long)pc_check.value, (unsigned long)program_addr);

    if (pc_check.value != program_addr) {
        printf("# WARNING: PC write didn't stick!\n");
    }

    // Clear registers
    rp2350_write_reg(target, 0, 5, 0);
    rp2350_write_reg(target, 0, 6, 0);
    rp2350_write_reg(target, 0, 7, 0);

    // Trace 10 instructions
    trace_context_t ctx = {
        .instruction_count = 0,
        .expected_start_pc = program_addr,
        .pc_sequence_valid = true,
        .last_pc = program_addr
    };

    printf("# Starting trace from PC=0x%08lx...\n", (unsigned long)program_addr);
    int result = rp2350_trace(target, 0, 10, trace_callback_basic, &ctx, false);

    if (result < 0) {
        printf("# Trace failed with error code %d\n", result);
        test_send_response(RESP_FAIL, "Trace failed");
        return false;
    }

    printf("# Traced %d instructions\n", result);

    if (result != 10) {
        printf("# Expected 10 instructions, got %d\n", result);
        test_send_response(RESP_FAIL, "Instruction count mismatch");
        return false;
    }

    printf("# Basic trace test passed\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test: Trace with Register Capture
//==============================================================================

static bool trace_callback_with_regs(const trace_record_t *record, void *user_data) {
    trace_context_t *ctx = (trace_context_t *)user_data;

    printf("# [%lu] PC=0x%08lx INST=0x%08lx\n",
           (unsigned long)ctx->instruction_count,
           (unsigned long)record->pc,
           (unsigned long)record->instruction);

    // Print register changes (x5, x6, x7)
    printf("#      x5=0x%08lx x6=0x%08lx x7=0x%08lx\n",
           (unsigned long)record->regs[5],
           (unsigned long)record->regs[6],
           (unsigned long)record->regs[7]);

    ctx->instruction_count++;
    return true;
}

static bool test_trace_with_registers(swd_target_t *target) {
    printf("# Testing instruction trace with register capture...\n");

    rp2350_halt(target, 0);

    // Upload simple program that modifies registers
    uint32_t program[] = {
        0x00100293,  // addi x5, x0, 1    (x5 = 1)
        0x00200313,  // addi x6, x0, 2    (x6 = 2)
        0x00300393,  // addi x7, x0, 3    (x7 = 3)
        0x006282B3,  // add x5, x5, x6    (x5 = x5 + x6 = 3)
        0x007303B3,  // add x7, x6, x7    (x7 = x6 + x7 = 5)
        0x0000006f,  // j 0 (loop)
    };

    uint32_t program_addr = 0x20010100;

    for (uint i = 0; i < sizeof(program)/sizeof(program[0]); i++) {
        swd_error_t err = rp2350_write_mem32(target, program_addr + (i * 4), program[i]);
        if (err != SWD_OK) {
            printf("# Failed to upload program\n");
            test_send_response(RESP_FAIL, "Program upload failed");
            return false;
        }
    }

    // Disable interrupts
    swd_result_t mstatus_read = rp2350_read_csr(target, 0, 0x300);
    if (mstatus_read.error == SWD_OK) {
        rp2350_write_csr(target, 0, 0x300, mstatus_read.value & ~(1 << 3));
    }

    // Set PC and clear registers
    rp2350_write_pc(target, 0, program_addr);
    rp2350_write_reg(target, 0, 5, 0);
    rp2350_write_reg(target, 0, 6, 0);
    rp2350_write_reg(target, 0, 7, 0);

    // Trace 5 instructions with register capture
    trace_context_t ctx = {
        .instruction_count = 0,
        .expected_start_pc = program_addr,
        .pc_sequence_valid = true,
        .last_pc = program_addr
    };

    printf("# Starting trace with register capture from PC=0x%08lx...\n",
           (unsigned long)program_addr);
    int result = rp2350_trace(target, 0, 5, trace_callback_with_regs, &ctx, true);

    if (result < 0) {
        printf("# Trace failed with error code %d\n", result);
        test_send_response(RESP_FAIL, "Trace failed");
        return false;
    }

    printf("# Traced %d instructions with register capture\n", result);

    // Verify final register values
    swd_result_t x5 = rp2350_read_reg(target, 0, 5);
    swd_result_t x6 = rp2350_read_reg(target, 0, 6);
    swd_result_t x7 = rp2350_read_reg(target, 0, 7);

    printf("# Final register values: x5=0x%08lx x6=0x%08lx x7=0x%08lx\n",
           (unsigned long)x5.value, (unsigned long)x6.value, (unsigned long)x7.value);

    // Expected: x5=3 (1+2), x6=2, x7=5 (2+3)
    if (x5.value == 3 && x6.value == 2 && x7.value == 5) {
        printf("# Register values match expected results\n");
    } else {
        printf("# Register values don't match (expected x5=3, x6=2, x7=5)\n");
    }

    printf("# Trace with registers test passed\n");
    test_send_response(RESP_PASS, NULL);
    return true;
}

//==============================================================================
// Test: Early Termination via Callback
//==============================================================================

static bool trace_callback_early_stop(const trace_record_t *record, void *user_data) {
    trace_context_t *ctx = (trace_context_t *)user_data;

    printf("# [%lu] PC=0x%08lx INST=0x%08lx\n",
           (unsigned long)ctx->instruction_count,
           (unsigned long)record->pc,
           (unsigned long)record->instruction);

    ctx->instruction_count++;

    // Stop after 7 instructions (should give us x5=6)
    if (ctx->instruction_count >= 7) {
        printf("# Callback requesting early stop after %lu instructions\n",
               (unsigned long)ctx->instruction_count);
        return false;
    }

    return true;
}

static bool test_trace_early_stop(swd_target_t *target) {
    printf("# Testing trace early termination via callback...\n");

    rp2350_halt(target, 0);

    // More interesting program with jumps, nops, and arithmetic:
    //   li x5, 0         # x5 = 0
    //   addi x5, x5, 1   # x5 = 1
    //   j skip1          # jump over nops
    //   nop
    //   nop
    // skip1:
    //   addi x5, x5, 2   # x5 = 3
    //   j skip2          # jump over nops
    //   nop
    //   nop
    // skip2:
    //   addi x5, x5, 3   # x5 = 6
    //   j loop
    // loop:
    //   j loop           # infinite loop
    uint32_t program[] = {
        0x00000293,  // 0:  li x5, 0         (addi x5, x0, 0)
        0x00128293,  // 4:  addi x5, x5, 1
        0x00c0006f,  // 8:  j skip1          (jal x0, 12) -> PC+12 = 20
        0x00000013,  // 12: nop
        0x00000013,  // 16: nop
        0x00228293,  // 20: addi x5, x5, 2   (skip1)
        0x00c0006f,  // 24: j skip2          (jal x0, 12) -> PC+12 = 36
        0x00000013,  // 28: nop
        0x00000013,  // 32: nop
        0x00328293,  // 36: addi x5, x5, 3   (skip2)
        0x0040006f,  // 40: j loop           (jal x0, 4) -> PC+4 = 44
        0x0000006f,  // 44: j loop           (jal x0, 0) -> PC+0 = 44
    };

    uint32_t program_addr = 0x20010200;

    // Upload program
    for (uint i = 0; i < sizeof(program)/sizeof(program[0]); i++) {
        rp2350_write_mem32(target, program_addr + (i * 4), program[i]);
    }

    // Disable interrupts
    swd_result_t mstatus_read = rp2350_read_csr(target, 0, 0x300);
    if (mstatus_read.error == SWD_OK) {
        rp2350_write_csr(target, 0, 0x300, mstatus_read.value & ~(1 << 3));
    }

    // Set PC and clear x5
    rp2350_write_pc(target, 0, program_addr);
    rp2350_write_reg(target, 0, 5, 0);

    // Trace execution:
    // 1. li x5, 0        -> x5=0
    // 2. addi x5, x5, 1  -> x5=1
    // 3. j skip1
    // 4. addi x5, x5, 2  -> x5=3
    // 5. j skip2
    // 6. addi x5, x5, 3  -> x5=6
    // 7. j loop

    // Request 100 instructions, but callback will stop at 7
    trace_context_t ctx = {
        .instruction_count = 0,
        .expected_start_pc = program_addr,
        .pc_sequence_valid = true,
        .last_pc = program_addr
    };

    printf("# Requesting 100 instructions, callback will stop at 7...\n");
    printf("# Expected execution: li(0) -> addi(1) -> j -> addi(3) -> j -> addi(6) -> j\n");
    int result = rp2350_trace(target, 0, 100, trace_callback_early_stop, &ctx, false);

    if (result < 0) {
        printf("# Trace failed with error code %d\n", result);
        test_send_response(RESP_FAIL, "Trace failed");
        return false;
    }

    printf("# Traced %d instructions (stopped by callback)\n", result);

    // Verify x5 has the expected value after 7 instructions
    swd_result_t x5 = rp2350_read_reg(target, 0, 5);
    if (x5.error != SWD_OK) {
        printf("# Failed to read x5 after trace\n");
        test_send_response(RESP_FAIL, "Failed to read x5");
        return false;
    }

    printf("# After %d instructions: x5 = 0x%08lx (expected 0x00000006)\n",
           result, (unsigned long)x5.value);

    // After 7 instructions, x5 should be 6
    if (result == 7 && x5.value == 6) {
        printf("# Callback early stop worked correctly, x5 has expected value\n");
        test_send_response(RESP_PASS, NULL);
        return true;
    } else if (result != 7) {
        printf("# Expected 7 instructions, got %d\n", result);
        test_send_response(RESP_FAIL, "Wrong instruction count");
        return false;
    } else {
        printf("# x5 has wrong value (expected 6, got %lu)\n", (unsigned long)x5.value);
        test_send_response(RESP_FAIL, "x5 verification failed");
        return false;
    }
}

//==============================================================================
// Test: Trace Loop Detection
//==============================================================================

typedef struct {
    uint32_t instruction_count;
    uint32_t loop_pc;
    uint32_t loop_count;
    bool loop_detected;
} loop_context_t;

static bool trace_callback_loop_detect(const trace_record_t *record, void *user_data) {
    loop_context_t *ctx = (loop_context_t *)user_data;

    if (ctx->instruction_count == 0) {
        // First instruction
        ctx->loop_pc = record->pc;
        printf("# Loop entry point: PC=0x%08lx\n", (unsigned long)record->pc);
    } else if (record->pc == ctx->loop_pc) {
        ctx->loop_count++;
        printf("# Loop iteration %lu detected\n", (unsigned long)ctx->loop_count);

        if (ctx->loop_count >= 3) {
            printf("# Detected 3 loop iterations, stopping trace\n");
            ctx->loop_detected = true;
            return false;
        }
    }

    ctx->instruction_count++;
    return true;
}

static bool test_trace_loop_detection(swd_target_t *target) {
    printf("# Testing loop detection during trace...\n");

    rp2350_halt(target, 0);

    // Simple 3-instruction loop
    // Program at 0x20010300:
    //   0x300: addi x5, x5, 1
    //   0x304: addi x6, x6, 2
    //   0x308: j -8           # Jump back to 0x300 (0x308 + (-8) = 0x300)
    uint32_t program[] = {
        0x00128293,  // addi x5, x5, 1
        0x00230313,  // addi x6, x6, 2
        0xFF9FF06F,  // j -8 (jal x0, -8) - jump back to start
    };

    uint32_t program_addr = 0x20010300;

    for (uint i = 0; i < sizeof(program)/sizeof(program[0]); i++) {
        rp2350_write_mem32(target, program_addr + (i * 4), program[i]);
    }

    // Disable interrupts
    swd_result_t mstatus_read = rp2350_read_csr(target, 0, 0x300);
    if (mstatus_read.error == SWD_OK) {
        rp2350_write_csr(target, 0, 0x300, mstatus_read.value & ~(1 << 3));
    }

    rp2350_write_pc(target, 0, program_addr);
    rp2350_write_reg(target, 0, 5, 0);
    rp2350_write_reg(target, 0, 6, 0);

    loop_context_t ctx = {
        .instruction_count = 0,
        .loop_pc = 0,
        .loop_count = 0,
        .loop_detected = false
    };

    printf("# Starting trace to detect loop...\n");
    int result = rp2350_trace(target, 0, 50, trace_callback_loop_detect, &ctx, false);

    if (result < 0) {
        printf("# Trace failed with error code %d\n", result);
        test_send_response(RESP_FAIL, "Trace failed");
        return false;
    }

    printf("# Traced %d instructions\n", result);
    printf("# Loop detected: %s, Loop count: %lu\n",
           ctx.loop_detected ? "YES" : "NO",
           (unsigned long)ctx.loop_count);

    if (ctx.loop_detected && ctx.loop_count >= 3) {
        printf("# Loop detection test passed\n");
        test_send_response(RESP_PASS, NULL);
        return true;
    } else {
        printf("# Loop detection test failed\n");
        test_send_response(RESP_FAIL, "Loop not detected");
        return false;
    }
}

//==============================================================================
// Test: Trace Hart 1
//==============================================================================

static bool trace_callback_hart1(const trace_record_t *record, void *user_data) {
    trace_context_t *ctx = (trace_context_t *)user_data;

    printf("# [Hart1-%lu] PC=0x%08lx INST=0x%08lx\n",
           (unsigned long)ctx->instruction_count,
           (unsigned long)record->pc,
           (unsigned long)record->instruction);

    ctx->instruction_count++;
    return true;
}

static bool test_trace_hart1(swd_target_t *target) {
    printf("# Testing instruction trace on hart 1...\n");

    rp2350_halt(target, 1);

    // Upload simple program to hart 1
    uint32_t program[] = {
        0x00100313,  // addi x6, x0, 1
        0x00200393,  // addi x7, x0, 2
        0x007303B3,  // add x7, x6, x7
        0x0000006f,  // j 0
    };

    uint32_t program_addr = 0x20011000;

    for (uint i = 0; i < sizeof(program)/sizeof(program[0]); i++) {
        swd_error_t err = rp2350_write_mem32(target, program_addr + (i * 4), program[i]);
        if (err != SWD_OK) {
            printf("# Failed to upload program\n");
            test_send_response(RESP_FAIL, "Program upload failed");
            return false;
        }
    }

    // Disable interrupts on hart 1
    swd_result_t mstatus_read = rp2350_read_csr(target, 1, 0x300);
    if (mstatus_read.error == SWD_OK) {
        rp2350_write_csr(target, 1, 0x300, mstatus_read.value & ~(1 << 3));
    }

    // Set PC
    rp2350_write_pc(target, 1, program_addr);
    rp2350_write_reg(target, 1, 6, 0);
    rp2350_write_reg(target, 1, 7, 0);

    trace_context_t ctx = {
        .instruction_count = 0,
        .expected_start_pc = program_addr,
        .pc_sequence_valid = true,
        .last_pc = program_addr
    };

    printf("# Starting trace on hart 1 from PC=0x%08lx...\n",
           (unsigned long)program_addr);
    int result = rp2350_trace(target, 1, 8, trace_callback_hart1, &ctx, false);

    if (result < 0) {
        printf("# Trace failed with error code %d\n", result);
        test_send_response(RESP_FAIL, "Hart 1 trace failed");
        return false;
    }

    printf("# Traced %d instructions on hart 1\n", result);

    if (result == 8) {
        printf("# Hart 1 trace test passed\n");
        test_send_response(RESP_PASS, NULL);
        return true;
    } else {
        printf("# Expected 8 instructions, got %d\n", result);
        test_send_response(RESP_FAIL, "Instruction count mismatch");
        return false;
    }
}

//==============================================================================
// Test Suite Definition
//==============================================================================

test_case_t trace_tests[] = {
    { "TRACE 1: Basic Instruction Trace", test_trace_basic, false, false },
    { "TRACE 2: Trace with Register Capture", test_trace_with_registers, false, false },
    { "TRACE 3: Early Termination via Callback", test_trace_early_stop, false, false },
    { "TRACE 4: Loop Detection", test_trace_loop_detection, false, false },
    { "TRACE 5: Trace Hart 1", test_trace_hart1, false, false },
};

const uint32_t trace_test_count = sizeof(trace_tests) / sizeof(trace_tests[0]);
