#include <string.h>
#include <json99/json_buffer_reader.h>
#include <json99/json_buffer_writer.h>
#include <json99/json_utf8_writer.h>
#include "test.h"

static int
write_utf8s(
  JsonUtf8Writer* writer,
  const uint8_t* bytes,
  size_t size
) {
  for (size_t i = 0; i < size; i++) {
    int result = writer->writer.write_utf8(writer, bytes[i]);
    if (result != JSON_OK) {
      return result;
    }
  }
  return writer->writer.finish(writer);
}

static void
test_conversion(
  void
) {
  uint8_t output[32];
  JsonBufferWriter destination;
  JsonUtf8Writer writer;
  json_buffer_writer_init(&destination, output, sizeof(output));
  json_utf8_writer_init(&writer, &destination.writer);
  const uint16_t units[] = {0, 0x7f, 0x80, 0x7ff, 0x800, 0xffff,
    0xd800, 0xdc00, 0xdbff, 0xdfff};
  const uint8_t expected[] = {
    0x00, 0x7f, 0xc2, 0x80, 0xdf, 0xbf, 0xe0, 0xa0, 0x80, 0xef,
    0xbf, 0xbf, 0xf0, 0x90, 0x80, 0x80, 0xf4, 0x8f, 0xbf, 0xbf,
  };
  for (size_t i = 0; i < sizeof(units) / sizeof(*units); i++) {
    CHECK_OK(writer.writer.write_utf16(&writer, units[i]));
  }
  CHECK_OK(writer.writer.finish(&writer));
  CHECK_OK(writer.writer.finish(&writer));
  CHECK_EQ(destination.position, sizeof(expected));
  CHECK_EQ(memcmp(output, expected, sizeof(expected)), 0);

  json_buffer_writer_reset(&destination);
  json_utf8_writer_init(&writer, &destination.writer);
  CHECK_OK(write_utf8s(&writer, expected, sizeof(expected)));
  CHECK_EQ(destination.position, sizeof(expected));
  CHECK_EQ(memcmp(output, expected, sizeof(expected)), 0);

  json_utf8_writer_init(&writer, NULL);
  CHECK_OK(write_utf8s(&writer, expected, sizeof(expected)));
}

static void
test_invalid(
  void
) {
  const uint8_t invalid[][4] = {
    {0x80}, {0xc0, 0x80}, {0xc1, 0xbf}, {0xe0, 0x9f, 0xbf},
    {0xed, 0xa0, 0x80}, {0xf0, 0x8f, 0xbf, 0xbf},
    {0xf4, 0x90, 0x80, 0x80}, {0xf5}, {0xff}, {0xc2, 'x'},
  };
  JsonUtf8Writer writer;
  for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
    json_utf8_writer_init(&writer, NULL);
    CHECK_EQ(write_utf8s(&writer, invalid[i], 4), JSON_ERROR_INVALID);
    CHECK_EQ(writer.writer.write_utf8(&writer, 'a'), JSON_ERROR_INVALID);
    CHECK_EQ(writer.writer.finish(&writer), JSON_ERROR_INVALID);
  }
  const uint8_t incomplete[] = {0xf0, 0x90, 0x80};
  for (size_t size = 1; size <= sizeof(incomplete); size++) {
    json_utf8_writer_init(&writer, NULL);
    CHECK_EQ(write_utf8s(&writer, incomplete, size), JSON_ERROR_INVALID);
  }
  json_utf8_writer_init(&writer, NULL);
  CHECK_EQ(writer.writer.write_utf16(&writer, 0xdc00), JSON_ERROR_INVALID);
  json_utf8_writer_init(&writer, NULL);
  CHECK_OK(writer.writer.write_utf16(&writer, 0xd800));
  CHECK_EQ(writer.writer.finish(&writer), JSON_ERROR_INVALID);
  json_utf8_writer_init(&writer, NULL);
  CHECK_OK(writer.writer.write_utf16(&writer, 0xd800));
  CHECK_EQ(writer.writer.write_utf8(&writer, 'a'), JSON_ERROR_INVALID);
  json_utf8_writer_init(&writer, NULL);
  CHECK_OK(writer.writer.write_utf8(&writer, 0xc2));
  CHECK_EQ(writer.writer.write_utf16(&writer, 0x80), JSON_ERROR_INVALID);
  json_utf8_writer_init(&writer, NULL);
  CHECK_OK(writer.writer.write_utf16(&writer, 0xd800));
  CHECK_EQ(writer.writer.write_utf16(&writer, 'a'), JSON_ERROR_INVALID);
}

