#include <json99/json_buffer_reader.h>
#include "json_assert.h"
#include "json_require.h"

static int
json_buffer_reader_read(
  void* user,
  uint8_t* out
) {
  JsonBufferReader* reader = user;

  JSON_ASSERT_NE(reader, NULL);
  JSON_ASSERT_NE(reader->data, NULL);

  JSON_REQUIRE_LT(reader->position, reader->size, JSON_ERROR_EOF);

  if (out != NULL) {
    *out = reader->data[reader->position];
  }

  reader->position++;

  return JSON_OK;
}

static int
json_buffer_reader_peek(
  void* user,
  uint8_t* out_peeked,
  bool* out_at_end
) {
  JSON_ASSERT(out_peeked != NULL || out_at_end != NULL);

  JsonBufferReader* reader = user;

  JSON_ASSERT_NE(reader, NULL);
  JSON_ASSERT_NE(reader->data, NULL);

  if (reader->position < reader->size) {
    if (out_peeked != NULL) {
      *out_peeked = reader->data[reader->position];
    }
    if (out_at_end != NULL) {
      *out_at_end = false;
    }
  } else if (out_at_end != NULL) {
    *out_at_end = true;
  }

  return JSON_OK;
}

void
json_buffer_reader_init(
  JsonBufferReader* out_reader,
  const void* data,
  size_t size
) {
  JSON_ASSERT_NE(out_reader, NULL);

  *out_reader = (JsonBufferReader) {
    .reader = {
      .user = out_reader,
      .read = json_buffer_reader_read,
      .peek = json_buffer_reader_peek,
    },
    .data = data,
    .size = size,
  };
}
