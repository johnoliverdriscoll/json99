#include <json99/json.h>
#include <sio99/sio_file_reader.h>
#include "test.h"

static int
validate(
  const char* text
) {
  FILE* file = tmpfile();
  CHECK_NE(file, NULL);
  CHECK(fputs(text, file) >= 0);
  rewind(file);

  SioFileReader reader;
  JsonParser parser;
  JsonValue value;
  sio_file_reader_init(&reader, file);
  int result = json_parser_init((SioReader*) &reader, 8, &parser, &value);
  if (result == JSON_OK) {
    result = json_value_advance(&value);
  }

  CHECK_EQ(fclose(file), 0);
  return result;
}

int
main(
  void
) {
  FILE* file = tmpfile();
  CHECK(file != NULL);
  CHECK(fputs("42", file) >= 0);
  rewind(file);
  SioFileReader reader;
  sio_file_reader_init(&reader, file);
  uint8_t byte;
  bool at_end;
  CHECK(sio_reader_peek((SioReader*) &reader, &byte, &at_end) == JSON_OK);
  CHECK(byte == '4' && !at_end);
  CHECK(sio_reader_peek((SioReader*) &reader, &byte, &at_end) == JSON_OK);
  CHECK(byte == '4' && !at_end);
  JsonParser parser;
  JsonValue value;
  CHECK(json_parser_init((SioReader*) &reader, 8, &parser, &value) == JSON_OK);
  int64_t number;
  CHECK(json_value_get_int64(&value, &number) == JSON_OK && number == 42);
  CHECK(
    sio_reader_peek((SioReader*) &reader, NULL, &at_end) == JSON_OK
    && at_end
  );
  CHECK(fclose(file) == 0);

  file = tmpfile();
  CHECK_NE(file, NULL);
  sio_file_reader_init(&reader, file);
  byte = 0x7f;
  CHECK_EQ(sio_reader_read((SioReader*) &reader, &byte), SIO_ERROR_EOF);
  CHECK_EQ(byte, 0x7f);
  CHECK_EQ(sio_reader_read((SioReader*) &reader, NULL), SIO_ERROR_EOF);
  CHECK_EQ(fclose(file), 0);

  const char* truncated[] = {
    "\"abc",
    "tru",
    "\"\\u12",
    "[1",
  };
  for (size_t i = 0; i < sizeof(truncated) / sizeof(truncated[0]); i++) {
    CHECK_EQ(validate(truncated[i]), JSON_ERROR_EOF);
  }
  CHECK_OK(validate("\"abc\""));
  CHECK_OK(validate("true"));

  return EXIT_SUCCESS;
}


