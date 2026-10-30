#include <string.h>
#include <stdlib.h>
#include <json99/json.h>
#include <sio99/sio_buffer_writer.h>
#include <json99/json_number_writer.h>
#include <sio99/sio.h>
#include <json99/json_utf8_writer.h>
#include "json_assert.h"
#include "json_require.h"
#include "json_try.h"

static int
json_reader_peek(
  SioReader* reader,
  uint8_t* out_peeked,
  bool* out_at_end
) {
  int result = sio_reader_peek(reader, out_peeked, out_at_end);
  return result == SIO_OK ? JSON_OK : JSON_ERROR_IO;
}

static int
json_reader_read(
  SioReader* reader,
  uint8_t* out_read
) {
  int result = sio_reader_read(reader, out_read);
  if (result == SIO_ERROR_EOF) {
    return JSON_ERROR_EOF;
  }
  return result == SIO_OK ? JSON_OK : JSON_ERROR_IO;
}

static int
json_writer_write(
  SioWriter* writer,
  uint8_t val
) {
  if (writer == NULL) {
    return JSON_OK;
  }

  int result = sio_writer_write(writer, val);
  return result == SIO_OK ? JSON_OK : JSON_ERROR_IO;
}

static int
json_unicode_writer_write_utf8(
  JsonUnicodeWriter* writer,
  uint8_t val
) {
  if (writer == NULL) {
    return JSON_OK;
  }

  JSON_ASSERT_NE(writer->write_utf8, NULL);

  return writer->write_utf8(writer->user, val);
}

static int
json_unicode_writer_write_utf16(
  JsonUnicodeWriter* writer,
  uint16_t val
) {
  if (writer == NULL) {
    return JSON_OK;
  }

  JSON_ASSERT_NE(writer->write_utf16, NULL);

  return writer->write_utf16(writer->user, val);
}

static int
json_unicode_writer_finish(
  JsonUnicodeWriter* writer
) {
  if (writer == NULL) {
    return JSON_OK;
  }

  JSON_ASSERT_NE(writer->finish, NULL);

  return writer->finish(writer->user);
}

static int
json_peek_required(
  SioReader* reader,
  uint8_t* out
) {
  JSON_ASSERT_NE(out, NULL);

  uint8_t c;
  bool at_end;
  JSON_TRY(json_reader_peek(reader, &c, &at_end));
  JSON_REQUIRE(!at_end, JSON_ERROR_EOF);

  *out = c;
  return JSON_OK;
}

static int
json_skip_whitespace(
  SioReader* reader
) {
  for (;;) {
    char c;
    JSON_TRY(json_peek_required(reader, (uint8_t*) &c));
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
      break;
    }
    JSON_TRY(json_reader_read(reader, (uint8_t*) &c));
  }

  return JSON_OK;
}

static int
json_get_next_type(
  SioReader* reader,
  JsonType* out_type
) {
  JSON_ASSERT_NE(out_type, NULL);

  JSON_TRY(json_skip_whitespace(reader));

  char c;
  JSON_TRY(json_peek_required(reader, (uint8_t*) &c));

  switch (c) {
  case 'n':
    *out_type = JSON_TYPE_NULL;
    break;
  case 't':
  case 'f':
    *out_type = JSON_TYPE_BOOL;
    break;
  case '"':
    *out_type = JSON_TYPE_STRING;
    break;
  case '[':
    *out_type = JSON_TYPE_ARRAY;
    break;
  case '{':
    *out_type = JSON_TYPE_OBJECT;
    break;
  default:
    JSON_REQUIRE((c >= '0' && c <= '9') || c == '-', JSON_ERROR_INVALID);
    *out_type = JSON_TYPE_NUMBER;
    break;
  }

  return JSON_OK;
}

static int
json_read_expected_chars(
  SioReader* reader,
  const char* expected,
  size_t len
) {
  for (size_t i = 0; i < len; i++) {
    char c;
    JSON_TRY(json_reader_read(reader, (uint8_t*) &c));
    JSON_REQUIRE_EQ(expected[i], c, JSON_ERROR_INVALID);
  }
  return JSON_OK;
}

#define json_read_expected(reader, expected) \
  json_read_expected_chars(                  \
    (reader),                                \
    "" expected,                             \
    sizeof("" expected) - 1                  \
  )

