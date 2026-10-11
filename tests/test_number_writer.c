#include <json99/json.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <sio99/sio_buffer_reader.h>
#include <sio99/sio_buffer_writer.h>
#include <json99/json_number_writer.h>
#include "test.h"

static int
read_number(
  const char* text,
  SioWriter* writer
) {
  SioBufferReader reader;
  JsonParser parser;
  JsonValue value;
  sio_buffer_reader_init(&reader, text, strlen(text));
  int result = json_parser_init((SioReader*) &reader, 1, &parser, &value);
  if (result != JSON_OK) {
    return result;
  }
  result = json_value_read_number(&value, writer);
  if (result == JSON_OK) {
    CHECK(json_value_at_end(&value));
  }
  return result;
}

static void
test_exact_text(
  void
) {
  const char* cases[] = {"-0", "1.2300E+004", "0.000", "-12e-09"};
  for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
    char output[32] = {0};
    SioBufferWriter writer;
    sio_buffer_writer_init(&writer, output, sizeof(output));
    CHECK_OK(read_number(cases[i], &writer.writer));
    CHECK_EQ(writer.position, strlen(cases[i]));
    CHECK_EQ(memcmp(output, cases[i], writer.position), 0);
    CHECK_OK(read_number(cases[i], NULL));
  }
  char output[2];
  SioBufferWriter writer;
  sio_buffer_writer_init(&writer, output, sizeof(output));
  CHECK_EQ(read_number("123", &writer.writer), JSON_ERROR_IO);
  CHECK_EQ(writer.position, 2);
  CHECK_EQ(memcmp(output, "12", 2), 0);
  CHECK_EQ(read_number("true", NULL), JSON_ERROR_INVALID);
  CHECK_EQ(read_number("01", NULL), JSON_ERROR_INVALID);
  CHECK_EQ(read_number("1.", NULL), JSON_ERROR_EOF);
  CHECK_EQ(read_number("1e+", NULL), JSON_ERROR_EOF);
  CHECK_EQ(read_number("1e+x", NULL), JSON_ERROR_INVALID);
}

static void
test_conversions(
  void
) {
  const char* exact[] = {
    "0", "-0", "1.0", "1e3", "12300e-2",
    "9223372036854775807", "-9223372036854775808",
  };
  int64_t expected[] = {0, 0, 1, 1000, 123, INT64_MAX, INT64_MIN};
  JsonNumberWriter writer;
  for (size_t i = 0; i < sizeof(exact) / sizeof(*exact); i++) {
    json_number_writer_init(&writer);
    CHECK_OK(read_number(exact[i], &writer.writer));
    int64_t integer;
    double real;
    CHECK_OK(json_number_writer_get_int64(&writer, &integer));
    CHECK_EQ(integer, expected[i]);
    CHECK_OK(json_number_writer_get_double(&writer, &real));
    CHECK_EQ(real, (double) expected[i]);
    CHECK_OK(json_number_writer_get_int64(&writer, &integer));
    CHECK_EQ(integer, expected[i]);
    CHECK_OK(json_number_writer_get_double(&writer, NULL));
  }
  const char* fractions[] = {
    "1.5", "9223372036854775808", "-9223372036854775809",
  };
  for (size_t i = 0; i < sizeof(fractions) / sizeof(*fractions); i++) {
    json_number_writer_init(&writer);
    CHECK_OK(read_number(fractions[i], &writer.writer));
    int64_t integer = 77;
    CHECK_EQ(json_number_writer_get_int64(&writer, &integer),
      JSON_ERROR_OUT_OF_RANGE);
    CHECK_EQ(integer, 77);
  }
  json_number_writer_init(&writer);
  CHECK_OK(read_number("-1e-99999999999999999999999", &writer.writer));
  double real;
  CHECK_OK(json_number_writer_get_double(&writer, &real));
  CHECK_EQ(real, 0.0);
  CHECK(signbit(real));
  json_number_writer_init(&writer);
  CHECK_OK(read_number("1e99999999999999999999999", &writer.writer));
  real = 77;
  CHECK_EQ(
    json_number_writer_get_double(&writer, &real),
    JSON_ERROR_OUT_OF_RANGE
  );
  CHECK_EQ(real, 77);
  const char* boundaries[] = {
    "1.7976931348623157e308", "4.9406564584124654e-324",
    "2.2250738585072014e-308", "-0e99999999999999999999999", "123.45e-3",
  };
  for (size_t i = 0; i < sizeof(boundaries) / sizeof(*boundaries); i++) {
    json_number_writer_init(&writer);
    CHECK_OK(read_number(boundaries[i], &writer.writer));
    CHECK_OK(json_number_writer_get_double(&writer, &real));
    CHECK_EQ(real, strtod(boundaries[i], NULL));
  }

}

