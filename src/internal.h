/**
 * @file internal.h
 * @brief Internal structures and helpers (not public API)
 *
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 */

#ifndef PICO2_SWD_RISCV_INTERNAL_H
#define PICO2_SWD_RISCV_INTERNAL_H

#include "pico2-swd-riscv/types.h"
#include "hardware/pio.h"
//==============================================================================
// Debug Logging
//==============================================================================

#ifndef PICO2_SWD_DEBUG_LEVEL
#define PICO2_SWD_DEBUG_LEVEL 1
#endif

#if PICO2_SWD_DEBUG_LEVEL >= 3
#define SWD_DEBUG(fmt, ...) printf("[SWD DEBUG] " fmt, ##__VA_ARGS__)
#else
#define SWD_DEBUG(fmt, ...) ((void)0)
#endif

#if PICO2_SWD_DEBUG_LEVEL >= 2
#define SWD_INFO(fmt, ...) printf("[SWD INFO] " fmt, ##__VA_ARGS__)
#else
#define SWD_INFO(fmt, ...) ((void)0)
#endif

#if PICO2_SWD_DEBUG_LEVEL >= 1
#define SWD_WARN(fmt, ...) printf("[SWD WARN] " fmt, ##__VA_ARGS__)
#else
#define SWD_WARN(fmt, ...) ((void)0)
#endif

//==============================================================================
// SWD Protocol Constants
//==============================================================================

#define SWD_TURNAROUND_CYCLES 1
#define SWD_IDLE_CYCLES 8

//==============================================================================
// DAP State
//==============================================================================

typedef struct {
    // Current AP selection
    uint8_t current_apsel;
    uint8_t current_bank;
    bool ctrlsel;
    uint32_t select_cache;  // Last DP_SELECT value written

    // Power state
    bool powered;

    // Configuration
    uint retry_count;
} dap_state_t;

//==============================================================================
// Per-Hart State
//==============================================================================

/**
 * @brief Per-hart state for RISC-V debugging
 *
 * RP2350 has 2 RISC-V harts (hardware threads). Each hart maintains
 * independent execution state, registers, and cache.
 */
typedef struct {
    // Halt state
    bool halt_state_known;  // false after resume, true after halt/read status
    bool halted;            // true if hart is currently halted

    // Register cache
    bool cache_valid;       // true if cached values are current
    uint32_t cached_pc;
    uint32_t cached_gprs[32];
    uint64_t cache_timestamp;  // For LRU if needed
} hart_state_t;

//==============================================================================
// RP2350 Debug Module State
//==============================================================================

#define RP2350_NUM_HARTS 2

typedef struct {
    // Initialization state (shared across harts)
    bool initialized;
    bool sba_initialized;

    // Per-hart state
    hart_state_t harts[RP2350_NUM_HARTS];

    // Cache configuration (shared across harts)
    bool cache_enabled;

    // Note: Breakpoint/trigger support removed - to be reimplemented later
} rp2350_state_t;

//==============================================================================
// PIO State
//==============================================================================

typedef struct {
    PIO pio;
    uint sm;
    uint pio_offset;
    uint pin_swclk;
    uint pin_swdio;
    uint freq_khz;
    bool initialized;
} pio_state_t;

//==============================================================================
// Target Context (Opaque Handle)
//==============================================================================

struct swd_target {
    // Hardware configuration
    pio_state_t pio;

    // Connection state
    bool connected;
    uint32_t idcode;

    // Protocol layers
    dap_state_t dap;
    rp2350_state_t rp2350;

    // Error tracking
    swd_error_t last_error;
    uint8_t last_ack;
    char error_detail[128];

    // Resource tracking
    bool resource_registered;
};

//==============================================================================
// Global Resource Tracking
//==============================================================================

// Track which PIO/SM pairs are in use
typedef struct {
    swd_target_t *pio0_sm_owners[4];
    swd_target_t *pio1_sm_owners[4];
    uint active_count;
} resource_tracker_t;

extern resource_tracker_t g_resources;

//==============================================================================
// Internal Helpers
//==============================================================================

// Error management
void swd_set_error(swd_target_t *target, swd_error_t error, const char *detail, ...);
swd_error_t swd_ack_to_error(uint8_t ack);

// Resource management
swd_error_t allocate_pio_sm(PIO *pio, uint *sm);
void release_pio_sm(PIO pio, uint sm);
bool register_target(swd_target_t *target, PIO pio, uint sm);
void unregister_target(swd_target_t *target);

// Low-level SWD operations
swd_error_t swd_io_raw(swd_target_t *target, uint8_t request, uint32_t *data, bool write);
void swd_line_reset(swd_target_t *target);
void swd_send_idle_clocks(swd_target_t *target, uint count);

// Low-level DP/AP raw access (used internally)
swd_error_t swd_read_dp_raw(swd_target_t *target, uint8_t reg, uint32_t *value);
swd_error_t swd_write_dp_raw(swd_target_t *target, uint8_t reg, uint32_t value);
swd_error_t swd_read_ap_raw(swd_target_t *target, uint8_t reg, uint32_t *value);
swd_error_t swd_write_ap_raw(swd_target_t *target, uint8_t reg, uint32_t value);

// DP/AP utilities
uint8_t calculate_parity(uint32_t value);
uint32_t make_dp_select_rp2350(uint8_t apsel, uint8_t bank, bool ctrlsel);

#endif // PICO2_SWD_RISCV_INTERNAL_H
