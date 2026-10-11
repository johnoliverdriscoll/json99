#ifndef JSON_REQUIRE_H
#define JSON_REQUIRE_H

#define JSON_REQUIRE(cond, val)                 \
  do {                                          \
    if (!(cond)) {                              \
      return (val);                             \
    }                                           \
  } while (0)

#define JSON_REQUIRE(cond, val)                 \
  do {                                          \
    if (!(cond)) {                              \
      return (val);                             \
    }                                           \
  } while (0)

#define JSON_REQUIRE_EQ(a, b, val)              \
  do {                                          \
    if ((a) != (b)) {                           \
      return (val);                             \
    }                                           \
  } while (0)

#define JSON_REQUIRE_NE(a, b, val)              \
  do {                                          \
    if ((a) == (b)) {                           \
      return (val);                             \
    }                                           \
  } while (0)

#define JSON_REQUIRE_LT(a, b, val)              \
  do {                                          \
    if ((a) >= (b)) {                           \
      return (val);                             \
    }                                           \
  } while (0)

#define JSON_REQUIRE_LTE(a, b, val)             \
  do {                                          \
    if ((a) > (b)) {                            \
      return (val);                             \
    }                                           \
  } while (0)

#define JSON_REQUIRE_GT(a, b, val)              \
  do {                                          \
    if ((a) <= (b)) {                           \
      return (val);                             \
    }                                           \
  } while (0)

#define JSON_REQUIRE_GTE(a, b, val)             \
  do {                                          \
    if ((a) < (b)) {                            \
      return (val);                             \
    }                                           \
  } while (0)

#endif /* JSON_REQUIRE_H */
