#ifndef JSON_TRY_H
#define JSON_TRY_H

#include <json99/json.h>

#define JSON_TRY(call)                          \
  do {                                          \
    int json_try_result_ = (call);              \
    if (json_try_result_ != JSON_OK) {          \
      return json_try_result_;                  \
    }                                           \
  } while(0)

#endif /* JSON_TRY_H */
