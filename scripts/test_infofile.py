#!/usr/bin/env python3
"""
Python test suite for InfoFile parser - mirrors test/test_infofile.c functionality
"""

import sys
import time
from pathlib import Path

# Add parent directory to path to import cmparser
sys.path.insert(0, str(Path(__file__).parent.parent))

from cmparser import InfoFile


def get_time_ns():
    """Get current time in nanoseconds"""
    return time.perf_counter_ns()


# Test cases for road.rd5 (large file with ~385K entries)
road_test_cases = [
    # Beginning of file
    ("FileIdent", "IPGRoad 14.0", "File identifier (line 2)"),
    ("FileCreator", "CarMaker Office 14.1.1", "File creator (line 3)"),

    # Early section
    ("Junction.0.ID", "579482", "Junction ID"),
    ("Junction.0.Type", "Area", "Junction type"),
    ("Junction.0.RST", "Countryroad", "Junction road surface type"),
    ("Route.0.Length", "1046050.30450494", "Route length"),
    ("Route.0.ID", "9495", "Route ID"),
    ("Route.0.Name", "Route_2", "Route name"),
    ("nLinks", "3535", "Number of links"),
    ("nJunctions", "2834", "Number of junctions"),

    # Middle section
    ("Link.2175.LaneSection.0.LaneR.0.ID", "477549", "Link lane ID (middle)"),

    # Late section (around line 900,000)
    ("Link.3485.LateralCenterLineOffset.ID", "894619", "Link 3485 offset ID (late)"),
    ("Link.3485.LaneSection.0.ID", "894558", "Link 3485 lane section (late)"),
    ("Link.3485.LaneSection.0.Start", "0", "Link 3485 section start (late)"),

    # End of file
    ("Control.TrfLight.68", "941160 JuncArm_381952 Time>=0.000000 3 0 15 4 28 4", "Traffic light 68 (end)"),
    ("Control.TrfLight.69", "941161 CtrlTL015 \"\" 1 0 15 3 15 3", "Traffic light 69 (end)"),
    ("MaxUsedObjId", "941652", "Max object ID (line 996366)"),

    # Multiline values
    ("Junction.0.Link.0.LaneSection.0.LaneL.0.Width",
     "445061 -1 0 0 1 3.99495155267296 0 -999 -999\n362229 -1 0 1 1 3.78377990903144 0 -999 -999",
     "Multiline width (2 lines)"),
    ("Junction.0.Link.0.LaneSection.0.LaneR.0.Width",
     "445062 -1 0 0 1 4.00700290085828 0 -999 -999\n362227 -1 0 1 1 3.78039994814133 0 -999 -999",
     "Multiline width (2 lines)"),
    ("Junction.1.HMMesh.DeltaU", "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1", "Multiline DeltaU (1 line)"),
    ("Junction.3.RL.3.Seg.0.Curve.Points",
     "362894 362565 -0.820901971666899 45.6687762230722 0 0 0 100 277.903092940284 -999\n"
     "362895 362565 3.2474857365014 16.4614153836155 0 0 0 100 -999 -999\n"
     "362896 362565 4.77912495504279 5.3511128462851 0 0 0 100 277.720834107522 -999",
     "Multiline curve points (3 lines)"),
]

