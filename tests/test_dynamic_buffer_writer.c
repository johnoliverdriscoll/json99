#include <string.h>
#include <json99/json_dynamic_buffer_writer.h>
#include "test.h"

int
main(
  void
) {
  JsonDynamicBufferWriter writer;
  void* data;
  size_t size;

  CHECK_NE(data = malloc(1026), NULL);

  json_dynamic_buffer_writer_init(&writer);
  size = json_dynamic_buffer_writer_get_size(&writer);
  CHECK_EQ(size, 0);
  for (size_t i = 0; i < 1025; i++) {
    CHECK_OK(writer.writer.write(writer.writer.user, (uint8_t) i));
  }
  size = json_dynamic_buffer_writer_get_size(&writer);
  CHECK_EQ(size, 1025);
  memset(data, 0x7f, 1026);
  CHECK_EQ(
    json_dynamic_buffer_writer_copy_bytes(&writer, data, 1024),
    JSON_ERROR_EOF
  );
  for (size_t i = 0; i < 1025; i++) {
    CHECK_EQ(((uint8_t*) data)[i], 0x7f);
  }
  CHECK_EQ(
    json_dynamic_buffer_writer_copy_bytes(&writer, data, 0),
    JSON_ERROR_EOF
  );
  for (size_t i = 0; i < 1025; i++) {
    CHECK_EQ(((uint8_t*) data)[i], 0x7f);
  }
  CHECK_OK(json_dynamic_buffer_writer_copy_bytes(&writer, data, 1026));
  for (size_t i = 0; i < size; i++) {
    CHECK_EQ(((uint8_t*) data)[i], (uint8_t) i);
  }
  CHECK_EQ(
    json_dynamic_buffer_writer_copy_cstring(&writer, data, 1026),
    JSON_ERROR_INVALID
  );
  json_dynamic_buffer_writer_deinit(&writer);

  json_dynamic_buffer_writer_init(&writer);
  const char string[] = "Hello, world!";
  for (size_t i = 0; i < sizeof(string) - 1; i++) {
    CHECK_OK(writer.writer.write(writer.writer.user, (uint8_t) string[i]));
  }
  memset(data, 0x7f, 1026);
  CHECK_EQ(
    json_dynamic_buffer_writer_copy_cstring(&writer, data, sizeof(string) - 1),
    JSON_ERROR_EOF
  );
  for (size_t i = 0; i < 1025; i++) {
    CHECK_EQ(((uint8_t*) data)[i], 0x7f);
  }
  CHECK_EQ(
    json_dynamic_buffer_writer_copy_cstring(&writer, data, 0),
    JSON_ERROR_EOF
  );
  for (size_t i = 0; i < 1025; i++) {
    CHECK_EQ(((uint8_t*) data)[i], 0x7f);
  }
  CHECK_OK(json_dynamic_buffer_writer_copy_cstring(&writer, data, 1026));
  CHECK_EQ(strcmp(string, data), 0);
  json_dynamic_buffer_writer_deinit(&writer);

  json_dynamic_buffer_writer_init(&writer);
  CHECK_EQ(
    json_dynamic_buffer_writer_copy_cstring(&writer, data, 0),
    JSON_ERROR_EOF
  );
  CHECK_OK(json_dynamic_buffer_writer_copy_bytes(&writer, NULL, 0));
  CHECK_OK(json_dynamic_buffer_writer_copy_cstring(&writer, data, 1));
  CHECK_EQ(strcmp("", data), 0);
  json_dynamic_buffer_writer_deinit(&writer);

  free(data);
  
  return EXIT_SUCCESS;
}
