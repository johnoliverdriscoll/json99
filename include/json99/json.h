#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sio99/sio.h>

/**
 * Result codes returned by json99 operations.
 * `JSON_OK` indicates success. Any other result indicates failure.
 * Reader EOF becomes `JSON_ERROR_EOF`. Other reader failures and all byte
 * writer failures become `JSON_ERROR_IO`. I/O implementations may retain
 * their own detailed errors for callers to inspect.
 */
enum {

  /** Operation finished successfully. */
  JSON_OK,

  /** Required input exhausted. */
  JSON_ERROR_EOF,

  /** A reader or writer operation failed. */
  JSON_ERROR_IO,

  /** Invalid syntax or unexpected value encountered. */
  JSON_ERROR_INVALID,

  /** Numeric overflow encountered. */
  JSON_ERROR_OVERFLOW,

  /** Number cannot be represented by the requested type or parser limits. */
  JSON_ERROR_OUT_OF_RANGE,

  /** Maximum container nesting depth exceeded. */
  JSON_ERROR_DEPTH_LIMIT,

  /** Memory allocation failed. */
  JSON_ERROR_NO_MEMORY,

  /** Unknown error encountered. */
  JSON_ERROR_UNKNOWN,
};

/**
 * JSON value types.
 */
typedef enum JsonType {
  JSON_TYPE_NULL,
  JSON_TYPE_BOOL,
  JSON_TYPE_NUMBER,
  JSON_TYPE_STRING,
  JSON_TYPE_ARRAY,
  JSON_TYPE_OBJECT,
} JsonType;

/**
 * Caller-owned iterator pointing to a value in a JSON stream.
 * Object traversal alternates string keys and their values. Consuming a
 * key advances to its value, and consuming that value advances to the next key.
 * Check `json_value_at_end` before querying or consuming a value.
 *
 * Successful consuming operations advance the iterator. At the root,
 * completion marks it at end without consuming trailing bytes or requiring
 * EOF. Inside a container, advancement also consumes separators and
 * whitespace up to the next value, or the closing delimiter.
 *
 * On error, input and output may already have been consumed or modified.
 * There is no rollback, so do not resume traversal with the failed iterator,
 * except after a completed read followed by a numeric conversion range error
 * as documented by the numeric getters.
 */
typedef struct JsonValue JsonValue;

/**
 * Caller-owned JSON parsing context. Borrows its reader and allocates no
 * storage. Keep the parser and reader alive at stable addresses while
 * iterators refer to them.
 */
typedef struct JsonParser JsonParser;

/**
 * Writer interface accepting UTF-8 bytes and UTF-16 code units.
 */
typedef struct JsonUnicodeWriter JsonUnicodeWriter;

/**
 * Initializes `out_parser` with the borrowed `reader` and returns the root
 * iterator in `out_it`. Consumes leading JSON whitespace and identifies
 * the value type without consuming or fully validating the value.
 * Empty or whitespace-only input returns `JSON_ERROR_EOF`.
 *
 * `max_depth` limits nested containers, counting a root container as one.
 * A limit of zero permits scalar values only. Skipping containers uses
 * recursion. The parser consumes one root value, without requiring end of
 * stream.
 */
int json_parser_init(
  SioReader* reader,
  unsigned max_depth,
  JsonParser* out_parser,
  JsonValue* out_it
);

/**
 * Returns the identified type at `it` without consuming or validating it.
 * `it` must not be at end.
 */
JsonType json_value_get_type(
  const JsonValue* it
);

/**
 * Reads the boolean value at `it` into `out_value`, consuming its encoded
 * bytes and advancing `it` to the next value or the end of the container.
 * `out_value` may be `NULL` to discard the result.
 * Returns `JSON_ERROR_INVALID` if the value is not a boolean.
 */
int json_value_get_bool(
  JsonValue* it,
  bool* out_value
);

