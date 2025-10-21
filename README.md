# liberg - ERG File Library

A C library for reading and writing ERG files and their associated `.erg.info` metadata files.

## Current Status

Currently implemented:
- ✅ ERG.info file parsing (key=value and key:value multiline format)
- ✅ ERG.info file writing
- ⏳ ERG binary file parsing (planned)
- ⏳ ERG binary file writing (planned)

## ERG.info Format

ERG.info files use a simple key-value format with two syntaxes:

### Single-line values (using `=`)
```
File.Format = erg
File.ByteOrder = LittleEndian
File.DateInSeconds = 1750288191
```

### Multi-line values (using `:`)
```
Comment:
	This is a multiline comment
	Each continuation line is indented with a tab
	Values can contain = and : characters
```

Special features:
- Comments start with `#`
- Empty lines are ignored
- Values can contain any Unicode characters including `=` and `:`
- Multi-line values preserve newlines and indentation

## Building

### Requirements
- CMake 3.10 or later
- C11 compatible compiler (GCC, Clang, MSVC)

### Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

This will create:
- `libliberg.a` - Static library
- `libliberg.so` - Shared library
- `test_erginfo` - Test suite
- `example` - Example program

### Running Tests

```bash
# Run basic unit tests
./build/test_erginfo

# Run tests with a real erg.info file
./build/test_erginfo example/AEB_CBLA_30kph_15kph_50%_080836.erg.info
```

## Usage

### Basic Example

```c
#include <erginfo.h>

int main() {
    ErgInfo info;
    erginfo_init(&info);

    // Parse a file
    if (erginfo_parse_file("data.erg.info", &info) == 0) {
        // Get values
        const char *format = erginfo_get(&info, "File.Format");
        printf("Format: %s\n", format);

        // Set or update values
        erginfo_set(&info, "MyKey", "MyValue");

        // Write to a new file
        erginfo_write_file("output.erg.info", &info);
    }

    // Clean up
    erginfo_free(&info);
    return 0;
}
```

### API Reference

#### Initialization and Cleanup

```c
void erginfo_init(ErgInfo *info);
```
Initialize an ErgInfo structure. Must be called before using the structure.

```c
void erginfo_free(ErgInfo *info);
```
Free all memory associated with an ErgInfo structure.

#### Parsing

```c
int erginfo_parse_file(const char *filename, ErgInfo *info);
```
Parse an erg.info file from disk. Returns 0 on success, -1 on error.

```c
int erginfo_parse_string(const char *data, size_t len, ErgInfo *info);
```
Parse an erg.info file from a string buffer. Returns 0 on success, -1 on error.

#### Accessing Data

```c
const char *erginfo_get(const ErgInfo *info, const char *key);
```
Get a value by key. Returns NULL if the key is not found.

```c
int erginfo_set(ErgInfo *info, const char *key, const char *value);
```
Set or update a key-value pair. Returns 0 on success, -1 on error.

#### Writing

```c
int erginfo_write_file(const char *filename, const ErgInfo *info);
```
Write an ErgInfo structure to a file. Returns 0 on success, -1 on error.

```c
char *erginfo_write_string(const ErgInfo *info);
```
Write an ErgInfo structure to a string buffer (allocates memory).
Returns allocated string on success, NULL on error.
Caller must free the returned string.

### Example Program

Run the example program with:

```bash
./build/example example/AEB_CBLA_30kph_15kph_50%_080836.erg.info
```

## Project Structure

```
liberg/
├── include/         # Public header files
│   └── erginfo.h
├── src/            # Implementation files
│   └── erginfo.c
├── test/           # Test suite
│   └── test_erginfo.c
├── example/        # Example code and sample files
│   ├── example.c
│   └── *.erg.info
├── build/          # Build directory (generated)
└── CMakeLists.txt
```

## Installation

```bash
cd build
cmake --install .
```

This will install:
- Headers to `/usr/local/include/`
- Libraries to `/usr/local/lib/`

## License

MIT License (see LICENSE file)

## Contributing

Contributions are welcome! This is the initial implementation focused on ERG.info parsing.
Future work includes parsing the binary ERG format itself.