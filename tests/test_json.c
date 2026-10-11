#include <math.h>
#include <string.h>
#include <json99/json_buffer_reader.h>
#include <json99/json_buffer_writer.h>
#include "test.h"

static int
validate(
  const char* text,
  unsigned depth
) {
  JsonBufferReader reader;
  JsonParser parser;
  JsonValue value;
  json_buffer_reader_init(&reader, text, strlen(text));
  int result = json_parser_init((JsonReader*) &reader, depth, &parser, &value);
  if (result == JSON_OK) {
    return json_value_advance(&value);
  }
  return result;
}

int
main(
  void
) {
  const char text[] =
    "["
    "  true,"
    "  null,"
    "  -9223372036854775808,"
    "  1.25e2,"
    "  \"a\\n\\u20ac\\ud83d\\ude00\","
    "  [1,{\"x\":false}],"
    "  7"
    "]";

  JsonParser parser;
  JsonBufferReader reader;
  JsonBufferWriter writer;
  JsonValue root, item;
  bool boolean;
  int64_t integer;
  double real;
  char buffer[16];
  const char expected[] = "a\n\xe2\x82\xac\xf0\x9f\x98\x80";

  json_buffer_reader_init(&reader, text, sizeof(text) - 1);
  json_buffer_writer_init(&writer, buffer, sizeof(buffer));
  CHECK_OK(json_parser_init((JsonReader*) &reader, 8, &parser, &root));

  CHECK(json_value_is_array(&root));
  CHECK_OK(json_value_enter_container(&root, &item));
  CHECK_OK(json_value_get_bool(&item, &boolean));
  CHECK(boolean);
  CHECK(json_value_is_null(&item));
  CHECK_OK(json_value_advance(&item));
  CHECK_OK(json_value_get_int64(&item, &integer));
  CHECK(integer == INT64_MIN);
  CHECK_OK(json_value_get_double(&item, &real));
  CHECK_EQ(real, 125.0);
  CHECK_OK(json_value_read_string(&item, (JsonWriter*) &writer));
  CHECK_EQ(writer.position, sizeof(expected) - 1);
  CHECK_EQ(memcmp(buffer, expected, writer.position), 0);
  CHECK_OK(json_value_advance(&item));
  CHECK_OK(json_value_get_int64(&item, &integer));
  CHECK_EQ(integer, 7);
  CHECK(json_value_at_end(&item));
  CHECK_OK(json_value_leave_container(&root, &item));
  CHECK(json_value_at_end(&root));

  const char* invalid[] = {
    "[1,]",
    "{\"a\" 1}",
    "[01]",
    "[1e]",
    "[true false]",
    "\"\\ud800\"",
    "\"\\x\"",
  };
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    CHECK_EQ(validate(invalid[i], 8), JSON_ERROR_INVALID);
  }
  CHECK_EQ(validate("[1", 8), JSON_ERROR_EOF);
  CHECK_OK(validate("[[0]]", 2));
  CHECK_EQ(validate("[[0]]", 1), JSON_ERROR_DEPTH_LIMIT);

  const char* numbers[] = {
    "9223372036854775808",
    "-9223372036854775809",
    "1.5",
  };
  for (size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
    json_buffer_reader_init(&reader, numbers[i], strlen(numbers[i]));
    CHECK_OK(json_parser_init((JsonReader*) &reader, 8, &parser, &root));
    CHECK_EQ(json_value_get_int64(&root, &integer), JSON_ERROR_OUT_OF_RANGE);
  }
  json_buffer_reader_init(&reader, "-0", 2);
  CHECK_OK(json_parser_init(&reader.reader, 8, &parser, &root));
  CHECK_OK(json_value_get_double(&root, &real));
  CHECK_EQ(real, 0.0);
  CHECK(signbit(real));
  json_buffer_reader_init(&reader, "1e9999", 6);
  CHECK_OK(json_parser_init(&reader.reader, 8, &parser, &root));
  CHECK_EQ(json_value_get_double(&root, &real), JSON_ERROR_OUT_OF_RANGE);

  uint8_t byte;
  bool at_end;
  json_buffer_reader_init(&reader, "x", 1);
  CHECK_OK(reader.reader.peek(reader.reader.user, &byte, &at_end));
  CHECK_EQ(byte, 'x');
  CHECK(!at_end);
  CHECK_EQ(reader.position, 0);
  CHECK_OK(reader.reader.read(reader.reader.user, &byte));
  CHECK_EQ(byte, 'x');
  CHECK_OK(reader.reader.peek(reader.reader.user, NULL, &at_end));
  CHECK(at_end);
  CHECK_EQ(reader.reader.read(reader.reader.user, &byte), JSON_ERROR_EOF);
  json_buffer_writer_init(&writer, buffer, 1);
  CHECK_OK(writer.writer.write(writer.writer.user, 'a'));
  CHECK_EQ(writer.writer.write(writer.writer.user, 'b'), JSON_ERROR_EOF);
  CHECK_EQ(writer.position, 1);
  CHECK_EQ(json_buffer_writer_get_size(&writer), writer.position);
  CHECK_EQ(buffer[0], 'a');

  return EXIT_SUCCESS;
}
