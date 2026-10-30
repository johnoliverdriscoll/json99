#include <json99/json.h>
#include <string.h>
#include <sio99/sio_buffer_reader.h>
#include <sio99/sio_buffer_writer.h>
#include "test.h"

static void
test_output(
  void
) {
  const char* strings[] = {
    "\"\"",
    "\"plain text\"",
    "\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"",
    "\"\\u0000\\u0041\\u00e9\\u00E9\"",
    "\"\\uD83d\\ude00\"",
    "\"\xc2\x80\xe0\xa0\x80\xef\xbf\xbf\xf4\x8f\xbf\xbf\"",
    "\"caf\xc3\xa9 \\u20AC\\nend\"",
  };
  for (size_t i = 0; i < sizeof(strings) / sizeof(*strings); i++) {
    size_t size = strlen(strings[i]);
    for (unsigned discard = 0; discard < 2; discard++) {
      uint8_t output[128];
      memset(output, 0xa5, sizeof(output));
      SioBufferWriter writer;
      sio_buffer_writer_init(&writer, output, size - 2);
      SioBufferReader reader;
      JsonParser parser;
      JsonValue value;
      sio_buffer_reader_init(&reader, strings[i], size);
      CHECK_OK(json_parser_init((SioReader*) &reader, 0, &parser, &value));
      CHECK_OK(json_value_read_raw_string(
        &value,
        discard ? NULL : &writer.writer
      ));
      CHECK(json_value_at_end(&value));
      CHECK_EQ(reader.position, size);
      CHECK_EQ(writer.position, discard ? 0 : size - 2);
      if (!discard) {
        CHECK_EQ(memcmp(output, strings[i] + 1, size - 2), 0);
      }
      CHECK_EQ(output[size - 2], 0xa5);
    }
  }
}

static void
test_invalid(
  void
) {
  const char* strings[] = {
    "\"\\x\"", "\"\\u12xz\"", "\"\n\"", "\"\x1f\"",
    "\"\\uD800\"", "\"\\uDC00\"", "\"\\uD800\\u0041\"",
    "\"\xc2\"", "\"\xe0\xa0\"", "\"\xf0\x90\x80\"",
    "\"\x80\"", "\"\xc0\x80\"", "\"\xe0\x9f\xbf\"",
    "\"\xed\xa0\x80\"", "\"\xf0\x8f\xbf\xbf\"",
    "\"\xf4\x90\x80\x80\"", "\"\xf5\x80\x80\x80\"",
    "\"\xc2\\u0080\"", "\"\\uD800a\\uDC00\"",
  };
  for (size_t i = 0; i < sizeof(strings) / sizeof(*strings); i++) {
    for (unsigned discard = 0; discard < 2; discard++) {
      uint8_t output[128];
      SioBufferWriter writer;
      sio_buffer_writer_init(&writer, output, sizeof(output));
      SioBufferReader reader;
      JsonParser parser;
      JsonValue value;
      sio_buffer_reader_init(&reader, strings[i], strlen(strings[i]));
      CHECK_OK(json_parser_init((SioReader*) &reader, 0, &parser, &value));
      CHECK_EQ(json_value_read_raw_string(
        &value,
        discard ? NULL : &writer.writer
      ), JSON_ERROR_INVALID);
      CHECK(!json_value_at_end(&value));
    }
  }

  const char escapes[] = {'"', '\\', '/', 'b', 'f', 'n', 'r', 't'};
  for (size_t i = 0; i < sizeof(escapes); i++) {
    char surrogate[] = "\"\\uD800\\n\\uDC00\"";
    char utf8[] = "\"\xc2\\n\x80\"";
    surrogate[8] = escapes[i];
    utf8[3] = escapes[i];
    const char* interrupted[] = {surrogate, utf8};
    for (size_t j = 0; j < 2; j++) {
      SioBufferReader reader;
      JsonParser parser;
      JsonValue value;
      sio_buffer_reader_init(&reader, interrupted[j], strlen(interrupted[j]));
      CHECK_OK(json_parser_init((SioReader*) &reader, 0, &parser, &value));
      CHECK_EQ(json_value_read_raw_string(&value, NULL), JSON_ERROR_INVALID);
      CHECK(!json_value_at_end(&value));
    }
  }
}

static void
test_truncation(
  void
) {
  const char text[] = "\"a\\n\\uD83D\\uDE00\"";
  for (size_t size = 1; size < sizeof(text) - 1; size++) {
    SioBufferReader reader;
    JsonParser parser;
    JsonValue value;
    sio_buffer_reader_init(&reader, text, size);
    CHECK_OK(json_parser_init((SioReader*) &reader, 0, &parser, &value));
    CHECK_EQ(json_value_read_raw_string(&value, NULL), JSON_ERROR_EOF);
    CHECK(!json_value_at_end(&value));
  }
}