static int
json_read_unicode_unit(
  SioReader* reader,
  SioWriter* writer,
  uint16_t *out
) {
  char value[5] = {0};
  for (size_t i = 0; i < sizeof(value) - 1; i++) {
    JSON_TRY(json_reader_read(reader, (uint8_t*) &value[i]));
    JSON_REQUIRE(
      (value[i] >= '0' && value[i] <= '9')
      || (value[i] >= 'a' && value[i] <= 'f')
      || (value[i] >= 'A' && value[i] <= 'F'),
      JSON_ERROR_INVALID
    );
    if (writer != NULL) {
      JSON_TRY(json_writer_write(writer, value[i]));
    }
  }

  if (out != NULL) {
    *out = strtol(value, NULL, 16);
  }
  return JSON_OK;
}

int
json_parser_init(
  SioReader* reader,
  unsigned max_depth,
 JsonParser* out_parser,
  JsonValue* out_it
 ) {
  JSON_ASSERT_NE(out_parser, NULL);
  JSON_ASSERT_NE(out_it, NULL);

  JsonParser parser = {0};
  JsonValue it = {0};

  *out_parser = parser;
  *out_it = it;

  parser.reader = reader;
  parser.max_depth = max_depth;

  it.parser = out_parser;
  JSON_TRY(json_get_next_type(reader, &it.type));

  *out_parser = parser;
  *out_it = it;

  return JSON_OK;
}

static int
json_value_next(
  JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);

  if (it->container_type == JSON_TYPE_NULL) {
    it->at_end = true;
    return JSON_OK;
  }

  JSON_ASSERT(
    it->container_type == JSON_TYPE_ARRAY
    || it->container_type == JSON_TYPE_OBJECT
  );

  SioReader* reader = it->parser->reader;

  JSON_TRY(json_skip_whitespace(reader));

  char c;
  JSON_TRY(json_reader_read(reader, (uint8_t*) &c));

  if (c == ':') {
    JSON_REQUIRE_EQ(
      it->container_type,
      JSON_TYPE_OBJECT,
      JSON_ERROR_INVALID
    );
    JSON_REQUIRE(it->is_key, JSON_ERROR_INVALID);
    JSON_TRY(json_get_next_type(reader, &it->type));
    it->is_key = false;
    return JSON_OK;
  }

  if (it->container_type == JSON_TYPE_OBJECT) {
    JSON_REQUIRE(!it->is_key, JSON_ERROR_INVALID);
  }

  if (c == ',') {
    JSON_TRY(json_get_next_type(reader, &it->type));
    if (it->container_type == JSON_TYPE_OBJECT) {
      JSON_REQUIRE(it->type == JSON_TYPE_STRING, JSON_ERROR_INVALID);
      it->is_key = true;
    }
    return JSON_OK;
  }

  JSON_REQUIRE(
    (it->container_type == JSON_TYPE_ARRAY && c == ']')
    || (it->container_type == JSON_TYPE_OBJECT && c == '}'),
    JSON_ERROR_INVALID
  );

  it->at_end = true;

  return JSON_OK;
}

JsonType
json_value_get_type(
  const JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);

  return it->type;
}

static int
json_value_validate_null(
  JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);

  JSON_REQUIRE(it->type == JSON_TYPE_NULL, JSON_ERROR_INVALID);

  SioReader* reader = it->parser->reader;

  JSON_TRY(json_read_expected(reader, "null"));

  JSON_TRY(json_value_next(it));

  return JSON_OK;
}

int
json_value_get_bool(
  JsonValue* it,
  bool* out_value
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);

  JSON_REQUIRE(it->type == JSON_TYPE_BOOL, JSON_ERROR_INVALID);

  SioReader* reader = it->parser->reader;

  char c;
  JSON_TRY(json_peek_required(reader, (uint8_t*) &c));

  JSON_ASSERT(c == 't' || c == 'f');

  switch (c) {
  case 't':
    JSON_TRY(json_read_expected(reader, "true"));
    if (out_value != NULL) {
      *out_value = true;
    }
    break;
  case 'f':
    JSON_TRY(json_read_expected(reader, "false"));
    if (out_value != NULL) {
      *out_value = false;
    }
    break;
  }

  JSON_TRY(json_value_next(it));

  return JSON_OK;
}

