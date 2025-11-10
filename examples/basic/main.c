/**
 * @file main.c
 * @brief Basic example of using pico2-swd-riscv library
 *
 * This example demonstrates:
 * - Connecting to an RP2350 target
 * - Initializing the Debug Module
 * - Halting and resuming the hart
 * - Reading registers and PC
 * - Reading memory
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include <pico2-swd-riscv/swd.h>
#include <pico2-swd-riscv/rp2350.h>

int main() {
    // Initialize stdio (for printf over USB)
    stdio_init_all();
    sleep_ms(2000);  // Wait for USB

    printf("\n====================================\n");
    printf("pico2-swd-riscv Basic Example\n");
    printf("====================================\n\n");

    // Configure the SWD target
    swd_config_t config = swd_config_default();
    config.pin_swclk = 2;  // Change to your SWCLK pin
    config.pin_swdio = 3;  // Change to your SWDIO pin
    config.freq_khz = 1000;  // 1 MHz SWCLK
    config.enable_caching = true;

    printf("Creating target (SWCLK=GPIO%d, SWDIO=GPIO%d)...\n",
           config.pin_swclk, config.pin_swdio);

    // Create target handle
    swd_target_t *target = swd_target_create(&config);
    if (!target) {
        printf("ERROR: Failed to create target\n");
        printf("  Make sure PIO/SM resources are available\n");
        return 1;
    }

    printf("Target created successfully\n\n");

    // Connect to target
    printf("Connecting to target...\n");
    swd_error_t err = swd_connect(target);
    if (err != SWD_OK) {
        printf("ERROR: Connection failed: %s\n", swd_error_string(err));
        printf("  Details: %s\n", swd_get_last_error_detail(target));
        printf("\nTroubleshooting:\n");
        printf("  - Check wiring (SWCLK, SWDIO, GND)\n");
        printf("  - Ensure target is powered\n");
        printf("  - Try lower frequency (100 kHz)\n");
        swd_target_destroy(target);
        return 1;
    }

    printf("Connected successfully!\n");
    printf("  %s\n\n", swd_get_target_info(target));

    // Initialize RP2350 Debug Module
    printf("Initializing RP2350 Debug Module...\n");
    err = rp2350_init(target);
    if (err != SWD_OK) {
        printf("ERROR: Debug Module init failed: %s\n", swd_error_string(err));
        goto cleanup;
    }

    printf("Debug Module initialized\n\n");

    // Halt the hart (hart 0)
    printf("Halting hart 0...\n");
    err = rp2350_halt(target, 0);
    if (err == SWD_ERROR_ALREADY_HALTED) {
        printf("Hart 0 was already halted\n");
    } else if (err != SWD_OK) {
        printf("ERROR: Failed to halt: %s\n", swd_error_string(err));
        goto cleanup;
    } else {
        printf("Hart 0 halted successfully\n");
    }

    printf("\n");

    // Read Program Counter
    printf("Reading Program Counter (hart 0)...\n");
    swd_result_t pc = rp2350_read_pc(target, 0);
    if (pc.error == SWD_OK) {
        printf("  PC = 0x%08x\n", pc.value);
    } else {
        printf("  ERROR: %s\n", swd_error_string(pc.error));
    }

    printf("\n");

    // Read some general purpose registers
    printf("Reading registers (hart 0)...\n");
    for (uint8_t i = 1; i <= 5; i++) {
        swd_result_t reg = rp2350_read_reg(target, 0, i);
        if (reg.error == SWD_OK) {
            printf("  x%d  = 0x%08x\n", i, reg.value);
        } else {
            printf("  x%d  = ERROR: %s\n", i, swd_error_string(reg.error));
        }
    }

    printf("\n");

    // Read all registers at once (more efficient)
    printf("Reading all 32 registers (hart 0)...\n");
    uint32_t regs[32];
    err = rp2350_read_all_regs(target, 0, regs);
    if (err == SWD_OK) {
        for (int i = 0; i < 32; i += 4) {
            printf("  x%2d=0x%08x  x%2d=0x%08x  x%2d=0x%08x  x%2d=0x%08x\n",
                   i, regs[i], i+1, regs[i+1], i+2, regs[i+2], i+3, regs[i+3]);
        }
    } else {
        printf("  ERROR: %s\n", swd_error_string(err));
    }

    printf("\n");

    // Read some memory
    printf("Reading memory at 0x20000000...\n");
    for (uint32_t addr = 0x20000000; addr < 0x20000010; addr += 4) {
        swd_result_t mem = rp2350_read_mem32(target, addr);
        if (mem.error == SWD_OK) {
            printf("  [0x%08x] = 0x%08x\n", addr, mem.value);
        } else {
            printf("  [0x%08x] = ERROR: %s\n", addr, swd_error_string(mem.error));
        }
    }

    printf("\n");

    // Resume the hart
    printf("Resuming hart 0...\n");
    err = rp2350_resume(target, 0);
    if (err == SWD_OK) {
        printf("Hart 0 resumed successfully\n");
    } else {
        printf("ERROR: Failed to resume: %s\n", swd_error_string(err));
    }

cleanup:
    printf("\n");
    printf("Cleaning up and disconnecting...\n");
    swd_target_destroy(target);
    printf("Done!\n");

    return 0;
}
