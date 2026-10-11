#ifndef JSON_READER_H
#define JSON_READER_H

#include <stdint.h>

/** Caller-configured reader callbacks. Both callbacks must be non-`NULL`. */
struct JsonReader {

  /** Borrowed context passed unchanged to each callback. */
  void* user;

  /**
   * Consumes one byte, optionally storing it in `out_read` when non-`NULL`.
   * Returns `JSON_OK` on success or an error code on failure. Reader callbacks
   * should report exhausted input as `JSON_ERROR_EOF`.
   */
  int (*read)(
    void* user,
    uint8_t* out_read
  );

  /**
   * Inspects the next byte without consuming it. Either output may be `NULL`,
   * but at least one must be non-`NULL`. At EOF, returns `JSON_OK`, sets
   * `out_at_end` to `true` when supplied, and leaves `out_peeked` unchanged.
   * Otherwise sets the supplied outputs to the byte and `false`.
   */
  int (*peek)(
    void* user,
    uint8_t* out_peeked,
    bool* out_at_end
  );
};

#endif /* JSON_READER_H */
