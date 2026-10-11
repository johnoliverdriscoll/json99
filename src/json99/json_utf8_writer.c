#include <json99/json_utf8_writer.h>
#include "json_assert.h"
#include "json_try.h"

static int
json_utf8_writer_result(
  JsonUtf8Writer* writer,
  int result
) {
  if (result == UTF_ERROR_INVALID) {
    writer->error = JSON_ERROR_INVALID;
  } else if (result == UTF_ERROR_WRITE) {
    writer->error = JSON_ERROR_IO;
  }
  return writer->error;
}

static int
json_utf8_writer_write_utf8(
  void* user,
  uint8_t byte
) {
  JsonUtf8Writer* writer = user;
  JSON_TRY(writer->error);
  return json_utf8_writer_result(
    writer,
    utf_decoder_write_utf8(&writer->decoder, byte)
  );
}

static int
json_utf8_writer_write_utf16(
  void* user,
  uint16_t unit
) {
  JsonUtf8Writer* writer = user;
  JSON_TRY(writer->error);
  return json_utf8_writer_result(
    writer,
    utf_decoder_write_utf16(&writer->decoder, unit)
  );
}

static int
json_utf8_writer_finish(
  void* user
) {
  JsonUtf8Writer* writer = user;
  JSON_TRY(writer->error);
  return json_utf8_writer_result(writer, utf_decoder_finish(&writer->decoder));
}

void
json_utf8_writer_init(
  JsonUtf8Writer* out_writer,
  SioWriter* destination
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
  };
  utf_decoder_init(&out_writer->decoder, destination);
}
