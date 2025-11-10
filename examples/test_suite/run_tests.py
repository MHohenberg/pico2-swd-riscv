#!/usr/bin/env python3
"""
Simple test runner for pico2-swd-riscv test suite.

Connects to serial port, sends TEST_ALL command, and logs all output.

Usage:
    python3 run_tests.py [serial_port] [output_file]

Examples:
    python3 run_tests.py                          # Auto-detect port
    python3 run_tests.py /dev/ttyACM0             # Specific port
    python3 run_tests.py /dev/ttyACM0 results.txt # Save to file
"""

import sys
import time
import serial
import serial.tools.list_ports
from datetime import datetime

BAUD_RATE = 115200
TIMEOUT = 1  # seconds


def find_pico_port():
    """Try to auto-detect Pico USB serial port"""
    ports = serial.tools.list_ports.comports()

    for port in ports:
        if "Pico" in port.description or "2E8A" in port.hwid.upper():
            return port.device

    return None


def main():
    # Parse command line
    port = None
    output_file = None

    if len(sys.argv) > 1:
        port = sys.argv[1]
    if len(sys.argv) > 2:
        output_file = sys.argv[2]

    # Auto-detect port if not specified
    if not port:
        print("Searching for Pico...")
        port = find_pico_port()
        if not port:
            print("ERROR: Could not find Pico. Please specify port manually:")
            print(f"  python3 run_tests.py <serial_port> [output_file]")
            print("\nAvailable ports:")
            for p in serial.tools.list_ports.comports():
                print(f"  {p.device}: {p.description}")
            return 1
        print(f"Found Pico at {port}\n")

    # Open output file if specified
    output = None
    if output_file:
        try:
            output = open(output_file, 'w')
            print(f"Saving output to {output_file}\n")
        except Exception as e:
            print(f"ERROR: Failed to open output file: {e}")
            return 1

    # Connect to serial port
    try:
        print(f"Connecting to {port}...")
        ser = serial.Serial(port, BAUD_RATE, timeout=TIMEOUT)
        time.sleep(1)  # Wait for device to be ready
        print("Connected!\n")
    except Exception as e:
        print(f"ERROR: Failed to connect: {e}")
        if output:
            output.close()
        return 1

    try:
        # Write header to output file
        if output:
            output.write(f"pico2-swd-riscv Test Suite Results\n")
            output.write(f"Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            output.write(f"Port: {port}\n")
            output.write("=" * 70 + "\n\n")

        # Flush any pending data
        ser.reset_input_buffer()

        # Send TEST_ALL command
        print("Sending TEST_ALL command...\n")
        print("=" * 70)
        ser.write(b"TEST_ALL\n")
        ser.flush()

        # Read and print all output
        line_buffer = ""
        timeout_count = 0
        max_timeouts = 20  # Exit after 2 seconds of no data

        while True:
            if ser.in_waiting > 0:
                # Reset timeout counter when we receive data
                timeout_count = 0

                # Read available data
                chunk = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                line_buffer += chunk

                # Process complete lines
                while '\n' in line_buffer:
                    line, line_buffer = line_buffer.split('\n', 1)
                    line = line.rstrip('\r')

                    # Print to console
                    print(line)

                    # Write to file
                    if output:
                        output.write(line + '\n')
                        output.flush()

                    # Check for test completion
                    if "TEST SUMMARY" in line or "ALL TESTS PASSED" in line or "SOME TESTS FAILED" in line:
                        # Continue reading for a bit more to catch final output
                        time.sleep(0.5)
                        # Flush remaining buffer
                        if ser.in_waiting > 0:
                            chunk = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                            line_buffer += chunk
                            for final_line in line_buffer.split('\n'):
                                final_line = final_line.rstrip('\r')
                                if final_line:
                                    print(final_line)
                                    if output:
                                        output.write(final_line + '\n')

            else:
                # No data available, wait a bit
                time.sleep(TIMEOUT)
                timeout_count += 1

                if timeout_count >= max_timeouts:
                    print("\n(No more data received, test complete)")
                    break

        print("=" * 70)

    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        ser.close()
        if output:
            output.write("\n" + "=" * 70 + "\n")
            output.write(f"Test completed at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            output.close()
            print(f"\nResults saved to {output_file}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
