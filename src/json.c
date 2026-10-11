#include <string.h>
#include <stdlib.h>
#include <json99/json.h>
#include <json99/json_number_writer.h>
#include "json_assert.h"
#include "json_require.h"
#include "json_try.h"

static int
json_reader_peek(
  JsonReader* reader,
  uint8_t* out_peeked,
  bool* out_at_end
) {
  JSON_ASSERT_NE(reader, NULL);
  JSON_ASSERT_NE(reader->peek, NULL);

  return reader->peek(reader->user, out_peeked, out_at_end);
}

static int
json_reader_read(
  JsonReader* reader,
  uint8_t* out_read
) {
  JSON_ASSERT_NE(reader, NULL);
  JSON_ASSERT_NE(reader->read, NULL);

  return reader->read(reader->user, out_read);
}

static int
json_writer_write(
  JsonWriter* writer,
  uint8_t val
) {
  if (writer == NULL) {
    return JSON_OK;
  }

  return writer->write(writer->user, val);
}

static int
json_peek_required(
  JsonReader* reader,
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
  JsonReader* reader
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
  JsonReader* reader,
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
  JsonReader* reader,
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
  JsonReader* reader,
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
  }

  if (out != NULL) {
    *out = strtol(value, NULL, 16);
  }
  return JSON_OK;
}

static int
json_read_unicode(
  JsonReader* reader,
  uint32_t *out
) {
  uint16_t first;
  JSON_TRY(json_read_unicode_unit(reader, &first));

  if (first >= 0xd800 && first <= 0xdbff) {

    JSON_TRY(json_read_expected(reader, "\\u"));

    uint16_t second;
    JSON_TRY(json_read_unicode_unit(reader, &second));

    JSON_REQUIRE(second >= 0xdc00 && second <= 0xdfff, JSON_ERROR_INVALID);

    if (out != NULL) {
      *out = 0x10000u
        + (((uint32_t) first - 0xd800u) << 10)
        + ((uint32_t) second - 0xdc00u);
    }
    return JSON_OK;
  }

  JSON_REQUIRE(first < 0xd800 || first > 0xdfff, JSON_ERROR_INVALID);

  if (out != NULL) {
    *out = first;
  }
  return JSON_OK;
}

static int
json_write_utf8(
  JsonWriter* writer,
  uint32_t codepoint
) {
  JSON_REQUIRE(
    codepoint <= 0x10ffff && (codepoint < 0xd800 || codepoint > 0xdfff),
    JSON_ERROR_INVALID
  );

  uint8_t bytes[4];
  size_t size;

  if (codepoint <= 0x7f) {
    bytes[0] = (uint8_t) codepoint;
    size = 1;
  } else if (codepoint <= 0x7ff) {
    bytes[0] = 0xc0 | (codepoint >> 6);
    bytes[1] = 0x80 | (codepoint & 0x3f);
    size = 2;
  } else if (codepoint <= 0xffff) {
    bytes[0] = 0xe0 | (codepoint >> 12);
    bytes[1] = 0x80 | ((codepoint >> 6) & 0x3f);
    bytes[2] = 0x80 | (codepoint & 0x3f);
    size = 3;
  } else {
    bytes[0] = 0xf0 | (codepoint >> 18);
    bytes[1] = 0x80 | ((codepoint >> 12) & 0x3f);
    bytes[2] = 0x80 | ((codepoint >> 6) & 0x3f);
    bytes[3] = 0x80 | (codepoint & 0x3f);
    size = 4;
  }

  for (size_t i = 0; i < size; i++) {
    JSON_TRY(json_writer_write(writer, bytes[i]));
  }

  return JSON_OK;
}

int
json_parser_init(
  JsonReader* reader,
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

  JsonReader* reader = it->parser->reader;

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
  
  JsonReader* reader = it->parser->reader;

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

  JsonReader* reader = it->parser->reader;

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
  JsonWriter* writer
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);
  JSON_REQUIRE(it->type == JSON_TYPE_NUMBER, JSON_ERROR_INVALID);

  JsonReader* reader = it->parser->reader;

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
  JSON_TRY(json_value_read_number(it, &writer.writer));
  return json_number_writer_get_double(&writer, out_value);
}

int
json_value_get_int64(
  JsonValue* it,
  int64_t* out_value
) {
  JsonNumberWriter writer;
  json_number_writer_init(&writer);
  JSON_TRY(json_value_read_number(it, &writer.writer));
  return json_number_writer_get_int64(&writer, out_value);
}

