#include <stdlib.h>
#include <json99/json.h>
#include <sio99/sio_dynamic_buffer_writer.h>
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

  SioDynamicBufferWriter writer;
  sio_dynamic_buffer_writer_init(&writer);

  int res;

  res = json_value_read_string(it, (SioWriter*) &writer);
  if (res != JSON_OK) {
    sio_dynamic_buffer_writer_deinit(&writer);
    JSON_TRY(res);
  }

  size_t size = sio_dynamic_buffer_writer_get_size(&writer);

  if (size == 0) {
    sio_dynamic_buffer_writer_deinit(&writer);
    return JSON_OK;
  }

  *out_bytes = malloc(size);
  if (*out_bytes == NULL) {
    sio_dynamic_buffer_writer_deinit(&writer);
    JSON_TRY(JSON_ERROR_NO_MEMORY);
  }

  sio_dynamic_buffer_writer_copy_bytes(&writer, *out_bytes);

  *out_size = size;

  sio_dynamic_buffer_writer_deinit(&writer);
  return JSON_OK;
}

int
json_value_get_cstring(
  JsonValue* it,
  char** out_cstring
) {
  JSON_ASSERT_NE(out_cstring, NULL);

  *out_cstring = NULL;

  SioDynamicBufferWriter writer;
  sio_dynamic_buffer_writer_init(&writer);
  int res;

  res = json_value_read_string(it, (SioWriter*) &writer);
  if (res != JSON_OK) {
    sio_dynamic_buffer_writer_deinit(&writer);
    JSON_TRY(res);
  }

  size_t size = sio_dynamic_buffer_writer_get_size(&writer);

  *out_cstring = malloc(size + 1);
  if (*out_cstring == NULL) {
    sio_dynamic_buffer_writer_deinit(&writer);
    JSON_TRY(JSON_ERROR_NO_MEMORY);
  }

  res = sio_dynamic_buffer_writer_copy_cstring(
    &writer,
    *out_cstring
  );
  if (res != SIO_OK) {
    sio_dynamic_buffer_writer_deinit(&writer);
    free(*out_cstring);
    *out_cstring = NULL;
    return res == SIO_ERROR_INVALID ? JSON_ERROR_INVALID : JSON_ERROR_IO;
  }

  sio_dynamic_buffer_writer_deinit(&writer);
  return JSON_OK;
}
