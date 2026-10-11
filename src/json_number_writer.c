#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <json99/json_number_writer.h>
#include "json_assert.h"
#include "json_require.h"
#include "json_try.h"

#if DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024 || DBL_MIN_EXP != -1021
#error "JSON double conversion requires IEEE-754 binary64."
#endif

/*
 * Binary64 rounding boundaries have terminating decimal expansions with at
 * most 1075 fractional places. Keeping 1100 significant digits and a sticky
 * digit preserves which side of every boundary the input occupies.
 */
#define JSON_NUMBER_COUNT_MAX (INT64_MAX / 4)

enum {
  JSON_NUMBER_START,
  JSON_NUMBER_SIGN,
  JSON_NUMBER_ZERO,
  JSON_NUMBER_INTEGER,
  JSON_NUMBER_POINT,
  JSON_NUMBER_FRACTION,
  JSON_NUMBER_EXPONENT,
  JSON_NUMBER_EXPONENT_SIGN,
  JSON_NUMBER_EXPONENT_DIGITS,
};

static int
json_number_digit(
  JsonNumberWriter* number,
  uint8_t digit,
  bool fractional
) {
  if (fractional) {
    JSON_REQUIRE_LT(
      number->fractional,
      JSON_NUMBER_COUNT_MAX,
      JSON_ERROR_OUT_OF_RANGE
    );
    number->fractional++;
  }

  if (number->significant == 0 && digit == '0') {
    return JSON_OK;
  }

  JSON_REQUIRE_LT(
    number->significant,
    JSON_NUMBER_COUNT_MAX,
    JSON_ERROR_OUT_OF_RANGE
  );
  number->significant++;
  if (digit == '0') {
    number->trailing_zeros++;
  } else {
    number->trailing_zeros = 0;
  }

  if (number->stored < sizeof(number->digits)) {
    number->digits[number->stored++] = (char) digit;
  } else if (digit != '0') {
    number->sticky = true;
  }

  return JSON_OK;
}

static int
json_number_writer_write_byte(
  JsonNumberWriter* number,
  uint8_t c
) {
  bool digit = c >= '0' && c <= '9';
  switch (number->state) {
  case JSON_NUMBER_START:
    if (c == '-') {
      number->negative = true;
      number->state = JSON_NUMBER_SIGN;
      return JSON_OK;
    }
    /* fall through */
  case JSON_NUMBER_SIGN:
    JSON_REQUIRE(digit, JSON_ERROR_INVALID);
    number->state = c == '0' ? JSON_NUMBER_ZERO : JSON_NUMBER_INTEGER;
    return json_number_digit(number, c, false);
  case JSON_NUMBER_ZERO:
  case JSON_NUMBER_INTEGER:
    if (digit) {
      JSON_REQUIRE(number->state == JSON_NUMBER_INTEGER, JSON_ERROR_INVALID);
      return json_number_digit(number, c, false);
    }
    if (c == '.') {
      number->state = JSON_NUMBER_POINT;
      return JSON_OK;
    }
    break;
  case JSON_NUMBER_POINT:
    JSON_REQUIRE(digit, JSON_ERROR_INVALID);
    number->state = JSON_NUMBER_FRACTION;
    return json_number_digit(number, c, true);
  case JSON_NUMBER_FRACTION:
    if (digit) {
      return json_number_digit(number, c, true);
    }
    break;
  case JSON_NUMBER_EXPONENT:
    if (c == '+' || c == '-') {
      number->exponent_negative = c == '-';
      number->state = JSON_NUMBER_EXPONENT_SIGN;
      return JSON_OK;
    }
    /* fall through */
  case JSON_NUMBER_EXPONENT_SIGN:
  case JSON_NUMBER_EXPONENT_DIGITS:
    JSON_REQUIRE(digit, JSON_ERROR_INVALID);
    number->state = JSON_NUMBER_EXPONENT_DIGITS;
    int64_t value = c - '0';
    if (number->exponent > (INT64_MAX / 2 - value) / 10) {
      number->exponent = INT64_MAX / 2;
    } else {
      number->exponent = number->exponent * 10 + value;
    }
    return JSON_OK;
  default:
    return JSON_ERROR_INVALID;
  }

  JSON_REQUIRE(c == 'e' || c == 'E', JSON_ERROR_INVALID);
  number->state = JSON_NUMBER_EXPONENT;
  return JSON_OK;
}

