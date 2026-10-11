#ifndef JSON_WRITER_H
#define JSON_WRITER_H

#include <stdint.h>

/**
 * Writes the byte `c`, returning `JSON_OK` on success or an error code.
 * `user` is the context supplied by `JsonWriter`.
 */
typedef int (*JsonWriterWriteFn)(
  void* user,
  uint8_t c
);

/** Caller-configured writer callback, which must be non-`NULL`. */
struct JsonWriter {
  /** Borrowed context passed unchanged to the callback. */
  void* user;
  /** Writes one byte. */
  JsonWriterWriteFn write;
};

#endif /* JSON_WRITER_H */
