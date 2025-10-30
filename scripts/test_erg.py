#!/usr/bin/env python3
"""
Python test suite for ERG parser - mirrors test/test_erg.c functionality
"""

import sys
import time
from pathlib import Path

# Add parent directory to path to import cmparser
sys.path.insert(0, str(Path(__file__).parent.parent))

from cmparser import ERG

EPSILON = 1e-9


def get_time_ns():
    """Get current time in nanoseconds"""
    return time.perf_counter_ns()


def test_erg_basic(erg_path):
    """Test basic ERG parsing - mirrors test_erg_basic() in C"""
    print("Testing basic ERG parsing...")

    erg = ERG(erg_path)

    print(f"  Loaded ERG file: {erg_path}")
    print(f"  Number of signals: {len(erg.metadata)}")
    print(f"  Number of samples: {len(erg.dataframe)}")

    # Get row size from all signals' dtype
    if erg.metadata:
        row_size = sum(erg.dataframe[sig].dtype.itemsize for sig in erg.metadata.keys())
        print(f"  Row size: {row_size} bytes")

    # Python ERG exposes endianness
    endian_str = "Little-Endian" if erg.endianness == "<" else "Big-Endian"
    print(f"  Endianness: {endian_str}")

    # Basic sanity checks
    assert len(erg.metadata) > 0, "No signals found"
    assert len(erg.dataframe) > 0, "No samples found"

    print("[OK] Basic ERG parsing test passed")


def test_erg_signal_access(erg_path):
    """Test signal access - mirrors test_erg_signal_access() in C"""
    print("\nTesting signal access...")

    erg = ERG(erg_path)

    # Test getting Time signal
    if "Time" in erg.dataframe.columns:
        time_data = erg.dataframe["Time"].values
        print(f"  Found 'Time' signal: {len(time_data)} samples")

        print(f"  Time range: {time_data[0]:.3f} to {time_data[-1]:.3f} seconds")

        # Check that time is monotonically increasing
        for i in range(1, len(time_data)):
            assert time_data[i] >= time_data[i-1], f"Time not monotonic at index {i}"

        print("[OK] Time signal test passed")
    else:
        print("  'Time' signal not found, testing with first signal")

    # Test signal info
    if erg.metadata:
        first_signal_name = list(erg.metadata.keys())[0]
        sig_info = erg.metadata[first_signal_name]

        print(f"  First signal: {first_signal_name}")
        print(f"    Type size: {erg.dataframe[first_signal_name].dtype.itemsize} bytes")
        print(f"    Unit: {sig_info.get('unit', '')}")
        print(f"    Factor: {sig_info.get('factor', 1.0):.6f}")
        print(f"    Offset: {sig_info.get('offset', 0.0):.6f}")

        print("[OK] Signal info test passed")

    # Test non-existent signal
    assert "NonExistentSignal123" not in erg.dataframe.columns
    print("[OK] Non-existent signal test passed")


def test_erg_export_csv(erg_path):
    """Test CSV export - mirrors test_erg_export_csv() in C"""
    print("\nExporting CSV file...")

    erg = ERG(erg_path)

    # Signal names to export
    signal_names = ["Time", "Car.ax", "Car.v", "Vhcl.tRoad"]

    # Check which signals are available
    available_signals = []
    for sig_name in signal_names:
        if sig_name in erg.dataframe.columns:
            print(f"  Found signal: {sig_name} ({len(erg.dataframe)} samples)")
            available_signals.append(sig_name)
        else:
            print(f"  Signal not found: {sig_name}")

    if not available_signals:
        print("[WARNING] No signals found for CSV export")
        return

    # Export to CSV
    csv_df = erg.dataframe[available_signals]

    # Add empty columns for missing signals to match C output format
    for sig_name in signal_names:
        if sig_name not in available_signals:
            csv_df[sig_name] = ""

    # Reorder to match expected column order
    csv_df = csv_df[signal_names]
    csv_df.to_csv("result.csv", index=False, float_format="%.6f")

    # Calculate actual frequency from Time signal
    if "Time" in available_signals and len(erg.dataframe) >= 2:
        time_data = erg.dataframe["Time"].values
        dt = time_data[1] - time_data[0]
        frequency = 1.0 / dt
        print(f"  Time signal frequency: {frequency:.3f} Hz (dt = {dt:.6f} s)")
        print(f"  Time range: {time_data[0]:.3f} to {time_data[-1]:.3f} seconds")

    print(f"  Wrote {len(erg.dataframe)} rows to result.csv")
    print("[OK] CSV export test passed")


