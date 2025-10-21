# liberg - ERG File Library

A C library for reading and writing ERG files and their associated `.erg.info` metadata files.

## Current Status

Currently implemented:

- ✅ Info file parsing (key=value and key:value multiline format)
- ✅ Info file writing
- ⏳ ERG binary file parsing (planned)
- ⏳ ERG binary file writing (planned)

## Info File Format

Info files (e.g., .erg.info, .dat.info) use a simple key-value format with two syntaxes:

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
- `test_infofile` - Test suite

### Running Tests

```bash
# Run basic unit tests
./build/test_infofile

# Run tests with a real erg.info file
./build/test_infofile example/result.erg.info
```

## Usage

### Basic Example

```c
#include <infofile.h>

int main() {
    InfoFile info;
    infofile_init(&info);

    // Parse a file
    if (infofile_parse_file("data.erg.info", &info) == 0) {
        // Get values
        const char *format = infofile_get(&info, "File.Format");
        printf("Format: %s\n", format);

        // Set or update values
        infofile_set(&info, "MyKey", "MyValue");

        // Write to a new file
        infofile_write_file("output.erg.info", &info);
    }

    // Clean up
    infofile_free(&info);
    return 0;
}
```

### API Reference

#### Initialization and Cleanup

```c
void infofile_init(InfoFile *info);
```

Initialize an InfoFile structure. Must be called before using the structure.

```c
void infofile_free(InfoFile *info);
```

Free all memory associated with an InfoFile structure.

#### Parsing

```c
int infofile_parse_file(const char *filename, InfoFile *info);
```

Parse an info file from disk. Returns 0 on success, -1 on error.

```c
int infofile_parse_string(const char *data, size_t len, InfoFile *info);
```

Parse an info file from a string buffer. Returns 0 on success, -1 on error.

#### Accessing Data

```c
const char *infofile_get(const InfoFile *info, const char *key);
```

Get a value by key. Returns NULL if the key is not found.

```c
int infofile_set(InfoFile *info, const char *key, const char *value);
```

Set or update a key-value pair. Returns 0 on success, -1 on error.

#### Writing

```c
int infofile_write_file(const char *filename, const InfoFile *info);
```

Write an InfoFile structure to a file. Returns 0 on success, -1 on error.

```c
char *infofile_write_string(const InfoFile *info);
```

Write an InfoFile structure to a string buffer (allocates memory).
Returns allocated string on success, NULL on error.
Caller must free the returned string.


## Project Structure

```
liberg/
├── include/         # Public header files
│   └── infofile.h
├── src/            # Implementation files
│   └── infofile.c
├── test/           # Test suite
│   └── test_infofile.c
├── example/        # Sample data files
│   ├── result.erg
│   └── result.erg.info
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

Contributions are welcome! This is the initial implementation focused on info file parsing.
Future work includes parsing the binary ERG format itself.
