#include <stdlib.h>
#include <string.h>
#include <json99/json_dynamic_buffer_writer.h>
#include "json_assert.h"
#include "json_require.h"
#include "json_try.h"

static int
json_dynamic_buffer_writer_reserve(
  uint8_t** data,
  size_t* capacity,
  size_t required_capacity
) {
  JSON_ASSERT_NE(data, NULL);
  JSON_ASSERT_NE(capacity, NULL);

  if (*capacity >= required_capacity) {
    return JSON_OK;
  }

  size_t new_capacity = *capacity;

  if (new_capacity == 0) {
    new_capacity = 8;
  }

  while (new_capacity < required_capacity) {
    JSON_REQUIRE_LTE(new_capacity, SIZE_MAX / 2, JSON_ERROR_OVERFLOW);
    new_capacity *= 2;
  }

  void* new_data = realloc(*data, new_capacity);

  JSON_REQUIRE_NE(new_data, NULL, JSON_ERROR_NO_MEMORY);

  *data = new_data;
  *capacity = new_capacity;
  return JSON_OK;
}

static int
json_dynamic_buffer_writer_write(
  void* user,
  uint8_t c
) {
  JsonDynamicBufferWriter* writer = user;

  JSON_ASSERT_NE(writer, NULL);
  JSON_ASSERT_LT(writer->position, SIZE_MAX);

  JSON_TRY(json_dynamic_buffer_writer_reserve(
    &writer->data,
    &writer->capacity,
    writer->position + 1
  ));

  JSON_ASSERT_NE(writer->data, NULL);

  writer->data[writer->position++] = c;

  return JSON_OK;
}

void
json_dynamic_buffer_writer_init(
  JsonDynamicBufferWriter* out_writer
) {
  JSON_ASSERT_NE(out_writer, NULL);

  *out_writer = (JsonDynamicBufferWriter) {
    .writer = {
      .user = out_writer,
      .write = json_dynamic_buffer_writer_write,
    },
  };
}

void
json_dynamic_buffer_writer_deinit(
  JsonDynamicBufferWriter* writer
) {
  JSON_ASSERT_NE(writer, NULL);

  free(writer->data);

  memset(writer, 0, sizeof(*writer));
}

size_t
json_dynamic_buffer_writer_get_size(
  const JsonDynamicBufferWriter* writer
) {
  JSON_ASSERT_NE(writer, NULL);

  return writer->position;
}

int
json_dynamic_buffer_writer_copy_bytes(
  const JsonDynamicBufferWriter* writer,
  void* buffer,
  size_t capacity
) {
  JSON_ASSERT_NE(writer, NULL);

  JSON_REQUIRE_LTE(writer->position, capacity, JSON_ERROR_EOF);

  if (writer->position > 0) {
    JSON_ASSERT_NE(buffer, NULL);
    memcpy(buffer, writer->data, writer->position);
  }

  return JSON_OK;
}

int
json_dynamic_buffer_writer_copy_cstring(
  const JsonDynamicBufferWriter* writer,
  char* buffer,
  size_t capacity
) {
  JSON_ASSERT_NE(writer, NULL);
  JSON_ASSERT_NE(buffer, NULL);

  JSON_REQUIRE_LT(writer->position, capacity, JSON_ERROR_EOF);

  for (size_t i = 0; i < writer->position; i++) {
    JSON_REQUIRE_NE(writer->data[i], '\0', JSON_ERROR_INVALID);
  }

  if (writer->position > 0) {
    memcpy(buffer, writer->data, writer->position);
  }

  buffer[writer->position] = '\0';

  return JSON_OK;
}

void
json_dynamic_buffer_writer_reset(
  JsonDynamicBufferWriter* writer
) {
  JSON_ASSERT_NE(writer, NULL);

  writer->position = 0;
}
