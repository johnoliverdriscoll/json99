#ifndef JSON_DYNAMIC_BUFFER_WRITER_H
#define JSON_DYNAMIC_BUFFER_WRITER_H

#include <stddef.h>
#include <json99/json.h>
#include <json99/json_writer.h>

/**
 * Caller-owned writer that owns a buffer grown with `realloc`. Release the
 * buffer with `json_dynamic_buffer_writer_deinit`, including after errors.
 * May be cast to `JsonWriter*` for reading strings or numbers. Keep
 * this object at a stable address while in use because its callback context
 * points back to it.
 */
typedef struct JsonDynamicBufferWriter JsonDynamicBufferWriter;

/**
 * Initializes an empty writer without allocating memory. Subsequent writes
 * grow its buffer with `realloc`. No terminator is added.
 * Allocation failure returns `JSON_ERROR_NO_MEMORY`. Capacity arithmetic
 * overflow returns `JSON_ERROR_OVERFLOW`.
 * Call `json_dynamic_buffer_writer_deinit` before reinitializing an existing
 * writer that owns an allocation.
 */
void json_dynamic_buffer_writer_init(
  JsonDynamicBufferWriter* out_writer
);

/**
 * Frees the owned buffer and clears `writer`, invalidating pointers into
 * that buffer. Does not free the caller-owned writer object. `writer` must
 * be non-`NULL` and have been initialized or already deinitialized. Repeated
 * calls are harmless. Call `json_dynamic_buffer_writer_init` before writing
 * again.
 */
void json_dynamic_buffer_writer_deinit(
  JsonDynamicBufferWriter* writer
);

/**
 * Returns the number of bytes written, not the allocation capacity.
 * `writer` must be non-`NULL`. Does not reset the writer.
 */
size_t json_dynamic_buffer_writer_get_size(
  const JsonDynamicBufferWriter* writer
);

/**
 * Copies the written bytes into caller-owned `buffer`, without a terminator.
 * `writer` must be non-`NULL`. For nonempty output, `buffer` must be
 * non-`NULL`. For empty output, `NULL` and zero capacity are allowed.
 * Returns `JSON_ERROR_EOF` if `capacity` is smaller than the written size.
 * Leaves the destination unchanged on error and does not reset or modify the
 * writer. The copy remains valid after the writer is deinitialized.
 */
int json_dynamic_buffer_writer_copy_bytes(
  const JsonDynamicBufferWriter* writer,
  void* buffer,
  size_t capacity
);

/**
 * Copies the written bytes into caller-owned `buffer` and appends a null
 * terminator. `writer` and `buffer` must be non-`NULL`, even for empty output.
 * Returns `JSON_ERROR_EOF` unless `capacity` exceeds the written size, or
 * `JSON_ERROR_INVALID` if the written bytes contain a null byte.
 * Empty output requires one byte for the terminator. Leaves the destination
 * unchanged on error and does not reset or modify the writer. The copy remains
 * valid after the writer is deinitialized.
 */
int json_dynamic_buffer_writer_copy_cstring(
  const JsonDynamicBufferWriter* writer,
  char* buffer,
  size_t capacity
);

/**
 * Resets the number of bytes written to zero. Subsequent writes start at the
 * beginning of the buffer. Does not change its capacity. `writer` must be
 * non-`NULL` and initialized.
 */
void json_dynamic_buffer_writer_reset(
  JsonDynamicBufferWriter* writer
);

/** Adapter state, initialized by `json_dynamic_buffer_writer_init`. */
struct JsonDynamicBufferWriter {
  /** Writer interface for reading strings or numbers. */
  JsonWriter writer;
  /** Current allocation, or `NULL` before the first allocation. */
  uint8_t* data;
  /** Allocated capacity in bytes. */
  size_t capacity;
  /** Number of bytes written. */
  size_t position;
};

#endif /* JSON_DYNAMIC_BUFFER_WRITER_H */
