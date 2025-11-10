/**
 * @file swd.h
 * @brief Main public API for pico2-swd-riscv library
 *
 * This library provides SWD debugging support for RP2350 RISC-V cores.
 *
 * @section usage Basic Usage
 * @code
 * #include <pico2-swd-riscv/swd.h>
 *
 * swd_config_t config = swd_config_default();
 * config.pin_swclk = 2;
 * config.pin_swdio = 3;
 *
 * swd_target_t *target = swd_target_create(&config);
 * swd_connect(target);
 * swd_result_t idcode = swd_read_idcode(target);
 * swd_target_destroy(target);
 * @endcode
 *
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 */

#ifndef PICO2_SWD_RISCV_SWD_H
#define PICO2_SWD_RISCV_SWD_H

#include "pico2-swd-riscv/types.h"
#include "pico2-swd-riscv/version.h"
#include "hardware/pio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for creating an SWD target
 */
typedef struct {
    PIO pio;              ///< Which PIO block to use (pio0/pio1, or SWD_PIO_AUTO)
    uint sm;              ///< Which state machine (0-3, or SWD_SM_AUTO)
    uint pin_swclk;       ///< GPIO pin for SWCLK
    uint pin_swdio;       ///< GPIO pin for SWDIO
    uint freq_khz;        ///< SWCLK frequency in kHz (default: 1000)
    bool enable_caching;  ///< Enable register caching (default: true)
    uint retry_count;     ///< Number of retries on WAIT ACK (default: 5)
} swd_config_t;

/**
 * @brief Resource usage information
 */
typedef struct {
    bool pio0_sm_used[4];  ///< Which PIO0 state machines are in use
    bool pio1_sm_used[4];  ///< Which PIO1 state machines are in use
    uint active_targets;   ///< Number of active targets
} swd_resource_info_t;

//==============================================================================
// Configuration and Resource Management
//==============================================================================

/**
 * @brief Get default configuration
 *
 * Returns a configuration with sensible defaults:
 * - PIO: Auto-allocate
 * - SM: Auto-allocate
 * - Pins: Must be set by user
 * - Frequency: 1000 kHz
 * - Caching: Enabled
 * - Retry count: 5
 *
 * @return Default configuration structure
 */
swd_config_t swd_config_default(void);

/**
 * @brief Get current resource usage
 *
 * Query which PIO state machines are currently in use by SWD targets.
 *
 * @return Resource usage information
 */
swd_resource_info_t swd_get_resource_usage(void);

//==============================================================================
// Target Lifecycle Management
//==============================================================================

/**
 * @brief Create a new SWD target
 *
 * Allocates and initializes an SWD target with the given configuration.
 * If PIO/SM are set to auto, automatically allocates the next available
 * resources.
 *
 * @param config Configuration for the target
 * @return Pointer to target handle, or NULL on failure
 *
 * @note Call swd_target_destroy() when done to free resources
 */
swd_target_t* swd_target_create(const swd_config_t *config);

/**
 * @brief Destroy an SWD target and free resources
 *
 * Disconnects from target if connected, frees all resources including
 * PIO program memory and state machine.
 *
 * @param target Target to destroy (can be NULL)
 */
void swd_target_destroy(swd_target_t *target);

//==============================================================================
// Connection Management
//==============================================================================

/**
 * @brief Connect to target via SWD
 *
 * Performs the SWD connection sequence:
 * 1. Initialize PIO and GPIO
 * 2. Send JTAG-to-Dormant sequence
 * 3. Send Dormant-to-SWD sequence
 * 4. Read and verify IDCODE
 * 5. Power up debug domains
 *
 * @param target Target to connect to
 * @return SWD_OK on success, error code otherwise
 */
swd_error_t swd_connect(swd_target_t *target);

/**
 * @brief Disconnect from target
 *
 * Powers down debug domains and releases GPIO pins.
 *
 * @param target Target to disconnect from
 * @return SWD_OK on success, error code otherwise
 */
swd_error_t swd_disconnect(swd_target_t *target);

/**
 * @brief Check if target is connected
 *
 * @param target Target to check
 * @return true if connected, false otherwise
 */
bool swd_is_connected(const swd_target_t *target);

//==============================================================================
// Target Information
//==============================================================================

/**
 * @brief Read target IDCODE
 *
 * Reads the IDCODE register from the Debug Port. Must be connected.
 *
 * @param target Target to read from
 * @return Result containing IDCODE value or error
 */
swd_result_t swd_read_idcode(swd_target_t *target);

/**
 * @brief Get target information as a string
 *
 * Returns human-readable information about the connected target.
 *
 * @param target Target to query
 * @return Pointer to static string, or NULL if not connected
 */
const char* swd_get_target_info(const swd_target_t *target);

//==============================================================================
// Error Handling
//==============================================================================

/**
 * @brief Convert error code to string
 *
 * @param error Error code
 * @return Human-readable error string
 */
const char* swd_error_string(swd_error_t error);

/**
 * @brief Get detailed error information
 *
 * Returns additional context about the last error that occurred on this target.
 *
 * @param target Target to query
 * @return Detailed error message, or empty string if no error
 */
const char* swd_get_last_error_detail(const swd_target_t *target);

//==============================================================================
// Frequency Control
//==============================================================================

/**
 * @brief Set SWCLK frequency
 *
 * Changes the SWD clock frequency. Can be called while connected.
 * Useful for starting slow (100 kHz) and speeding up after connection.
 *
 * @param target Target to configure
 * @param freq_khz Frequency in kHz (recommended: 100-2000)
 * @return SWD_OK on success, error code otherwise
 */
swd_error_t swd_set_frequency(swd_target_t *target, uint32_t freq_khz);

/**
 * @brief Get current SWCLK frequency
 *
 * @param target Target to query
 * @return Current frequency in kHz, or 0 if invalid target
 */
uint32_t swd_get_frequency(const swd_target_t *target);

#ifdef __cplusplus
}
#endif

#endif // PICO2_SWD_RISCV_SWD_H
