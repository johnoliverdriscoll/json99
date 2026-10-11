#include <json99/json_buffer_writer.h>
#include "json_assert.h"
#include "json_require.h"

static int
json_buffer_writer_write(
  void* user,
  uint8_t c
) {
  JsonBufferWriter* writer = user;

  JSON_ASSERT_NE(writer, NULL);
  JSON_ASSERT_NE(writer->data, NULL);

  JSON_REQUIRE_LT(writer->position, writer->capacity, JSON_ERROR_EOF);

  writer->data[writer->position++] = c;

  return JSON_OK;
}

void
json_buffer_writer_init(
  JsonBufferWriter* out_writer,
  void* data,
  size_t capacity
) {
  JSON_ASSERT_NE(out_writer, NULL);

  *out_writer = (JsonBufferWriter) {
    .writer = {
      .user = out_writer,
      .write = json_buffer_writer_write,
    },
    .data = data,
    .capacity = capacity,
  };
}

size_t
json_buffer_writer_get_size(
  JsonBufferWriter* writer
) {
  JSON_ASSERT_NE(writer, NULL);

  return writer->position;
}

void
json_buffer_writer_reset(
  JsonBufferWriter* writer
) {
  JSON_ASSERT_NE(writer, NULL);

  writer->position = 0;
}
