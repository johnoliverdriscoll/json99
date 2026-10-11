#include <stdlib.h>
#include <json99/json.h>
#include <json99/json_dynamic_buffer_writer.h>
#include "json_assert.h"
#include "json_require.h"
#include "json_try.h"

int
json_value_get_bytes(
  JsonValue* it,
  void** out_bytes,
  size_t* out_size
) {
  JSON_ASSERT_NE(out_bytes, NULL);
  JSON_ASSERT_NE(out_size, NULL);

  *out_bytes = NULL;
  *out_size = 0;

  JsonDynamicBufferWriter writer;
  json_dynamic_buffer_writer_init(&writer);

  int res;
  
  res = json_value_read_string(it, (JsonWriter*) &writer);
  if (res != JSON_OK) {
    json_dynamic_buffer_writer_deinit(&writer);
    JSON_TRY(res);
  }

  size_t size = json_dynamic_buffer_writer_get_size(&writer);

  if (size == 0) {
    return JSON_OK;
  }

  *out_bytes = malloc(size);
  if (*out_bytes == NULL) {
    json_dynamic_buffer_writer_deinit(&writer);
    JSON_TRY(JSON_ERROR_NO_MEMORY);
  }

  res = json_dynamic_buffer_writer_copy_bytes(&writer, *out_bytes, size);
  if (res != JSON_OK) {
    json_dynamic_buffer_writer_deinit(&writer);
    free(*out_bytes);
    *out_bytes = NULL;
    JSON_TRY(res);
  }

  *out_size = size;

  json_dynamic_buffer_writer_deinit(&writer);
  return JSON_OK;
}

int
json_value_get_cstring(
  JsonValue* it,
  char** out_cstring
) {
  JSON_ASSERT_NE(out_cstring, NULL);

  *out_cstring = NULL;

  JsonDynamicBufferWriter writer;
  json_dynamic_buffer_writer_init(&writer);
  int res;

  res = json_value_read_string(it, (JsonWriter*) &writer);
  if (res != JSON_OK) {
    json_dynamic_buffer_writer_deinit(&writer);
    JSON_TRY(res);
  }

  size_t size = json_dynamic_buffer_writer_get_size(&writer);

  *out_cstring = malloc(size + 1);
  if (*out_cstring == NULL) {
    json_dynamic_buffer_writer_deinit(&writer);
    JSON_TRY(JSON_ERROR_NO_MEMORY);
  }

  res = json_dynamic_buffer_writer_copy_cstring(
    &writer,
    *out_cstring,
    size + 1
  );
  if (res != JSON_OK) {
    json_dynamic_buffer_writer_deinit(&writer);
    free(*out_cstring);
    *out_cstring = NULL;
    JSON_TRY(res);
  }

  json_dynamic_buffer_writer_deinit(&writer);
  return JSON_OK;
}
