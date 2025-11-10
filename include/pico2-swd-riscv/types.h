/**
 * @file types.h
 * @brief Core types and error codes for pico2-swd-riscv library
 *
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 */

#ifndef PICO2_SWD_RISCV_TYPES_H
#define PICO2_SWD_RISCV_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to an SWD target
 *
 * Create with swd_target_create(), destroy with swd_target_destroy().
 * Do not access fields directly.
 */
typedef struct swd_target swd_target_t;

/**
 * @brief Error codes returned by library functions
 */
typedef enum {
    SWD_OK = 0,                    ///< Operation successful
    SWD_ERROR_TIMEOUT,             ///< Operation timed out
    SWD_ERROR_FAULT,               ///< Target returned FAULT ACK
    SWD_ERROR_PROTOCOL,            ///< SWD protocol error
    SWD_ERROR_PARITY,              ///< Parity check failed
    SWD_ERROR_WAIT,                ///< Target returned WAIT ACK (retry exhausted)
    SWD_ERROR_NOT_CONNECTED,       ///< Target not connected
    SWD_ERROR_NOT_HALTED,          ///< Operation requires hart to be halted
    SWD_ERROR_ALREADY_HALTED,      ///< Hart is already halted
    SWD_ERROR_INVALID_STATE,       ///< Invalid state for operation
    SWD_ERROR_NO_MEMORY,           ///< Memory allocation failed
    SWD_ERROR_INVALID_CONFIG,      ///< Invalid configuration
    SWD_ERROR_RESOURCE_BUSY,       ///< PIO/SM already in use
    SWD_ERROR_INVALID_PARAM,       ///< Invalid parameter
    SWD_ERROR_NOT_INITIALIZED,     ///< Debug module not initialized
    SWD_ERROR_ABSTRACT_CMD,        ///< Abstract command failed
    SWD_ERROR_BUS,                 ///< System bus access error
    SWD_ERROR_ALIGNMENT,           ///< Memory address alignment error
    SWD_ERROR_VERIFY,              ///< Memory verification failed
} swd_error_t;

/**
 * @brief Result type for operations that return a 32-bit value
 *
 * Check the error field before using value.
 */
typedef struct {
    swd_error_t error;  ///< Error code (SWD_OK on success)
    uint32_t value;     ///< Result value (valid only if error == SWD_OK)
} swd_result_t;

/**
 * @brief PIO selection mode
 */
typedef enum {
    SWD_PIO_AUTO = 0xFF,  ///< Automatically select next available PIO
} swd_pio_mode_t;

/**
 * @brief State machine selection mode
 */
typedef enum {
    SWD_SM_AUTO = 0xFF,   ///< Automatically select next available SM
} swd_sm_mode_t;

/**
 * @brief SWD ACK response values (internal use)
 */
#define SWD_ACK_OK     0x1
#define SWD_ACK_WAIT   0x2
#define SWD_ACK_FAULT  0x4
#define SWD_ACK_ERROR  0x7

#ifdef __cplusplus
}
#endif

#endif // PICO2_SWD_RISCV_TYPES_H
