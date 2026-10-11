#ifndef JSON_ASSERT_H
#define JSON_ASSERT_H

#include <assert.h>

#define JSON_ASSERT(cond) assert(cond)

#define JSON_ASSERT_EQ(a, b) JSON_ASSERT((a) == (b))

#define JSON_ASSERT_NE(a, b) JSON_ASSERT((a) != (b))

#define JSON_ASSERT_LT(a, b) JSON_ASSERT((a) < (b))

#define JSON_ASSERT_LTE(a, b) JSON_ASSERT((a) <= (b))

#define JSON_ASSERT_GT(a, b) JSON_ASSERT((a) > (b))

#define JSON_ASSERT_GTE(a, b) JSON_ASSERT((a) >= (b))

#endif /* JSON_ASSERT_H */
