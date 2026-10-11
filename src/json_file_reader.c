#include <json99/json_file_reader.h>
#include "json_assert.h"
#include "json_require.h"
#include "json_try.h"

static int
json_file_reader_read(
  void* user,
  uint8_t* out
){
  JsonFileReader* reader = user;

  JSON_ASSERT_NE(reader, NULL);

  int c = fgetc(reader->file);

  if (c == EOF) {
    JSON_REQUIRE(!ferror(reader->file), JSON_ERROR_IO);
    JSON_TRY(JSON_ERROR_EOF);
  }

  JSON_REQUIRE(c >=0 && c < 256, JSON_ERROR_IO);

  if (out != NULL) {
    *out = (uint8_t) c;
  }
  return JSON_OK;
}

static int
json_file_reader_peek(
  void* user,
  uint8_t* out_peeked,
  bool* out_at_end
) {
  JsonFileReader* reader = user;

  JSON_ASSERT_NE(reader, NULL);
  JSON_ASSERT(out_peeked != NULL || out_at_end != NULL);

  int peeked = fgetc(reader->file);

  if (peeked == EOF) {
    JSON_REQUIRE(!ferror(reader->file), JSON_ERROR_IO);
    if (out_at_end != NULL) {
      *out_at_end = true;
    }
    return JSON_OK;
  }

  JSON_REQUIRE_EQ(
    ungetc(peeked, reader->file),
    peeked,
    JSON_ERROR_EOF
  );

  JSON_REQUIRE(peeked >= 0 && peeked < 256, JSON_ERROR_IO);

  if (out_peeked != NULL) {
    *out_peeked = (uint8_t) peeked;
  }
  if (out_at_end != NULL) {
    *out_at_end = false;
  }
  return JSON_OK;
}

void
json_file_reader_init(
  JsonFileReader* out_reader,
  FILE* file
) {
  JSON_ASSERT_NE(out_reader, NULL);

  *out_reader = (JsonFileReader) {
    .reader = {
      .user = out_reader,
      .read = json_file_reader_read,
      .peek = json_file_reader_peek,
    },
    .file = file,
  };
}