/**
 * Reads the number value at `it` into `out_value`, consuming its encoded
 * bytes and advancing `it` to the next value or the end of the container.
 * Reads through `JsonNumberWriter` before converting. Conversion range errors
 * leave the iterator advanced and output unchanged.
 * Conversion uses `strtod` and may round. Requires IEEE-754 binary64 doubles.
 * Underflow to a subnormal or signed zero is accepted. A non-finite result
 * returns `JSON_ERROR_OUT_OF_RANGE`. `out_value` may be `NULL` to discard
 * the result while still checking conversion. A non-number returns
 * `JSON_ERROR_INVALID`.
 */
int json_value_get_double(
  JsonValue* it,
  double* out_value
);

/**
 * Reads the number value at `it` into `out_value`, consuming its encoded
 * bytes and advancing `it` to the next value or the end of the container.
 * Reads through `JsonNumberWriter` before converting. Conversion range errors
 * leave the iterator advanced and output unchanged.
 * The number must have an exact integral value within the `int64_t` range,
 * including when written in decimal or exponent notation. Fractional or
 * out-of-range values return `JSON_ERROR_OUT_OF_RANGE`. A non-number returns
 * `JSON_ERROR_INVALID`. `out_value` may be `NULL` to discard the result while
 * still checking conversion.
 */
int json_value_get_int64(
  JsonValue* it,
  int64_t* out_value
);

/**
 * Decodes the JSON string at `it` into a caller-owned allocation returned
 * in `out_bytes`, with its byte count in `out_size`. Uses the same decoding
 * rules as `json_value_read_string`: embedded zeros are allowed and no
 * terminator is added. An empty string returns `NULL` and zero.
 * Both output pointers must be non-`NULL`. On failure, outputs are `NULL`
 * and zero. The caller must free the returned allocation with `free`.
 * Requires linking `json99_heap`. Final allocation failure returns
 * `JSON_ERROR_NO_MEMORY`. Scratch-writer failures return `JSON_ERROR_IO`.
 * Parsing consumes and advances `it` before the final allocation, so a
 * subsequent failure does not restore the iterator.
 */
int json_value_get_bytes(
  JsonValue* it,
  void** out_bytes,
  size_t* out_size
);

/**
 * Decodes the JSON string at `it` into a caller-owned, null-terminated
 * UTF-8 string returned in `out_cstring`. Uses the decoding rules of
 * `json_value_read_string`, but rejects embedded U+0000 with
 * `JSON_ERROR_INVALID`. An empty string returns an allocation containing
 * only the terminator. `out_cstring` must be non-`NULL`. On failure,
 * `*out_cstring` is `NULL`.
 * The caller must free the returned allocation with `free`.
 * Requires linking `json99_heap`. Final allocation failure returns
 * `JSON_ERROR_NO_MEMORY`. Scratch-writer failures return `JSON_ERROR_IO`.
 * Parsing consumes and advances `it` before final allocation and C-string
 * validation. Failures do not restore the iterator.
 */
int json_value_get_cstring(
  JsonValue* it,
  char** out_cstring
);

/**
 * Consumes and validates the value at `it`, including any nested values,
 * and advances `it` to the next value or the end of the container.
 * Numbers are checked for syntax without conversion to a C numeric type.
 */
int json_value_advance(
  JsonValue* it
);

/**
 * Enters the array or object at `it`, initializing `out_recursed` to its first
 * value, or its end if empty. For objects, the first value is a property
 * key. Use `out_recursed` to traverse the container, then call
 * `json_value_leave_container` to resume traversal with `it`.
 * `it` must identify a container and `out_recursed` must be distinct from it.
 * Do not consume the parent while the child is active. Returns
 * `JSON_ERROR_DEPTH_LIMIT` if entering would exceed the configured limit.
 */
int json_value_enter_container(
  JsonValue* it,
  JsonValue* out_recursed
);

/**
 * Leaves the entered container, consuming and validating any remaining
 * values and advancing the parent iterator to the next value or the end
 * of its container. `recursed` must be the child entered from `it`, with
 * any nested traversal already left. On success, the child iterator must
 * not be used afterward. Leaving a root container completes the root value.
 */
