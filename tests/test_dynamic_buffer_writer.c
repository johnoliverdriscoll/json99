#include <string.h>
#include <json99/json.h>
#include <sio99/sio_buffer_reader.h>
#include <sio99/sio_dynamic_buffer_writer.h>
#include "test.h"

int
main(void) {
  const char text[] = "[\"a\\u0000b\",\"hello\"]";
  SioBufferReader reader;
  sio_buffer_reader_init(&reader, text, sizeof(text) - 1);
  JsonParser parser;
  JsonValue root;
  JsonValue child;
  CHECK_OK(json_parser_init((SioReader*) &reader, 1, &parser, &root));
  CHECK_OK(json_value_enter_container(&root, &child));
  SioDynamicBufferWriter writer;
  sio_dynamic_buffer_writer_init(&writer);
  CHECK_OK(json_value_read_string(&child, (SioWriter*) &writer));
  CHECK_EQ(sio_dynamic_buffer_writer_get_size(&writer), 3);
  uint8_t bytes[3];
  sio_dynamic_buffer_writer_copy_bytes(&writer, bytes);
  CHECK_EQ(memcmp(bytes, "a\0b", sizeof(bytes)), 0);
  char string[6];
  CHECK_EQ(sio_dynamic_buffer_writer_copy_cstring(&writer, string), SIO_ERROR_INVALID);
  sio_dynamic_buffer_writer_reset(&writer);
  CHECK_OK(json_value_read_string(&child, (SioWriter*) &writer));
  CHECK_EQ(sio_dynamic_buffer_writer_get_size(&writer), 5);
  CHECK_OK(sio_dynamic_buffer_writer_copy_cstring(&writer, string));
  CHECK_EQ(strcmp(string, "hello"), 0);
  CHECK_OK(json_value_leave_container(&root, &child));
  CHECK(json_value_at_end(&root));
  sio_dynamic_buffer_writer_deinit(&writer);
  return EXIT_SUCCESS;
}