int
json_value_read_number(
  JsonValue* it,
  SioWriter* writer
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);
  JSON_REQUIRE(it->type == JSON_TYPE_NUMBER, JSON_ERROR_INVALID);

  SioReader* reader = it->parser->reader;

  uint8_t c;
  JSON_TRY(json_peek_required(reader, &c));

  if (c == '-') {
    JSON_TRY(json_reader_read(reader, &c));
    JSON_TRY(json_writer_write(writer, c));
    JSON_TRY(json_peek_required(reader, &c));
  }

  bool at_end;

  JSON_REQUIRE(c >= '0' && c <= '9', JSON_ERROR_INVALID);
  bool leading_zero = c == '0';
  do {
    JSON_TRY(json_reader_read(reader, &c));
    JSON_TRY(json_writer_write(writer, c));
    JSON_TRY(json_reader_peek(reader, &c, &at_end));
    if (leading_zero) {
      JSON_REQUIRE(at_end || c < '0' || c > '9', JSON_ERROR_INVALID);
      break;
    }
  } while (!at_end && c >= '0' && c <= '9');

  if (!at_end && c == '.') {
    JSON_TRY(json_reader_read(reader, &c));
    JSON_TRY(json_writer_write(writer, c));
    JSON_TRY(json_peek_required(reader, &c));
    JSON_REQUIRE(c >= '0' && c <= '9', JSON_ERROR_INVALID);
    do {
      JSON_TRY(json_reader_read(reader, &c));
      JSON_TRY(json_writer_write(writer, c));
      JSON_TRY(json_reader_peek(reader, &c, &at_end));
    } while (!at_end && c >= '0' && c <= '9');
  }

  if (!at_end && (c == 'e' || c == 'E')) {
    JSON_TRY(json_reader_read(reader, &c));
    JSON_TRY(json_writer_write(writer, c));
    JSON_TRY(json_peek_required(reader, &c));
    if (c == '+' || c == '-') {
      JSON_TRY(json_reader_read(reader, &c));
      JSON_TRY(json_writer_write(writer, c));
      JSON_TRY(json_peek_required(reader, &c));
    }
    JSON_REQUIRE(c >= '0' && c <= '9', JSON_ERROR_INVALID);
    do {
      JSON_TRY(json_reader_read(reader, &c));
      JSON_TRY(json_writer_write(writer, c));
      JSON_TRY(json_reader_peek(reader, &c, &at_end));
    } while (!at_end && c >= '0' && c <= '9');
  }

  return json_value_next(it);
}

int
json_value_get_double(
  JsonValue* it,
  double* out_value
) {
  JsonNumberWriter writer;
  json_number_writer_init(&writer);
  JSON_TRY(json_value_read_number(it, (SioWriter*) &writer));
  return json_number_writer_get_double(&writer, out_value);
}

int
json_value_get_int64(
  JsonValue* it,
  int64_t* out_value
) {
  JsonNumberWriter writer;
  json_number_writer_init(&writer);
  JSON_TRY(json_value_read_number(it, (SioWriter*) &writer));
  return json_number_writer_get_int64(&writer, out_value);
}

int
json_value_read_string(
  JsonValue* it,
  SioWriter* writer
) {
  JsonUtf8Writer utf8;
  json_utf8_writer_init(&utf8, writer);
  return json_value_read_unicode(it, &utf8.writer);
}

int
json_value_read_unicode(
  JsonValue* it,
  JsonUnicodeWriter* writer
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);

  JSON_REQUIRE(it->type == JSON_TYPE_STRING, JSON_ERROR_INVALID);

  SioReader* reader = it->parser->reader;

  uint8_t byte;
  JSON_TRY(json_reader_read(reader, &byte));

  JSON_ASSERT_EQ(byte, '"');

  for (;;) {
    JSON_TRY(json_reader_read(reader, &byte));
    if (byte == '"') {
      break;
    }
    if (byte == '\\') {
      JSON_TRY(json_reader_read(reader, &byte));
      uint16_t unit;
      switch (byte) {
      case '"':
        JSON_TRY(json_unicode_writer_write_utf8(writer, '"'));
        break;
      case '\\':
        JSON_TRY(json_unicode_writer_write_utf8(writer, '\\'));
        break;
      case '/':
        JSON_TRY(json_unicode_writer_write_utf8(writer, '/'));
        break;
      case 'b':
        JSON_TRY(json_unicode_writer_write_utf8(writer, '\b'));
        break;
      case 'f':
        JSON_TRY(json_unicode_writer_write_utf8(writer, '\f'));
        break;
      case 'n':
        JSON_TRY(json_unicode_writer_write_utf8(writer, '\n'));
        break;
      case 'r':
        JSON_TRY(json_unicode_writer_write_utf8(writer, '\r'));
        break;
      case 't':
        JSON_TRY(json_unicode_writer_write_utf8(writer, '\t'));
        break;
      case 'u':
        JSON_TRY(json_read_unicode_unit(reader, NULL, &unit));
        JSON_TRY(json_unicode_writer_write_utf16(writer, unit));
        break;
      default:
        JSON_TRY(JSON_ERROR_INVALID);
      }
      continue;
    }
    JSON_REQUIRE_GTE(byte, 0x20, JSON_ERROR_INVALID);
    JSON_TRY(json_unicode_writer_write_utf8(writer, byte));
  }

  JSON_TRY(json_unicode_writer_finish(writer));

  JSON_TRY(json_value_next(it));

  return JSON_OK;
}

