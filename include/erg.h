#ifndef ERG_H
#define ERG_H

#include <stddef.h>
#include <stdint.h>
#include <infofile.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ERG (CarMaker binary results) file parser
 *
 * Reads binary ERG files containing simulation results from CarMaker.
 * Each ERG file has a companion .erg.info file with metadata.
 */

/**
 * Data type mapping from CarMaker to C types
 */
typedef enum {
    ERG_FLOAT,      // 4 bytes
    ERG_DOUBLE,     // 8 bytes
    ERG_LONGLONG,   // 8 bytes signed
    ERG_ULONGLONG,  // 8 bytes unsigned
    ERG_INT,        // 4 bytes signed
    ERG_UINT,       // 4 bytes unsigned
    ERG_SHORT,      // 2 bytes signed
    ERG_USHORT,     // 2 bytes unsigned
    ERG_CHAR,       // 1 byte signed
    ERG_UCHAR,      // 1 byte unsigned
    ERG_BYTES,      // Raw bytes (1-8)
    ERG_UNKNOWN
} ERGDataType;

/**
 * Metadata for a single signal/channel
 */
typedef struct {
    char*        name;      /* Signal name (e.g., "Time", "Car.v") */
    ERGDataType  type;      /* Data type */
    size_t       type_size; /* Size in bytes */
    char*        unit;      /* Unit string (e.g., "m/s", "s") */
    double       factor;    /* Scaling factor */
    double       offset;    /* Scaling offset */
} ERGSignal;

/**
 * Main ERG file structure
 */
typedef struct {
    char*         erg_path;       /* Path to .erg file */
    InfoFile*     info;           /* Parsed .erg.info file */

    ERGSignal*    signals;        /* Array of signal metadata */
    size_t        signal_count;   /* Number of signals */

    void*         raw_data;       /* Raw binary data buffer */
    size_t        data_size;      /* Size of data buffer */
    size_t        sample_count;   /* Number of samples/rows */

    int           little_endian;  /* 1 if little-endian, 0 if big-endian */
    size_t        row_size;       /* Size of one data row in bytes */
} ERG;

/**
 * Initialize an ERG structure
 * Does not load data - call erg_parse() to load
 */
void erg_init(ERG* erg, const char* erg_file_path);

/**
 * Parse the ERG file and load all data
 * Reads both .erg and .erg.info files
 * Exits on error with descriptive message
 */
void erg_parse(ERG* erg);

/**
 * Get signal data by name
 * Returns pointer to data array and sets length
 * Data is in raw binary format - caller must cast based on signal type
 *
 * @param erg Pointer to ERG structure
 * @param signal_name Name of signal (e.g., "Time", "Car.v")
 * @param length Output parameter for array length (number of samples)
 * @return Pointer to raw data array, NULL if signal not found
 */
const void* erg_get_signal(const ERG* erg, const char* signal_name, size_t* length);

/**
 * Get signal data as double array (with scaling applied)
 * Allocates new array - caller must free
 *
 * @param erg Pointer to ERG structure
 * @param signal_name Name of signal
 * @param length Output parameter for array length
 * @return Pointer to newly allocated double array, NULL if signal not found
 */
double* erg_get_signal_as_double(const ERG* erg, const char* signal_name, size_t* length);

/**
 * Get signal metadata by name
 *
 * @param erg Pointer to ERG structure
 * @param signal_name Name of signal
 * @return Pointer to signal metadata, NULL if not found
 */
const ERGSignal* erg_get_signal_info(const ERG* erg, const char* signal_name);

/**
 * Get index of signal by name
 *
 * @param erg Pointer to ERG structure
 * @param signal_name Name of signal
 * @return Index of signal, or -1 if not found
 */
int erg_find_signal_index(const ERG* erg, const char* signal_name);

/**
 * Free all memory associated with ERG structure
 */
void erg_free(ERG* erg);

#ifdef __cplusplus
}
#endif

#endif /* ERG_H */
