#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                \
  do {                                                                  \
    if (!(condition)) {                                                 \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);   \
      exit(EXIT_FAILURE);                                               \
    }                                                                   \
  } while (0)

#define CHECK_OK(statement) CHECK((statement) == JSON_OK)

#define CHECK_EQ(a, b) CHECK((a) == (b))

#define CHECK_NE(a, b) CHECK((a) != (b))

#endif /* TEST_H */