int
json_value_read_string(
  JsonValue* it,
  JsonWriter* writer
) {
  JSON_ASSERT_NE(it, NULL);
  JSON_ASSERT_NE(it->parser, NULL);

  JSON_REQUIRE(it->type == JSON_TYPE_STRING, JSON_ERROR_INVALID);

  JsonReader* reader = it->parser->reader;

  uint8_t u[4];
  JSON_TRY(json_reader_read(reader, &u[0]));

  JSON_ASSERT_EQ(u[0], '"');

  for (;;) {
    JSON_TRY(json_reader_read(reader, &u[0]));
    if (u[0] == '"') {
      break;
    }
    if (u[0] == '\\') {
      JSON_TRY(json_reader_read(reader, &u[0]));
      uint32_t codepoint;
      switch (u[0]) {
      case '"':
        JSON_TRY(json_writer_write(writer, '"'));
        break;
      case '\\':
        JSON_TRY(json_writer_write(writer, '\\'));
        break;
      case '/':
        JSON_TRY(json_writer_write(writer, '/'));
        break;
      case 'b':
        JSON_TRY(json_writer_write(writer, '\b'));
        break;
      case 'f':
        JSON_TRY(json_writer_write(writer, '\f'));
        break;
      case 'n':
        JSON_TRY(json_writer_write(writer, '\n'));
        break;
      case 'r':
        JSON_TRY(json_writer_write(writer, '\r'));
        break;
      case 't':
        JSON_TRY(json_writer_write(writer, '\t'));
        break;
      case 'u': 
        JSON_TRY(json_read_unicode(reader, &codepoint));
        JSON_TRY(json_write_utf8(writer, codepoint));
        break;
      default:
        JSON_TRY(JSON_ERROR_INVALID);
      }
      continue;
    }
    JSON_REQUIRE_GTE(u[0], 0x20, JSON_ERROR_INVALID);
    uint32_t size = 1;
    if (u[0] >= 0xc2 && u[0] <= 0xdf) {
      JSON_TRY(json_reader_read(reader, &u[1]));
      JSON_REQUIRE(u[1] >= 0x80 && u[1] <= 0xbf, JSON_ERROR_INVALID);
      size = 2;
    } else if (u[0] == 0xe0) {
      JSON_TRY(json_reader_read(reader, &u[1]));
      JSON_REQUIRE(u[1] >= 0xa0 && u[1] <= 0xbf, JSON_ERROR_INVALID);
      JSON_TRY(json_reader_read(reader, &u[2]));
      JSON_REQUIRE(u[2] >= 0x80 && u[2] <= 0xbf, JSON_ERROR_INVALID);
      size = 3;
    } else if (u[0] >= 0xe1 && u[0] <= 0xec) {
      JSON_TRY(json_reader_read(reader, &u[1]));
      JSON_REQUIRE(u[1] >= 0x80 && u[1] <= 0xbf, JSON_ERROR_INVALID);
      JSON_TRY(json_reader_read(reader, &u[2]));
      JSON_REQUIRE(u[2] >= 0x80 && u[2] <= 0xbf, JSON_ERROR_INVALID);
      size = 3;
    } else if (u[0] == 0xed) {
      JSON_TRY(json_reader_read(reader, &u[1]));
      JSON_REQUIRE(u[1] >= 0x80 && u[1] <= 0x9f, JSON_ERROR_INVALID);
      JSON_TRY(json_reader_read(reader, &u[2]));
      JSON_REQUIRE(u[2] >= 0x80 && u[2] <= 0xbf, JSON_ERROR_INVALID);
      size = 3;
    } else if (u[0] >= 0xee && u[0] <= 0xef) {
      JSON_TRY(json_reader_read(reader, &u[1]));
      JSON_REQUIRE(u[1] >= 0x80 && u[1] <= 0xbf, JSON_ERROR_INVALID);
      JSON_TRY(json_reader_read(reader, &u[2]));
      JSON_REQUIRE(u[2] >= 0x80 && u[2] <= 0xbf, JSON_ERROR_INVALID);
      size = 3;
    } else if (u[0] == 0xf0) {
      JSON_TRY(json_reader_read(reader, &u[1]));
      JSON_REQUIRE(u[1] >= 0x90 && u[1] <= 0xbf, JSON_ERROR_INVALID);
      JSON_TRY(json_reader_read(reader, &u[2]));
      JSON_REQUIRE(u[2] >= 0x80 && u[2] <= 0xbf, JSON_ERROR_INVALID);
      JSON_TRY(json_reader_read(reader, &u[3]));
      JSON_REQUIRE(u[3] >= 0x80 && u[3] <= 0xbf, JSON_ERROR_INVALID);
      size = 4;
    } else if (u[0] >= 0xf1 && u[0] <= 0xf3) {
      JSON_TRY(json_reader_read(reader, &u[1]));
      JSON_REQUIRE(u[1] >= 0x80 && u[1] <= 0xbf, JSON_ERROR_INVALID);
      JSON_TRY(json_reader_read(reader, &u[2]));
      JSON_REQUIRE(u[2] >= 0x80 && u[2] <= 0xbf, JSON_ERROR_INVALID);
      JSON_TRY(json_reader_read(reader, &u[3]));
      JSON_REQUIRE(u[3] >= 0x80 && u[3] <= 0xbf, JSON_ERROR_INVALID);
      size = 4;
    } else if (u[0] == 0xf4) {
      JSON_TRY(json_reader_read(reader, &u[1]));
      JSON_REQUIRE(u[1] >= 0x80 && u[1] <= 0x8f, JSON_ERROR_INVALID);
      JSON_TRY(json_reader_read(reader, &u[2]));
      JSON_REQUIRE(u[2] >= 0x80 && u[2] <= 0xbf, JSON_ERROR_INVALID);
      JSON_TRY(json_reader_read(reader, &u[3]));
      JSON_REQUIRE(u[3] >= 0x80 && u[3] <= 0xbf, JSON_ERROR_INVALID);
      size = 4;
    } else {
      JSON_REQUIRE_LT(u[0], 0x80, JSON_ERROR_INVALID);
    }
    for (size_t i = 0; i < size; i++) {
      JSON_TRY(json_writer_write(writer, u[i]));
    }
  }

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

  JsonReader* reader = it->parser->reader;

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