static void
test_destination_error(
  void
) {
  uint8_t output[1];
  JsonBufferWriter destination;
  JsonUtf8Writer writer;
  json_buffer_writer_init(&destination, output, sizeof(output));
  json_utf8_writer_init(&writer, &destination.writer);
  CHECK_EQ(writer.writer.write_utf16(&writer, 0x800), JSON_ERROR_EOF);
  CHECK_EQ(destination.position, 1);
  CHECK_EQ(writer.writer.write_utf8(&writer, 'a'), JSON_ERROR_EOF);
  CHECK_EQ(writer.writer.finish(&writer), JSON_ERROR_EOF);
}

typedef struct RecordingWriter {
  JsonUnicodeWriter writer;
  unsigned bytes;
  unsigned units;
  unsigned finishes;
  int error;
} RecordingWriter;

static int
record_byte(
  void* user,
  uint8_t byte
) {
  RecordingWriter* writer = user;
  CHECK_EQ(byte, writer->bytes == 0 ? 'a' : '\n');
  writer->bytes++;
  return JSON_OK;
}

static int
record_unit(
  void* user,
  uint16_t unit
) {
  RecordingWriter* writer = user;
  CHECK_EQ(unit, writer->units == 0 ? 0xd83d : 0xde00);
  writer->units++;
  return JSON_OK;
}

static int
record_finish(
  void* user
) {
  RecordingWriter* writer = user;
  writer->finishes++;
  return writer->error;
}

static void
test_parser(
  void
) {
  const char* text = "\"a\\n\\ud83d\\ude00\"";
  for (unsigned fail = 0; fail < 2; fail++) {
    RecordingWriter sink = {
      .writer = {
        .user = &sink,
        .write_utf8 = record_byte,
        .write_utf16 = record_unit,
        .finish = record_finish,
      },
      .error = fail ? JSON_ERROR_IO : JSON_OK,
    };
    JsonBufferReader reader;
    JsonParser parser;
    JsonValue value;
    json_buffer_reader_init(&reader, text, strlen(text));
    CHECK_OK(json_parser_init(&reader.reader, 1, &parser, &value));
    CHECK_EQ(json_value_read_unicode(&value, &sink.writer), sink.error);
    CHECK_EQ(sink.bytes, 2);
    CHECK_EQ(sink.units, 2);
    CHECK_EQ(sink.finishes, 1);
    CHECK_EQ(json_value_at_end(&value), !fail);
  }

  const char* invalid[] = {
    "\"\\ud800\"", "\"\\udc00\"", "\"\\ud800a\"",
    "\"\\ud800\\u0041\"", "\"\xc2\\u0080\"", "\"\xed\xa0\x80\"",
    "\"\\u00xz\"", "\"\n\"", "\"\\x\"", "\"\xf0\x90\"",
  };
  for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
    JsonBufferReader reader;
    JsonParser parser;
    JsonValue value;
    json_buffer_reader_init(&reader, invalid[i], strlen(invalid[i]));
    CHECK_OK(json_parser_init(&reader.reader, 1, &parser, &value));
    CHECK_EQ(json_value_read_string(&value, NULL), JSON_ERROR_INVALID);
  }
}

int
main(
  void
) {
  test_conversion();
  test_invalid();
  test_destination_error();
  test_parser();
  return EXIT_SUCCESS;
}
