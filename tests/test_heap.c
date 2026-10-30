#include <stdlib.h>
#include <string.h>
#include <json99/json.h>
#include <sio99/sio_buffer_reader.h>
#include "test.h"

int
main(
  void
) {
  JsonParser parser;
  SioBufferReader reader;
  JsonValue item;
  char* cstring;
  void* bytes;
  size_t size;

  sio_buffer_reader_init(&reader, "\"\"", 2);
  CHECK_OK(json_parser_init((SioReader*) &reader, 0, &parser, &item));
  CHECK_OK(json_value_get_cstring(&item, &cstring));
  CHECK_NE(cstring, NULL);
  CHECK_EQ(strcmp(cstring, ""), 0);
  free(cstring);

  sio_buffer_reader_init(&reader, "\"test\"", 6);
  CHECK_OK(json_parser_init((SioReader*) &reader, 0, &parser, &item));
  CHECK_OK(json_value_get_cstring(&item, &cstring));
  CHECK_NE(cstring, NULL);
  CHECK_EQ(strcmp(cstring, "test"), 0);
  free(cstring);

  sio_buffer_reader_init(&reader, "\"\\u0000\"", 8);
  CHECK_OK(json_parser_init((SioReader*) &reader, 0, &parser, &item));
  CHECK_EQ(json_value_get_cstring(&item, &cstring), JSON_ERROR_INVALID);
  CHECK_EQ(cstring, NULL);

  sio_buffer_reader_init(&reader, "\"\\u0000\"", 8);
  CHECK_OK(json_parser_init((SioReader*) &reader, 0, &parser, &item));
  CHECK_OK(json_value_get_bytes(&item, &bytes, &size));
  CHECK_NE(bytes, NULL);
  CHECK_EQ(size, 1);
  CHECK_EQ(memcmp(bytes, "\0", size), 0);
  free(bytes);

  return EXIT_SUCCESS;
}