static void
test_traversal(
  void
) {
  const char text[] = "[\"a\\n\",{\"\\u006B\":\"\"},42]";
  SioBufferReader reader;
  JsonParser parser;
  JsonValue root, array, object;
  sio_buffer_reader_init(&reader, text, sizeof(text) - 1);
  CHECK_OK(json_parser_init((SioReader*) &reader, 2, &parser, &root));
  CHECK_OK(json_value_enter_container(&root, &array));
  CHECK_OK(json_value_read_raw_string(&array, NULL));
  CHECK_EQ(json_value_get_type(&array), JSON_TYPE_OBJECT);
  CHECK_OK(json_value_enter_container(&array, &object));
  uint8_t output[6];
  SioBufferWriter writer;
  sio_buffer_writer_init(&writer, output, sizeof(output));
  CHECK_OK(json_value_read_raw_string(&object, &writer.writer));
  CHECK_EQ(writer.position, sizeof(output));
  CHECK_EQ(memcmp(output, "\\u006B", sizeof(output)), 0);
  CHECK(!object.is_key);
  CHECK_OK(json_value_read_raw_string(&object, NULL));
  CHECK(json_value_at_end(&object));
  CHECK_OK(json_value_leave_container(&array, &object));
  CHECK_EQ(json_value_get_type(&array), JSON_TYPE_NUMBER);
  size_t position = reader.position;
  CHECK_EQ(json_value_read_raw_string(&array, NULL), JSON_ERROR_INVALID);
  CHECK_EQ(reader.position, position);
  int64_t number;
  CHECK_OK(json_value_get_int64(&array, &number));
  CHECK_EQ(number, 42);
  CHECK_OK(json_value_leave_container(&root, &array));
  CHECK(json_value_at_end(&root));

  const char trailing[] = "\"x\" trailing";
  sio_buffer_reader_init(&reader, trailing, sizeof(trailing) - 1);
  CHECK_OK(json_parser_init((SioReader*) &reader, 0, &parser, &root));
  CHECK_OK(json_value_read_raw_string(&root, NULL));
  CHECK_EQ(reader.position, 3);
}

typedef struct FailingWriter {
  SioBufferWriter buffer;
  size_t limit;
} FailingWriter;

static int
fail_write(
  void* user,
  uint8_t byte
) {
  FailingWriter* writer = user;
  if (writer->buffer.position == writer->limit) {
    return SIO_ERROR_IO;
  }
  return writer->buffer.writer.write(&writer->buffer, byte);
}

static void
test_writer_failures(
  void
) {
  const char text[] = "\"a\\\"\\\\\\/\\b\\f\\n\\r\\t\\uD83D\\uDE00\"";
  size_t length = sizeof(text) - 3;
  for (size_t limit = 0; limit < length; limit++) {
    for (unsigned io_error = 0; io_error < 2; io_error++) {
      uint8_t output[128];
      FailingWriter sink = {.limit = limit};
      sio_buffer_writer_init(
        &sink.buffer,
        output,
        io_error ? sizeof(output) : limit
      );
      SioWriter writer = {.user = &sink, .write = fail_write};
      SioBufferReader reader;
      JsonParser parser;
      JsonValue value;
      sio_buffer_reader_init(&reader, text, sizeof(text) - 1);
      CHECK_OK(json_parser_init((SioReader*) &reader, 0, &parser, &value));
      CHECK_EQ(json_value_read_raw_string(
        &value,
        io_error ? &writer : &sink.buffer.writer
      ), JSON_ERROR_IO);
      CHECK_EQ(sink.buffer.position, limit);
      CHECK_EQ(memcmp(output, text + 1, limit), 0);
      CHECK(!json_value_at_end(&value));
    }
  }
}

typedef struct FailingReader {
  SioBufferReader buffer;
  size_t limit;
} FailingReader;

static int
fail_read(
  void* user,
  uint8_t* out
) {
  FailingReader* reader = user;
  if (reader->buffer.position == reader->limit) {
    return SIO_ERROR_IO;
  }
  return sio_reader_read((SioReader*) &reader->buffer, out);
}

static int
forward_peek(
  void* user,
  uint8_t* out,
  bool* at_end
) {
  FailingReader* reader = user;
  return sio_reader_peek((SioReader*) &reader->buffer, out, at_end);
}

static void
test_reader_failures(
  void
) {
  const char text[] = "\"a\\n\\uD83D\\uDE00\"";
  for (size_t limit = 0; limit < sizeof(text) - 1; limit++) {
    FailingReader source = {.limit = limit};
    sio_buffer_reader_init(&source.buffer, text, sizeof(text) - 1);
    SioReader reader = {
      .user = &source,
      .read = fail_read,
      .peek = forward_peek,
    };
    JsonParser parser;
    JsonValue value;
    CHECK_OK(json_parser_init(&reader, 0, &parser, &value));
    CHECK_EQ(json_value_read_raw_string(&value, NULL), JSON_ERROR_IO);
    CHECK_EQ(source.buffer.position, limit);
    CHECK(!json_value_at_end(&value));
  }
}

int
main(
  void
) {
  test_output();
  test_invalid();
  test_truncation();
  test_traversal();
  test_writer_failures();
  test_reader_failures();
  return EXIT_SUCCESS;
}