int json_value_leave_container(
  JsonValue* it,
  JsonValue* recursed
);

/**
 * Returns `true` if `it` has completed its container or root value.
 * Does not read input or test whether the underlying stream is at EOF.
 */
bool json_value_at_end(
  const JsonValue* it
);

/**
 * Returns `true` if `it` points to a null value.
 */
bool json_value_is_null(
  const JsonValue* it
);

/**
 * Returns `true` if `it` points to a boolean value.
 */
bool json_value_is_bool(
  const JsonValue* it
);

/**
 * Returns `true` if `it` points to a number value.
 */
bool json_value_is_number(
  const JsonValue* it
);

/**
 * Returns `true` if `it` points to a string value.
 */
bool json_value_is_string(
  const JsonValue* it
);

/**
 * Returns `true` if `it` points to an array value.
 */
bool json_value_is_array(
  const JsonValue* it
);

/**
 * Returns `true` if `it` points to an object value.
 */
bool json_value_is_object(
  const JsonValue* it
);

/**
 * Validates the JSON number at `it` and writes its exact original bytes,
 * without a terminator. Preserves signs, decimal digits, and exponent spelling.
 * `writer` may be `NULL` to validate and discard without numeric conversion
 * or number-length limits. No numeric accumulation or allocation is performed
 * by this function. A supplied writer may impose its own limits.
 * On success, consumes the number and advances `it`. Syntax, reader, or writer
 * errors may leave partial output and consumed input. A non-number returns
 * `JSON_ERROR_INVALID`. Root completion does not require end of stream.
 */
int json_value_read_number(
  JsonValue* it,
  SioWriter* writer
);

/**
 * Decodes the JSON string at `it` into UTF-8 bytes written to `writer`,
 * without a terminating null byte. Decodes escapes and surrogate pairs and
 * allows embedded U+0000. Rejects invalid UTF-8, invalid escapes,
 * unpaired surrogates, unescaped control bytes, and non-string values with
 * `JSON_ERROR_INVALID`. `writer` may be `NULL` to validate and discard.
 * On success, consumes the string and advances `it` to the next value or
 * the end of the container. Writer errors may leave partial output.
 */
int json_value_read_string(
  JsonValue* it,
  SioWriter* writer
);

/**
 * Reads a JSON string into a required Unicode sink. Unescaped bytes and simple
 * escapes go to `write_utf8`. Each `\uXXXX` escape becomes a numeric UTF-16
 * unit passed to `write_utf16`. Checks JSON syntax and rejects unescaped
 * control bytes. The sink is responsible for UTF-8 and surrogate-pair
 * validation. Calls `finish` after the closing quote and before advancing `it`.
 * Callback errors propagate unchanged. Failure may consume input and produce
 * partial output. No rollback or allocation is performed by this function.
 */
int json_value_read_unicode(
  JsonValue* it,
  JsonUnicodeWriter* writer
);

/**
 * Reads the JSON string at `it`, preserving escape sequences exactly as
 * written. Writes the original contents without surrounding quotes or a
 * terminating null byte. Validates escapes, UTF-8, and surrogate pairs without
 * converting the output. Rejects malformed strings and non-string values with
 * `JSON_ERROR_INVALID`. `writer` may be `NULL` to validate and discard.
 * On success, consumes the string and advances `it` to the next value or
 * the end of the container. Reader and writer failures are reported using
 * json99 result codes as described above.
 * Failure may leave consumed input and partial output.
 */
int json_value_read_raw_string(
  JsonValue* it,
  SioWriter* writer
);

/** Internal iterator state exposed for caller allocation. Do not modify. */
struct JsonValue {
  JsonParser* parser;
  JsonType type;
  JsonType container_type;
  unsigned depth;
  bool at_end;
  bool is_key;
};

/** Internal parser state exposed for caller allocation. Do not modify. */
struct JsonParser {
  JsonValue it;
  SioReader* reader;
  unsigned max_depth;
};

#endif /* JSON_H */

