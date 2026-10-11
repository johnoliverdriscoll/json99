#ifndef JSON_BUFFER_WRITER_H
#define JSON_BUFFER_WRITER_H

#include <stddef.h>
#include <json99/json.h>

/**
 * Caller-owned writer over borrowed, fixed-capacity storage.
 * May be cast to `JsonWriter*` for reading strings or numbers. Keep
 * this object at a stable address while in use because its callback context
 * points back to it.
 */
typedef struct JsonBufferWriter JsonBufferWriter;

/**
 * Initializes `out_writer` over `capacity` bytes of caller-owned `data`,
 * with zero bytes written. `data` must be non-`NULL` and remain valid while
 * writing. Writes beyond capacity return `JSON_ERROR_EOF` without modifying
 * the buffer or written size. No terminator is added.
 */
void json_buffer_writer_init(
  JsonBufferWriter* out_writer,
  void* data,
  size_t capacity
);

/**
 * Returns the number of bytes written, not the buffer capacity.
 * `out_writer` must be non-`NULL`. Does not reset the writer.
 */
size_t json_buffer_writer_get_size(
  JsonBufferWriter* out_writer
);

/**
 * Resets the number of bytes written to zero. Subsequent writes start at the
 * beginning of the buffer. `writer` must be non-`NULL` and initialized.
 */
void json_buffer_writer_reset(
  JsonBufferWriter* writer
);

/** Adapter state, initialized by `json_buffer_writer_init`. */
struct JsonBufferWriter {
  /** Writer interface for reading strings or numbers. */
  JsonWriter writer;
  /** Borrowed output buffer that is not freed by this writer. */
  uint8_t* data;
  /** Output capacity in bytes. */
  size_t capacity;
  /** Number of bytes written. */
  size_t position;
};

#endif /* JSON_BUFFER_WRITER_H */
