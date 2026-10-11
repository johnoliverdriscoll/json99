#include <string.h>
#include <json99/json.h>
#include <json99/json_utf8_writer.h>
#include <sio99/sio_buffer_reader.h>
#include "test.h"

typedef struct FailingOutput {
  SioWriter writer;
  int error;
  int last_error;
  unsigned calls;
  unsigned fail_at;
} FailingOutput;

static int
fail_write(
  void* user,
  uint8_t byte
) {
  FailingOutput* output = user;
  (void) byte;
  output->calls++;
  output->last_error = output->calls == output->fail_at ? output->error : SIO_OK;
  return output->last_error;
}

static int
fail_peek(
  void* user,
  uint8_t* byte,
  bool* end
) {
  (void) byte;
  (void) end;
  return *(int*) user;
}

static int
fail_read(
  void* user,
  uint8_t* byte
) {
  (void) byte;
  return *(int*) user;
}

static void
test_output_errors(void) {
  const int errors[] = {
    SIO_ERROR_IO, SIO_ERROR_EOF, SIO_ERROR_INVALID, SIO_ERROR_OVERFLOW,
    SIO_ERROR_NO_MEMORY, 123,
  };
  const char* texts[] = {"\"a\\u0800\"", "\"a\\u0800\"", "12.34"};
  const unsigned sizes[] = {4, 7, 5};
  for (size_t e = 0; e < sizeof(errors) / sizeof(*errors); e++) {
    for (unsigned mode = 0; mode < 3; mode++) {
      for (unsigned fail_at = 1; fail_at <= sizes[mode]; fail_at++) {
        FailingOutput output = {
          .writer = {.user = &output, .write = fail_write},
          .error = errors[e],
          .fail_at = fail_at,
        };
        SioBufferReader reader;
        sio_buffer_reader_init(&reader, texts[mode], strlen(texts[mode]));
        JsonParser parser;
        JsonValue value;
        CHECK_OK(json_parser_init((SioReader*) &reader, 1, &parser, &value));
        int result;
        if (mode == 0) {
          result = json_value_read_string(&value, (SioWriter*) &output);
        } else if (mode == 1) {
          result = json_value_read_raw_string(&value, (SioWriter*) &output);
        } else {
          result = json_value_read_number(&value, (SioWriter*) &output);
        }
        CHECK_EQ(result, JSON_ERROR_IO);
        CHECK_EQ(output.last_error, errors[e]);
        CHECK_EQ(output.calls, fail_at);
        CHECK(!json_value_at_end(&value));
      }
    }
    FailingOutput output = {
      .writer = {.user = &output, .write = fail_write},
      .error = errors[e],
      .fail_at = 2,
    };
    JsonUtf8Writer converter;
    json_utf8_writer_init(&converter, (SioWriter*) &output);
    CHECK_EQ(converter.writer.write_utf16(&converter, 0x800), JSON_ERROR_IO);
    CHECK_EQ(converter.writer.finish(&converter), JSON_ERROR_IO);
    CHECK_EQ(converter.writer.write_utf8(&converter, 'x'), JSON_ERROR_IO);
    CHECK_EQ(output.calls, 2);
    output.calls = 0;
    output.fail_at = 10;
    json_utf8_writer_init(&converter, (SioWriter*) &output);
    CHECK_OK(converter.writer.write_utf8(&converter, 'x'));
    CHECK_OK(converter.writer.finish(&converter));
  }
}

static int
peek_quote(
  void* user,
  uint8_t* byte,
  bool* end
) {
  (void) user;
  if (byte != NULL) {
    *byte = '"';
  }
  if (end != NULL) {
    *end = false;
  }
  return SIO_OK;
}

static void
test_input_errors(void) {
  const int errors[] = {SIO_ERROR_IO, SIO_ERROR_EOF, 123};
  for (size_t i = 0; i < sizeof(errors) / sizeof(*errors); i++) {
    int error = errors[i];
    SioReader reader = {.user = &error, .read = fail_read, .peek = fail_peek};
    JsonParser parser;
    JsonValue value;
    CHECK_EQ(json_parser_init(&reader, 1, &parser, &value), JSON_ERROR_IO);
    reader.peek = peek_quote;
    CHECK_OK(json_parser_init(&reader, 1, &parser, &value));
    CHECK_EQ(
      json_value_read_string(&value, NULL),
      error == SIO_ERROR_EOF ? JSON_ERROR_EOF : JSON_ERROR_IO
    );
  }
}

int
main(void) {
  test_output_errors();
  test_input_errors();
  return EXIT_SUCCESS;
}
