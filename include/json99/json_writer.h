#ifndef JSON_WRITER_H
#define JSON_WRITER_H

#include <stdint.h>

/** Caller-configured writer callback, which must be non-`NULL`. */
struct JsonWriter {

  /** Borrowed context passed unchanged to the callback. */
  void* user;

  /**
   * Writes the byte `c`, returning `JSON_OK` on success or an error code.
   * `user` is the context supplied by `JsonWriter`.
   */
  int (*write)(
    void* user,
    uint8_t c
  );
};

#endif /* JSON_WRITER_H */