def test_erg_performance(erg_path):
    """Performance benchmark - mirrors test_erg_performance() in C"""
    print("\nPerformance Benchmark...")

    # Get file info from first parse
    erg_temp = ERG(erg_path)
    signal_count = len(erg_temp.metadata)
    sample_count = len(erg_temp.dataframe)

    # Calculate data size
    row_size = sum(erg_temp.dataframe[sig].dtype.itemsize for sig in erg_temp.metadata.keys())
    data_size_mb = (sample_count * row_size) / (1024.0 * 1024.0)

    signals_to_test = min(signal_count, 10)
    signal_names = list(erg_temp.metadata.keys())[:signals_to_test]

    print(f"  File: {erg_path}")
    print(f"  Signals: {signal_count}, Samples: {sample_count}")
    print(f"  Data size: {data_size_mb:.2f} MB")

    # Calculate bytes per iteration before deleting erg_temp
    bytes_per_iteration = sum(
        erg_temp.dataframe[sig_name].dtype.itemsize * sample_count
        for sig_name in signal_names
    )

    del erg_temp

    # Benchmark pattern: 2 rounds of 5 iterations each
    ITERATIONS = 5
    ROUNDS = 2
    times = [[0.0] * ITERATIONS for _ in range(ROUNDS)]

    print(f"\n  Benchmark (pattern: Parse+Extract x5, Parse+Extract x5):")

    for round_num in range(ROUNDS):
        print(f"  Round {round_num + 1}:")
        for iter_num in range(ITERATIONS):
            start = get_time_ns()

            # Parse ERG file and extract signals
            erg = ERG(erg_path)
            for sig_name in signal_names:
                data = erg.dataframe[sig_name].values
                # Access data to ensure it's extracted
                _ = data[0]

            end = get_time_ns()
            elapsed_ns = end - start
            times[round_num][iter_num] = elapsed_ns

            print(f"    Iteration {iter_num + 1}: {elapsed_ns / 1_000_000:.2f} ms ({elapsed_ns:.0f} ns)")

    # Calculate averages
    total_time = sum(sum(round_times) for round_times in times)
    avg_time = total_time / (ROUNDS * ITERATIONS)

    print(f"\n  Average: {avg_time / 1_000_000:.2f} ms ({avg_time:.0f} ns)")

    # Calculate throughput
    throughput_mbps = (bytes_per_iteration / (1024.0 * 1024.0)) / (avg_time / 1_000_000_000.0)

    print(f"  Throughput: {throughput_mbps:.2f} MB/s")

    print("[OK] Performance benchmark completed")


def main():
    print("=== ERG Parser Test ===\n")

    if len(sys.argv) < 2:
        print("ERROR: ERG file path required", file=sys.stderr)
        print(f"Usage: {sys.argv[0]} <path/to/file.erg>", file=sys.stderr)
        return 1

    erg_path = sys.argv[1]

    # Check if file exists
    if not Path(erg_path).exists():
        print(f"ERROR: ERG file not found: {erg_path}", file=sys.stderr)
        return 1

    print(f"ERG file: {erg_path}\n")

    test_erg_basic(erg_path)
    test_erg_signal_access(erg_path)
    test_erg_export_csv(erg_path)
    test_erg_performance(erg_path)

    print("\n=== All ERG tests passed! ===")
    print("\nGenerated files:")
    print("  result.csv - CSV export of Time, Car.ax, Car.v, Vhcl.tRoad")

    return 0


if __name__ == "__main__":
    sys.exit(main())
