#ifndef JSON_BUFFER_READER_H
#define JSON_BUFFER_READER_H

#include <stddef.h>
#include <json99/json.h>

/**
 * Caller-owned reader over borrowed, immutable bytes. Uses no allocations.
 * May be cast to `JsonReader*` for use with the parser. Keep this object at a
 * stable address while in use because its callback context points back to it.
 */
typedef struct JsonBufferReader JsonBufferReader;

/**
 * Initializes `out_reader` to read `size` bytes from `data`, starting at zero.
 * `data` must be non-`NULL` and remain valid while reading, even for size zero.
 * Reads at the end return `JSON_ERROR_EOF`, while peeks report EOF without
 * error.
 */
void json_buffer_reader_init(
  JsonBufferReader* out_reader,
  const void* data,
  size_t size
);

/** Adapter state, initialized by `json_buffer_reader_init`. */
struct JsonBufferReader {
  /** Reader interface passed to the parser. */
  JsonReader reader;
  /** Borrowed input buffer that is not freed by this reader. */
  const uint8_t* data;
  /** Input length in bytes. */
  size_t size;
  /** Number of bytes consumed. */
  size_t position;
};

#endif /* JSON_BUFFER_READER_H */