int
json_value_read_raw_string(
  JsonValue* it,
  SioWriter* writer
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);

  JSON_REQUIRE(it->type == JSON_TYPE_STRING, JSON_ERROR_INVALID);

  SioReader* reader = it->parser->reader;

  JsonUtf8Writer utf8;
  json_utf8_writer_init(&utf8, NULL);

  SioBufferWriter utf16;
  uint8_t utf16_bytes[10];
  sio_buffer_writer_init(&utf16, utf16_bytes, sizeof(utf16_bytes));

  uint8_t byte;
  JSON_TRY(json_reader_read(reader, &byte));

  JSON_ASSERT_EQ(byte, '"');

  for (;;) {
    JSON_TRY(json_reader_read(reader, &byte));
    if (byte == '"') {
      break;
    }
    if (byte == '\\') {
      JSON_TRY(json_reader_read(reader, &byte));
      uint16_t unit;
      size_t unit_size;
      switch (byte) {
      case '"':
        JSON_TRY(json_unicode_writer_write_utf8(
          (JsonUnicodeWriter*) &utf8,
          '"'
        ));
        JSON_TRY(json_writer_write(writer, '\\'));
        JSON_TRY(json_writer_write(writer, '"'));
        break;
      case '\\':
        JSON_TRY(json_unicode_writer_write_utf8(
          (JsonUnicodeWriter*) &utf8,
          '\\'
        ));
        JSON_TRY(json_writer_write(writer, '\\'));
        JSON_TRY(json_writer_write(writer, '\\'));
        break;
      case '/':
        JSON_TRY(json_unicode_writer_write_utf8(
          (JsonUnicodeWriter*) &utf8,
          '/'
        ));
        JSON_TRY(json_writer_write(writer, '\\'));
        JSON_TRY(json_writer_write(writer, '/'));
        break;
      case 'b':
        JSON_TRY(json_unicode_writer_write_utf8(
          (JsonUnicodeWriter*) &utf8,
          '\b'
        ));
        JSON_TRY(json_writer_write(writer, '\\'));
        JSON_TRY(json_writer_write(writer, 'b'));
        break;
      case 'f':
        JSON_TRY(json_unicode_writer_write_utf8(
          (JsonUnicodeWriter*) &utf8,
          '\f'
        ));
        JSON_TRY(json_writer_write(writer, '\\'));
        JSON_TRY(json_writer_write(writer, 'f'));
        break;
      case 'n':
        JSON_TRY(json_unicode_writer_write_utf8(
          (JsonUnicodeWriter*) &utf8,
          '\n'
        ));
        JSON_TRY(json_writer_write(writer, '\\'));
        JSON_TRY(json_writer_write(writer, 'n'));
        break;
      case 'r':
        JSON_TRY(json_unicode_writer_write_utf8(
          (JsonUnicodeWriter*) &utf8,
          '\r'
        ));
        JSON_TRY(json_writer_write(writer, '\\'));
        JSON_TRY(json_writer_write(writer, 'r'));
        break;
      case 't':
        JSON_TRY(json_unicode_writer_write_utf8(
          (JsonUnicodeWriter*) &utf8,
          '\t'
        ));
        JSON_TRY(json_writer_write(writer, '\\'));
        JSON_TRY(json_writer_write(writer, 't'));
        break;
      case 'u':
        JSON_TRY(json_writer_write(writer, '\\'));
        JSON_TRY(json_writer_write(writer, 'u'));
        sio_buffer_writer_reset(&utf16);
        JSON_TRY(json_read_unicode_unit(reader, (SioWriter*) &utf16, &unit));
        JSON_TRY(json_unicode_writer_write_utf16(
          (JsonUnicodeWriter*) &utf8,
          unit
        ));
        unit_size = sio_buffer_writer_get_size(&utf16);
        for (size_t i = 0; i < unit_size; i++) {
          JSON_TRY(json_writer_write(writer, utf16_bytes[i]));
        }
        break;
      default:
        JSON_TRY(JSON_ERROR_INVALID);
      }
      continue;
    }
    JSON_REQUIRE_GTE(byte, 0x20, JSON_ERROR_INVALID);
    JSON_TRY(json_unicode_writer_write_utf8((JsonUnicodeWriter*) &utf8, byte));
    JSON_TRY(json_writer_write(writer, byte));
  }

  JSON_TRY(json_unicode_writer_finish((JsonUnicodeWriter*) &utf8));

  JSON_TRY(json_value_next(it));

  return JSON_OK;
}

