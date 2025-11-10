# Test Suite

Comprehensive test suite for the pico2-swd-riscv library.

## Hardware Setup

You'll need:
- **Debugger Pico**: RP2040 or RP2350 running the test suite firmware
- **Target Pico**: RP2350 board to be debugged
- USB connection from debugger Pico to your computer

### Wiring

Connect the debugger to the target:

```
Debugger Pico          Target Pico
─────────────          ───────────
GPIO 2 (SWCLK)   ───>  SWCLK
GPIO 3 (SWDIO)   <──>  SWDIO
GND              ───>  GND
```

**Important**: The target should be powered on and running (either from USB or external power).

## Building

From the pico2-swd-riscv directory:

```bash
mkdir -p build
cd build
cmake .. -DPICO2_SWD_BUILD_EXAMPLES=ON
make test_suite
```

## Flashing

1. Hold BOOTSEL on the debugger Pico and connect USB
2. Copy the UF2 file:

```bash
cp examples/test_suite/test_suite.uf2 /Volumes/RPI-RP2/
```

## Running Tests

### 1. Install Python dependencies

```bash
cd examples/test_suite
pip3 install pyserial
```

### 2. Run the test suite

The script will auto-detect the Pico serial port:

```bash
python3 run_tests.py
```

Or specify the port manually:

```bash
python3 run_tests.py /dev/ttyACM0               # Linux
python3 run_tests.py /dev/cu.usbmodem14101      # macOS
python3 run_tests.py COM3                       # Windows
```

Optional: Save output to file:

```bash
python3 run_tests.py /dev/ttyACM0 results.txt
```

## Test Coverage

The test suite is organized into 6 categories with 41 total tests:

### Basic Connection Tests (2 tests)
1. **Connection Verification** - SWD protocol initialization and IDCODE reading
2. **Debug Module Status** - RP2350 RISC-V debug module initialization and DMI access

### Hart 0 Tests (14 tests)
3. **Halt Hart 0** - Halting hart 0 via DMCONTROL
4. **Read PC** - Reading program counter via DPC CSR
5. **Read All GPRs** - Reading all 32 general-purpose registers (x0-x31)
6. **Write/Verify GPRs** - Writing and verifying test patterns to x1-x31, validating x0 is hardwired zero
7. **Write/Verify PC** - Writing PC to new address and verifying change
8. **Read ROM** - Reading boot ROM at 0x00000000 via System Bus Access
9. **Write/Verify SRAM** - Writing and verifying SRAM at 0x20000000
10. **Resume Hart 0** - Resuming hart execution via DMCONTROL
11. **Halt/Resume Stress Test** - Rapid halt/resume cycles to verify state machine robustness
12. **Register Stress Test** - High-volume register read/write operations
13. **Memory Stress Test** - Burst memory operations testing SBA throughput
14. **Execute Small Program** - Writing and executing a small program in SRAM
15. **Instruction Tracing** - Single-stepping with PC and instruction capture
16. **Hart Reset** - Resetting hart via DMCONTROL.ndmreset

### Hart 1 Tests (5 tests)
17. **Halt Hart 1** - Halting hart 1 via hartsel switching
18. **Read Hart 1 PC** - Verifying per-hart PC isolation
19. **Write/Verify Hart 1 Registers** - Per-hart register access validation
20. **PC Write Verification (Both Harts)** - Cross-hart PC verification
21. **Read All Hart 1 Registers** - Full register set access for hart 1

### Dual-Hart Tests (6 tests)
22. **Independent Hart Control** - Verifying independent halt/resume control
23. **Per-Hart Register Isolation** - Validating separate register contexts
24. **Execute Code on Hart 1** - Running distinct programs on each hart
25. **Hart 1 Reset** - Independent reset control verification
26. **Single-Step Both Harts** - Interleaved single-stepping of both harts
27. **Rapid Hart Switching** - Stress testing hartsel switching performance

