#ifndef JSON_UTF8_WRITER_H
#define JSON_UTF8_WRITER_H

#include <json99/json_unicode_writer.h>
#include <json99/json_writer.h>

/**
 * Caller-owned, allocation-free implementation of `JsonUnicodeWriter`.
 * Validates UTF-8 bytes and converts UTF-16 units into UTF-8 output. May be
 * cast to `JsonUnicodeWriter*`. Keep it at a stable address while in use.
 */
typedef struct JsonUtf8Writer JsonUtf8Writer;

/**
 * Initializes or resets the converter, borrowing `destination`. A `NULL`
 * destination validates and discards. Output contains no added terminator.
 * Complete scalar values are emitted as they arrive. Destination failures
 * may leave partial output. Invalid input returns `JSON_ERROR_INVALID`.
 * All errors are latched until reinitialization, including destination errors.
 *
 * Call the interface's `finish` callback at the end of input to detect an
 * unfinished UTF-8 sequence or unmatched surrogate. Successful finishing is
 * idempotent and does not reset the converter or close the destination.
 */
void json_utf8_writer_init(
  JsonUtf8Writer* out_writer,
  JsonWriter* destination
);

/** Internal conversion state exposed for caller allocation. Do not modify. */
struct JsonUtf8Writer {
  JsonUnicodeWriter writer;
  JsonWriter* destination;
  uint8_t bytes[4];
  unsigned size;
  unsigned expected;
  uint16_t high_surrogate;
  int error;
};

#endif /* JSON_UTF8_WRITER_H */