static int
json_value_validate_container(
  JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);

  JSON_ASSERT(it->type == JSON_TYPE_ARRAY || it->type == JSON_TYPE_OBJECT);

  JsonValue recursed;

  JSON_TRY(json_value_enter_container(it, &recursed));
  JSON_TRY(json_value_leave_container(it, &recursed));

  return JSON_OK;
}

int
json_value_advance(
  JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);

  switch (it->type) {
  case JSON_TYPE_NULL:
    JSON_TRY(json_value_validate_null(it));
    break;
  case JSON_TYPE_BOOL:
    JSON_TRY(json_value_get_bool(it, NULL));
    break;
  case JSON_TYPE_NUMBER:
    JSON_TRY(json_value_read_number(it, NULL));
    break;
  case JSON_TYPE_STRING:
    JSON_TRY(json_value_read_string(it, NULL));
    break;
  case JSON_TYPE_ARRAY:
  case JSON_TYPE_OBJECT:
    JSON_TRY(json_value_validate_container(it));
    break;
  }

  return JSON_OK;
}

int
json_value_enter_container(
  JsonValue* it,
  JsonValue* out_recursed
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);

  JSON_ASSERT(it->type == JSON_TYPE_ARRAY || it->type == JSON_TYPE_OBJECT);

  JSON_REQUIRE_LT(it->depth, it->parser->max_depth, JSON_ERROR_DEPTH_LIMIT);

  JsonValue recursed = {0};

  *out_recursed = recursed;

  SioReader* reader = it->parser->reader;

  char c;
  JSON_TRY(json_reader_read(reader, (uint8_t*) &c));

  JSON_ASSERT(
    (it->type == JSON_TYPE_ARRAY && c == '[')
    || (it->type == JSON_TYPE_OBJECT && c == '{')
  );

  JSON_ASSERT(!it->is_key);

  recursed = *it;
  recursed.container_type = it->type;
  recursed.depth++;

  JSON_TRY(json_skip_whitespace(reader));
  JSON_TRY(json_peek_required(reader, (uint8_t*) &c));

  if ((it->type == JSON_TYPE_ARRAY && c == ']')
    || (it->type == JSON_TYPE_OBJECT && c == '}')) {
    JSON_TRY(json_value_next(&recursed));
  } else {
    JSON_TRY(json_get_next_type(reader, &recursed.type));
    if (it->type == JSON_TYPE_OBJECT) {
      JSON_REQUIRE(recursed.type == JSON_TYPE_STRING, JSON_ERROR_INVALID);
      recursed.is_key = true;
    }
  }

  *out_recursed = recursed;
  return JSON_OK;
}

int
json_value_leave_container(
  JsonValue* it,
  JsonValue* recursed
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);

  JSON_ASSERT(it->type == JSON_TYPE_ARRAY || it->type == JSON_TYPE_OBJECT);
  JSON_ASSERT_EQ(it->type, recursed->container_type);

  while (!json_value_at_end(recursed)) {
    JSON_TRY(json_value_advance(recursed));
  }

  JSON_TRY(json_value_next(it));

  return JSON_OK;
}

bool
json_value_at_end(
  const JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);

  return it->at_end;
}

bool
json_value_is_null(
  const JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);

  return it->type == JSON_TYPE_NULL;
}

bool
json_value_is_bool(
  const JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);

  return it->type == JSON_TYPE_BOOL;
}

bool
json_value_is_number(
  const JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);

  return it->type == JSON_TYPE_NUMBER;
}

bool
json_value_is_string(
  const JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);

  return it->type == JSON_TYPE_STRING;
}

bool
json_value_is_array(
  const JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);

  return it->type == JSON_TYPE_ARRAY;
}

bool
json_value_is_object(
  const JsonValue* it
) {
  JSON_ASSERT_NE(it, NULL);

  return it->type == JSON_TYPE_OBJECT;
}
