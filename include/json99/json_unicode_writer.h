#ifndef JSON_UNICODE_WRITER_H
#define JSON_UNICODE_WRITER_H

#include <json99/json.h>

/**
 * Abstract Unicode sink. All callbacks are required. The caller owns the
 * context. No destination or encoding of output is prescribed by this type.
 * Implementations validate UTF-8 bytes and UTF-16 surrogate pairs, rejecting
 * a switch between input forms while a sequence is incomplete. `finish`
 * rejects incomplete input. Callback errors propagate to the producer.
 */
struct JsonUnicodeWriter {

  /** Borrowed context passed unchanged to each callback. */
  void* user;

  /** Writes one raw UTF-8 byte. */
  int (*write_utf8)(
    void* user,
    uint8_t byte
  );

  /** Writes one numeric UTF-16 code unit, without byte-order interpretation. */
  int (*write_utf16)(
    void* user,
    uint16_t unit
  );

  /** Checks that the sequence is complete. Does not close any destination. */
  int (*finish)(
    void* user
  );
};

#endif /* JSON_UNICODE_WRITER_H */