# Test cases for result.erg.info (smaller file with detailed metadata)
erg_test_cases = [
    # File header
    ("File.Format", "erg", "File format"),
    ("File.ByteOrder", "LittleEndian", "Byte order"),
    ("File.DateInSeconds", "1750288191", "Date in seconds"),

    # File attributes (beginning)
    ("File.At.1.Name", "FCA_WrngLvlSta_CM", "First attribute name"),
    ("File.At.1.Type", "Double", "First attribute type"),
    ("File.At.2.Name", "IDS.FCA_DclReqVal", "Second attribute name"),
    ("File.At.3.Name", "Time", "Time attribute"),
    ("Quantity.Time.Unit", "s", "Time unit"),

    # Middle attributes
    ("File.At.50.Name", "Output_FR_C_Radar.GW_Radar_Object_00.motion_type", "Attribute 50"),
    ("Quantity.Output_FR_C_Radar.GW_Radar_Object_00.motion_type.Unit", "-", "Motion type unit"),

    # Late attributes
    ("File.At.100.Name", "Output_FR_Camera.Object_02.Rel_vel_X", "Attribute 100"),
    ("File.At.133.Name", "Sensor.Collision.Vhcl.Fr1.Count", "Last attribute"),
    ("File.At.133.Type", "Int", "Last attribute type"),

    # Animation messages (multiline base64 data)
    ("Anim.Msg.0.Time", "0", "First animation time"),
    ("Anim.Msg.0.Class", "Anim", "First animation class"),
    ("Anim.Msg.0.Id", "18", "First animation ID"),

    # Vehicle info
    ("Anim.VehicleClass", "Car", "Vehicle class"),
    ("Anim.Vehicle.MovieSkin", "Kia_EV9_2023.obj", "Vehicle skin"),

    # Testrun info
    ("Testrun", "EuroNCAP_2026/Variations/AEB_CBLA/AEB_CBLA_30kph_15kph_50%", "Testrun path"),
    ("SimParam.DeltaT", "0.001", "Simulation delta time"),
    ("RandomSeed", "1750288116", "Random seed"),

    # Named values
    ("NamedValues.Count", "0", "Named values count"),
    ("KeyValues.Count", "0", "Key values count"),
    ("GPUSensors.Count", "0", "GPU sensors count"),

    # CarMaker version info
    ("CarMaker.NumVersion", "120001", "CarMaker numeric version"),
    ("CarMaker.Version", "12.0.1", "CarMaker version string"),
    ("CarMaker.Version.MatSupp", "12.0.1", "MatSupp version"),
    ("CarMaker.Version.Road", "12.0.1", "Road version"),
]


def test_basic_parsing():
    """Test basic parsing with inline data - mirrors test_basic_parsing() in C"""
    print("Testing basic parsing with inline data...")

    test_data = """#INFOFILE1.1 (UTF-8) - Do not remove this line!

File.Format = erg
File.ByteOrder = LittleEndian
File.DateInSeconds = 1750288191

Comment:
\tThis is a multiline comment
\tWith multiple lines

Anim.Msg.0.Data:
\tline1
\tline2
\tline3
"""

    # Write test data to temporary file
    import tempfile
    with tempfile.NamedTemporaryFile(mode='w', suffix='.info', delete=False, encoding='utf-8') as f:
        f.write(test_data)
        temp_path = f.name

    try:
        info = InfoFile(temp_path)

        assert len(info) == 5, f"Expected 5 entries, got {len(info)}"

        # Test single-line values
        format_val = info.get("File.Format")
        assert format_val is not None, "File.Format not found"
        assert format_val == "erg", f"Expected 'erg', got '{format_val}'"

        byte_order = info.get("File.ByteOrder")
        assert byte_order == "LittleEndian"

        date_secs = info.get("File.DateInSeconds")
        assert date_secs == 1750288191, f"Expected 1750288191, got {date_secs}"

        # Test multiline values (stored as list of lists)
        comment = info.get("Comment")
        assert comment is not None, "Comment not found"
        # Comment is stored as nested list
        assert isinstance(comment, list), f"Comment should be list, got {type(comment)}"

        anim_data = info.get("Anim.Msg.0.Data")
        assert anim_data is not None, "Anim.Msg.0.Data not found"
        assert isinstance(anim_data, list), f"Anim.Msg.0.Data should be list, got {type(anim_data)}"

        print(f"[OK] Basic parsing test passed ({len(info)} entries)")
    finally:
        Path(temp_path).unlink()


def test_file_comprehensive(filename, test_cases, file_desc):
    """Test comprehensive file parsing - mirrors test_file_comprehensive() in C"""
    print(f"\nTesting {file_desc}...")

    info = InfoFile(filename)

    print(f"  Parsed {len(info)} entries from {filename}")

    # Test all test cases
    passed = 0
    for key, expected_value, description in test_cases:
        value = info.get(key)
        if value is None:
            print(f"ERROR: Key '{key}' not found in file", file=sys.stderr)
            print(f"       {description}", file=sys.stderr)
            sys.exit(1)

        # Convert value to string for comparison (InfoFile converts numeric strings to int/float)
        value_str = str(value) if not isinstance(value, list) else " ".join(str(v) for v in value)

        # Handle multiline values
        if "\n" in expected_value:
            # For multiline values, InfoFile stores as list of lists
            if isinstance(value, list) and all(isinstance(v, list) for v in value):
                # Reconstruct the multiline string
                value_str = "\n".join(" ".join(str(x) for x in row) for row in value)

        if value_str != expected_value:
            print(f"ERROR: {description}", file=sys.stderr)
            print(f"       Key: {key}", file=sys.stderr)
            print(f"       Expected: '{expected_value}'", file=sys.stderr)
            print(f"       Got:      '{value_str}'", file=sys.stderr)
            sys.exit(1)

        passed += 1

    print(f"[OK] All {passed} test cases passed for {file_desc}")