static int
json_number_writer_write(
  void* user,
  uint8_t c
) {
  JsonNumberWriter* number = user;
  JSON_ASSERT_NE(number, NULL);

  if (number->error == JSON_OK) {
    number->error = json_number_writer_write_byte(number, c);
  }
  return number->error;
}

void
json_number_writer_init(
  JsonNumberWriter* out_writer
) {
  JSON_ASSERT_NE(out_writer, NULL);

  *out_writer = (JsonNumberWriter) {
    .writer = {
      .user = out_writer,
      .write = json_number_writer_write,
    },
  };
}

static int
json_number_writer_validate(
  const JsonNumberWriter* number
) {
  JSON_ASSERT_NE(number, NULL);

  JSON_TRY(number->error);
  JSON_REQUIRE(
    number->state == JSON_NUMBER_ZERO
      || number->state == JSON_NUMBER_INTEGER
      || number->state == JSON_NUMBER_FRACTION
      || number->state == JSON_NUMBER_EXPONENT_DIGITS,
    JSON_ERROR_INVALID
  );
  return JSON_OK;
}

int
json_number_writer_get_double(
  const JsonNumberWriter* number,
  double* out_value
) {
  JSON_TRY(json_number_writer_validate(number));
  int64_t signed_exponent = number->exponent_negative
    ? -number->exponent : number->exponent;

  double value;
  if (number->significant == 0) {
    if (number->negative) {
      value = -0.0;
    } else {
      value = 0.0;
    }
  } else {
    char text[JSON_NUMBER_WRITER_DIGITS + 32];
    size_t size = 0;
    if (number->negative) {
      text[size++] = '-';
    }
    memcpy(&text[size], number->digits, number->stored);
    size += number->stored;

    int64_t exponent =
      signed_exponent
      - number->fractional
      + number->significant
      - (int64_t) number->stored;
    if (number->sticky) {
      text[size++] = '1';
      exponent--;
    }

    text[size++] = 'e';

    uint64_t magnitude;
    if (exponent < 0) {
      text[size++] = '-';
      magnitude = (uint64_t) (-(exponent + 1)) + 1;
    } else {
      magnitude = (uint64_t) exponent;
    }

    size_t exponent_start = size;
    do {
      text[size++] = (char) ('0' + magnitude % 10);
      magnitude /= 10;
    } while (magnitude != 0);

    size_t left = exponent_start;
    size_t right = size - 1;
    while (left < right) {
      char digit = text[left];
      text[left++] = text[right];
      text[right--] = digit;
    }
    text[size] = '\0';

    value = strtod(text, NULL);
    JSON_REQUIRE(isfinite(value), JSON_ERROR_OUT_OF_RANGE);
  }

  if (out_value != NULL) {
    *out_value = value;
  }
  return JSON_OK;
}

int
json_number_writer_get_int64(
  const JsonNumberWriter* number,
  int64_t* out_value
) {
  JSON_TRY(json_number_writer_validate(number));
  int64_t signed_exponent = number->exponent_negative
    ? -number->exponent : number->exponent;

  uint64_t magnitude = 0;
  if (number->significant != 0) {
    int64_t shift = signed_exponent - number->fractional;
    JSON_REQUIRE(
      shift >= 0 || -shift <= number->trailing_zeros,
      JSON_ERROR_OUT_OF_RANGE
    );
    int64_t digits = number->significant + shift;
    JSON_REQUIRE(digits <= 19, JSON_ERROR_OUT_OF_RANGE);
    JSON_ASSERT(digits > 0);
    uint64_t limit;
    if (number->negative) {
      limit = (uint64_t) INT64_MAX + 1;
    } else {
      limit = (uint64_t) INT64_MAX;
    }
    for (int64_t i = 0; i < digits; i++) {
      uint64_t digit;
      if (i < number->significant) {
        digit = (uint64_t) (number->digits[i] - '0');
      } else {
        digit = 0;
      }
      JSON_REQUIRE(magnitude <= (limit - digit) / 10, JSON_ERROR_OUT_OF_RANGE);
      magnitude = magnitude * 10 + digit;
    }
  }

  int64_t value;
  if (number->negative && magnitude != 0) {
    value = -(int64_t) (magnitude - 1) - 1;
  } else {
    value = (int64_t) magnitude;
  }

  if (out_value != NULL) {
    *out_value = value;
  }
  return JSON_OK;
}