### Memory Tests (9 tests)
- **MEM 1: Basic Memory R/W (Halted)** - Fundamental memory access via halted hart
- **MEM 2: Walking 1s Pattern** - Bit-level verification with shifting 1s
- **MEM 3: Walking 0s Pattern** - Bit-level verification with shifting 0s
- **MEM 4: Checkerboard Pattern** - 0xAAAAAAAA/0x55555555 pattern testing
- **MEM 5: Address-Based Pattern** - Address-as-data pattern verification
- **MEM 6: Large Block (4KB)** - Bulk transfer testing
- **MEM 7: Memory Access While Running** - SBA non-intrusive access validation
- **MEM 8: RAM Fill with CPU (256KB)** - Large-scale memory initialization by target CPU
- **MEM 9: Checksum Verification (256KB)** - Integrity validation of full SRAM

### Trace Tests (5 tests)
- **TRACE 1: Basic Instruction Trace** - Single-step execution with PC and instruction capture
- **TRACE 2: Trace with Register Capture** - Extended trace with GPR snapshots per instruction
- **TRACE 3: Early Termination via Callback** - User-defined trace abort conditions
- **TRACE 4: Loop Detection** - Automatic detection of repeated instruction sequences
- **TRACE 5: Trace Hart 1** - Verifying trace functionality on second hart

## Output

The test suite provides real-time output with:
- Test category headers
- Per-test pass/fail status
- Detailed diagnostic messages
- Value displays in hex format (e.g., PC, registers, memory)
- Summary statistics per category and overall

Example output:
```
====================================
  pico2-swd-riscv Test Suite
====================================
Version: 0.1.0
Pins: SWCLK=2, SWDIO=3

Test suite ready!
Send 'TEST_ALL' to run full test suite, or 'HELP' for commands.

# Command: TEST_ALL

====================================
  Running Full Test Suite
====================================

=== BASIC CONNECTION TESTS ===
# Connecting to target...
# Connected to target
# Initializing RP2350 debug module...
# RP2350 debug module initialized
[TEST 1: Connection Verification] ... PASS
[TEST 2: Debug Module Status] ... PASS

=== HART 0 TESTS ===
# Halting harts for clean state...
[TEST 3: Halt Hart 0] ... PASS
[TEST 4: Read PC]
  PC = 0x00000192
... PASS
[TEST 5: Read All GPRs]
  x0  = 0x00000000  x1  = 0x20041f90  x2  = 0x20042000  x3  = 0x00000000
  ...
... PASS

...

====================================
  Overall Test Results
====================================
Passed:  41
Failed:  0
Skipped: 0
Total:   41

ALL TESTS PASSED!
```

## Exit Codes

The Python runner exits with:
- **0**: Test suite completed (check output for pass/fail details)
- **1**: Communication error or exception occurred

## Troubleshooting

### "Could not find Pico"
- Make sure the debugger Pico is connected via USB
- Check that it's running the test_suite firmware (not BOOTSEL mode)
- On Linux, you may need sudo or add your user to the `dialout` group

### Connection or initialization failures
- Check wiring between debugger and target (SWCLK=GPIO2, SWDIO=GPIO3, GND)
- Verify target is powered on
- Verify target is RP2350 (not RP2040 - this library is RISC-V only)
- Try reducing SWD frequency in main.c (default is 1 MHz)

### Test failures
- Target may be in BOOTSEL mode - power cycle it
- Check for short circuits on SWCLK/SWDIO lines
- Some memory regions may be read-only (ROM, Flash)
- SRAM is at 0x20000000 - 0x20080000 (512KB)

## Protocol Reference

The test suite uses a simple command/response protocol over USB serial:

Commands:
```
READY      - Check if test suite is ready
TEST_ALL   - Run full test suite
DISCONNECT - Disconnect from target
HELP       - Show available commands
```

Responses:
```
PASS                - Test/command succeeded
FAIL:<message>      - Test/command failed with error
VALUE:<hex>         - Data value (8-digit hex)
# <text>            - Comment/diagnostic output
```

## License

MIT License - see LICENSE file in the root directory.