def test_file_performance(filename, file_desc):
    """Performance benchmark - mirrors test_file_performance() in C"""
    print(f"\nPerformance Benchmark for {file_desc}...")

    ITERATIONS = 5

    # Benchmark parsing
    print("  Parsing benchmark:")
    parse_times = []
    entry_count = 0

    for iter_num in range(ITERATIONS):
        start = get_time_ns()
        info = InfoFile(filename)
        end = get_time_ns()

        elapsed_ns = end - start
        parse_times.append(elapsed_ns)
        entry_count = len(info)

        print(f"    Iteration {iter_num + 1}: {elapsed_ns / 1_000_000:.2f} ms ({elapsed_ns:.0f} ns) - {entry_count} entries")

    avg_parse_time = sum(parse_times) / ITERATIONS
    print(f"    Average: {avg_parse_time / 1_000_000:.2f} ms ({avg_parse_time:.0f} ns)")
    print(f"    Throughput: {entry_count / (avg_parse_time / 1_000_000_000.0):.0f} entries/sec")

    # Benchmark lookups
    info = InfoFile(filename)
    print(f"\n  Lookup benchmark ({len(info)} entries):")

    if len(info) > 0:
        # Get keys at different positions
        keys = list(info.keys())
        test_keys = [
            (keys[0], "beginning"),
            (keys[len(keys) // 2], "middle"),
            (keys[-1], "end"),
        ]

        for key, desc in test_keys:
            lookup_times = []

            for _ in range(ITERATIONS * 1000):
                start = get_time_ns()
                value = info.get(key)
                end = get_time_ns()
                lookup_times.append(end - start)

                if value is None:
                    break

            avg_lookup_time = sum(lookup_times) / len(lookup_times)
            print(f"    {desc} key: {avg_lookup_time:.0f} ns/lookup ({avg_lookup_time / 1000.0:.2f} us)")

    print("[OK] Performance benchmark completed")


def main():
    print("=== InfoFile Parser Comprehensive Test ===\n")

    # Test basic parsing with inline data
    test_basic_parsing()

    # Determine file paths
    road_file = None
    erg_file = None

    if len(sys.argv) > 1:
        road_file = sys.argv[1]
    else:
        # Check default location
        if Path("example/road.rd5").exists():
            road_file = "example/road.rd5"

    if len(sys.argv) > 2:
        erg_file = sys.argv[2]
    else:
        # Check default location
        if Path("example/result.erg.info").exists():
            erg_file = "example/result.erg.info"

    # Test road.rd5 if available
    if road_file and Path(road_file).exists():
        test_file_comprehensive(road_file, road_test_cases, "road.rd5 (large file)")
        test_file_performance(road_file, "road.rd5 (large file)")
    else:
        print("\n[WARNING] road.rd5 not found - skipping large file tests")
        print("  (Place file in example/road.rd5 or pass path as first argument)")

    # Test result.erg.info if available
    if erg_file and Path(erg_file).exists():
        test_file_comprehensive(erg_file, erg_test_cases, "result.erg.info (detailed metadata)")
        test_file_performance(erg_file, "result.erg.info (detailed metadata)")
    else:
        print("\n[WARNING] result.erg.info not found - skipping metadata tests")
        print("  (Place file in example/result.erg.info or pass path as second argument)")

    print("\n=== All tests passed! ===")

    if road_file and erg_file and Path(road_file).exists() and Path(erg_file).exists():
        print("\nAll comprehensive tests completed successfully!")
        print(f"  road.rd5:         ~385K entries tested")
        print(f"  result.erg.info:  Detailed metadata tested")

    return 0


if __name__ == "__main__":
    sys.exit(main())
