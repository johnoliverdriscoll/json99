#ifndef JSON_NUMBER_WRITER_H
#define JSON_NUMBER_WRITER_H

#include <json99/json.h>
#include <json99/json_writer.h>

/** Significant digits retained for binary64 rounding. */
#define JSON_NUMBER_WRITER_DIGITS 1100

/**
 * Caller-owned, allocation-free numeric writer. May be cast to `JsonWriter*`.
 * Keeps bounded conversion state rather than the original number text.
 * Keep it at a stable address while writing because its context points to it.
 */
typedef struct JsonNumberWriter JsonNumberWriter;

/**
 * Initializes or resets the writer for one JSON number, without allocating.
 * Feed it with `json_value_read_number` or write the number bytes directly.
 * Do not include whitespace or a terminator. Syntax errors are latched until
 * reinitialization. Digit counter exhaustion returns `JSON_ERROR_OUT_OF_RANGE`.
 * Exceeding the digit buffer records rounding information without failing.
 */
void json_number_writer_init(
  JsonNumberWriter* out_writer
);

/**
 * Converts the written number to an exact `int64_t`, including integral
 * decimal and exponent notation. Fractional or out-of-range values return
 * `JSON_ERROR_OUT_OF_RANGE`. Incomplete or invalid input returns
 * `JSON_ERROR_INVALID`, or a previously latched write error.
 * Does not modify the writer. Leaves output unchanged on error.
 * `out_value` may be `NULL` to check conversion without storing the result.
 */
int json_number_writer_get_int64(
  const JsonNumberWriter* writer,
  int64_t* out_value
);

/**
 * Converts the written number using `strtod`, with binary64 rounding.
 * Accepts subnormal values and signed zero, including underflow. Non-finite
 * results return `JSON_ERROR_OUT_OF_RANGE`. Requires IEEE-754 binary64 doubles.
 * Has the same input validation and output rules as the integer getter.
 * Neither getter consumes or resets the writer, so both may be called.
 */
int json_number_writer_get_double(
  const JsonNumberWriter* writer,
  double* out_value
);

/** Internal conversion state exposed for caller allocation. Do not modify. */
struct JsonNumberWriter {
  JsonWriter writer;
  char digits[JSON_NUMBER_WRITER_DIGITS];
  size_t stored;
  int64_t significant;
  int64_t fractional;
  int64_t trailing_zeros;
  int64_t exponent;
  bool negative;
  bool sticky;
  bool exponent_negative;
  unsigned state;
  int error;
};

#endif /* JSON_NUMBER_WRITER_H */
