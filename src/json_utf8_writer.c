#include <json99/json_utf8_writer.h>
#include "json_assert.h"
#include "json_try.h"

static int
json_utf8_writer_emit(
  JsonUtf8Writer* writer,
  const uint8_t* bytes,
  unsigned size
) {
  if (writer->destination != NULL) {
    for (unsigned i = 0; i < size; i++) {
      writer->error = writer->destination->write(
        writer->destination->user,
        bytes[i]
      );
      JSON_TRY(writer->error);
    }
  }
  return JSON_OK;
}

static int
json_utf8_writer_write_utf8(
  void* user,
  uint8_t byte
) {
  JsonUtf8Writer* writer = user;
  JSON_TRY(writer->error);

  if (writer->high_surrogate != 0) {
    return writer->error = JSON_ERROR_INVALID;
  }
  if (writer->size == 0) {
    if (byte <= 0x7f) {
      return json_utf8_writer_emit(writer, &byte, 1);
    }
    if (byte >= 0xc2 && byte <= 0xdf) {
      writer->expected = 2;
    } else if (byte >= 0xe0 && byte <= 0xef) {
      writer->expected = 3;
    } else if (byte >= 0xf0 && byte <= 0xf4) {
      writer->expected = 4;
    } else {
      return writer->error = JSON_ERROR_INVALID;
    }
  } else {
    if (byte < 0x80 || byte > 0xbf) {
      return writer->error = JSON_ERROR_INVALID;
    }
    if (writer->size == 1) {
      uint8_t lead = writer->bytes[0];
      if ((lead == 0xe0 && byte < 0xa0)
        || (lead == 0xed && byte > 0x9f)
        || (lead == 0xf0 && byte < 0x90)
        || (lead == 0xf4 && byte > 0x8f)) {
        return writer->error = JSON_ERROR_INVALID;
      }
    }
  }
  writer->bytes[writer->size++] = byte;
  if (writer->size == writer->expected) {
    JSON_TRY(json_utf8_writer_emit(writer, writer->bytes, writer->size));
    writer->size = 0;
  }
  return JSON_OK;
}

static int
json_utf8_writer_write_utf16(
  void* user,
  uint16_t unit
) {
  JsonUtf8Writer* writer = user;
  JSON_TRY(writer->error);

  if (writer->size != 0) {
    return writer->error = JSON_ERROR_INVALID;
  }
  uint32_t codepoint = unit;
  if (writer->high_surrogate != 0) {
    if (unit < 0xdc00 || unit > 0xdfff) {
      return writer->error = JSON_ERROR_INVALID;
    }
    codepoint = 0x10000u
      + (((uint32_t) writer->high_surrogate - 0xd800u) << 10)
      + ((uint32_t) unit - 0xdc00u);
    writer->high_surrogate = 0;
  } else if (unit >= 0xd800 && unit <= 0xdbff) {
    writer->high_surrogate = unit;
    return JSON_OK;
  } else if (unit >= 0xdc00 && unit <= 0xdfff) {
    return writer->error = JSON_ERROR_INVALID;
  }

  uint8_t bytes[4];
  unsigned size;
  if (codepoint <= 0x7f) {
    bytes[0] = (uint8_t) codepoint;
    size = 1;
  } else if (codepoint <= 0x7ff) {
    bytes[0] = 0xc0 | (codepoint >> 6);
    bytes[1] = 0x80 | (codepoint & 0x3f);
    size = 2;
  } else if (codepoint <= 0xffff) {
    bytes[0] = 0xe0 | (codepoint >> 12);
    bytes[1] = 0x80 | ((codepoint >> 6) & 0x3f);
    bytes[2] = 0x80 | (codepoint & 0x3f);
    size = 3;
  } else {
    bytes[0] = 0xf0 | (codepoint >> 18);
    bytes[1] = 0x80 | ((codepoint >> 12) & 0x3f);
    bytes[2] = 0x80 | ((codepoint >> 6) & 0x3f);
    bytes[3] = 0x80 | (codepoint & 0x3f);
    size = 4;
  }
  return json_utf8_writer_emit(writer, bytes, size);
}

static int
json_utf8_writer_finish(
  void* user
) {
  JsonUtf8Writer* writer = user;
  JSON_TRY(writer->error);

  if (writer->size != 0 || writer->high_surrogate != 0) {
    writer->error = JSON_ERROR_INVALID;
  }
  return writer->error;
}

void
json_utf8_writer_init(
  JsonUtf8Writer* out_writer,
  JsonWriter* destination
) {
  JSON_ASSERT_NE(out_writer, NULL);
  JSON_ASSERT(destination == NULL || destination->write != NULL);

  *out_writer = (JsonUtf8Writer) {
    .writer = {
      .user = out_writer,
      .write_utf8 = json_utf8_writer_write_utf8,
      .write_utf16 = json_utf8_writer_write_utf16,
      .finish = json_utf8_writer_finish,
    },
    .destination = destination,
  };
}