static void
test_direct_writer(
  void
) {
  const char* invalid[] = {"", "-", "1.", "1e", "1e+", "01", "1x", "1 2"};
  for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
    JsonNumberWriter writer;
    json_number_writer_init(&writer);
    for (size_t j = 0; invalid[i][j] != '\0'; j++) {
      int result = writer.writer.write(writer.writer.user, invalid[i][j]);
      if (result != JSON_OK) {
        CHECK_EQ(result, JSON_ERROR_INVALID);
        CHECK_EQ(writer.writer.write(writer.writer.user, '0'), result);
        break;
      }
    }
    int64_t integer = 77;
    CHECK_EQ(
      json_number_writer_get_int64(&writer, &integer),
      JSON_ERROR_INVALID
    );
    CHECK_EQ(integer, 77);
    json_number_writer_init(&writer);
    CHECK_OK(writer.writer.write(writer.writer.user, '2'));
    CHECK_OK(json_number_writer_get_int64(&writer, &integer));
    CHECK_EQ(integer, 2);
  }
}

static void
test_long_numbers(
  void
) {
  char text[10016];
  char output[10016];
  text[0] = '1';
  memset(text + 1, '0', 10000);
  memcpy(text + 10001, "e-10000", 8);
  SioBufferWriter buffer;
  sio_buffer_writer_init(&buffer, output, sizeof(output));
  CHECK_OK(read_number(text, &buffer.writer));
  CHECK_EQ(buffer.position, strlen(text));
  CHECK_EQ(memcmp(text, output, buffer.position), 0);
  CHECK_OK(read_number(text, NULL));
  JsonNumberWriter writer;
  json_number_writer_init(&writer);
  CHECK_OK(read_number(text, &writer.writer));
  int64_t integer;
  double real;
  CHECK_OK(json_number_writer_get_int64(&writer, &integer));
  CHECK_EQ(integer, 1);
  CHECK_OK(json_number_writer_get_double(&writer, &real));
  CHECK_EQ(real, 1.0);

  size_t size = strlen(text);
  output[0] = '[';
  memcpy(output + 1, text, size);
  memcpy(output + 1 + size, ",2]", 3);
  SioBufferReader reader;
  JsonParser parser;
  JsonValue root, item;
  sio_buffer_reader_init(&reader, output, size + 4);
  CHECK_OK(json_parser_init((SioReader*) &reader, 1, &parser, &root));
  CHECK_OK(json_value_enter_container(&root, &item));
  CHECK_OK(json_value_advance(&item));
  CHECK_OK(json_value_get_int64(&item, &integer));
  CHECK_EQ(integer, 2);
  CHECK(json_value_at_end(&item));

  // Exact halfway value rounds to even. A nonzero digit beyond the stored
  // prefix changes which side of the binary64 boundary the input occupies.
  const char halfway[] =
    "1.00000000000000011102230246251565404236316680908203125";
  json_number_writer_init(&writer);
  CHECK_OK(read_number(halfway, &writer.writer));
  CHECK_OK(json_number_writer_get_double(&writer, &real));
  CHECK_EQ(real, 1.0);
  memcpy(text, halfway, sizeof(halfway) - 1);
  memset(text + sizeof(halfway) - 1, '0', 2000 - sizeof(halfway));
  text[1999] = '1';
  text[2000] = '\0';
  json_number_writer_init(&writer);
  CHECK_OK(read_number(text, &writer.writer));
  CHECK(writer.sticky);
  CHECK_OK(json_number_writer_get_double(&writer, &real));
  CHECK_EQ(real, 0x1.0000000000001p0);
}

static void
test_advancement(
  void
) {
  const char text[] = "[1.5,1e9999,2]";
  SioBufferReader reader;
  JsonParser parser;
  JsonValue root, item;
  sio_buffer_reader_init(&reader, text, sizeof(text) - 1);
  CHECK_OK(json_parser_init((SioReader*) &reader, 1, &parser, &root));
  CHECK_OK(json_value_enter_container(&root, &item));
  int64_t integer = 77;
  double real = 77;
  CHECK_EQ(json_value_get_int64(&item, &integer), JSON_ERROR_OUT_OF_RANGE);
  CHECK_EQ(integer, 77);
  CHECK_EQ(json_value_get_double(&item, &real), JSON_ERROR_OUT_OF_RANGE);
  CHECK_EQ(real, 77);
  CHECK_OK(json_value_get_int64(&item, &integer));
  CHECK_EQ(integer, 2);
  CHECK(json_value_at_end(&item));
  CHECK_OK(json_value_leave_container(&root, &item));
}

int
main(
  void
) {
  test_exact_text();
  test_conversions();
  test_direct_writer();
  test_long_numbers();
  test_advancement();
  return EXIT_SUCCESS;
}

