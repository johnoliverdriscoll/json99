# json99

A forward-only JSON parser for C99 with caller-owned parsing contexts and
no heap allocation by the core parser.

## Design goals

**Forward-only parsing:** consume input sequentially without constructing a
document tree.

**Explicit memory ownership:** caller-owned parsing contexts, with allocation
available through optional convenience functions.

**Composable I/O:** read from custom sources and decode strings into
caller-selected destinations.

**Small C99 core:** keep file I/O and heap-backed helpers in separate
optional libraries.

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

## Build options

| Option | Default | Effect |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | `OFF` | Build shared libraries. |
| `JSON99_BUILD_TESTING` | Top-level project only | Build json99 tests. |
| `JSON99_WITH_STDIO` | `ON` | Build and install `json99_stdio`, the file reader. |
| `JSON99_WITH_HEAP` | `ON` | Build and install `json99_heap`, the allocating getters and dynamic buffer writer. |

## Parsing behavior

**Traversal**

Initialization identifies the root value. Type queries inspect the value
without consuming it. Validation happens as input is consumed. Successful reads
advance the iterator automatically. Use `json_value_advance` to skip and
validate a value, including its nested contents.

**Containers**

Iterating through an array yields elements. Iterating through an object
alternates string keys and values. Entering a container creates a child
iterator. Use the child to traverse the container, then leave it to resume
with the parent.

**Numbers**

`json_value_read_number` writes a number's original bytes to a `JsonWriter`,
validating its syntax without converting it. Pass `NULL` to validate and skip
numbers of any length without buffering them. A supplied writer can impose
its own storage limits. No terminator is written.

`JsonNumberWriter`, declared in `<json99/json_number_writer.h>`, accumulates
bounded conversion state without allocating. Initialize it with
`json_number_writer_init`, pass its writer interface to
`json_value_read_number`, then call `json_number_writer_get_int64` or
`json_number_writer_get_double`. Both getters leave the state unchanged.
Use a buffer writer instead when you need to retain the exact text.

The convenience functions use this number writer internally.
`json_value_get_int64` accepts exact
integral values, including `1.0` and `1e0`, within the `int64_t` range.
`json_value_get_double` may round, accepts underflow to subnormal values or
signed zero, and rejects non-finite results. Conversion range errors return
`JSON_ERROR_OUT_OF_RANGE`. These conversion errors occur after the iterator
has advanced and leave the output unchanged. Syntax and I/O failures still
follow the general rule that a failed iterator must not be resumed.

**Strings**

Strings and object keys decode to UTF-8, including escapes and
surrogate pairs. Invalid UTF-8, unpaired surrogates, and unescaped control
bytes are rejected.

**Ownership**

Parser and iterator storage belongs to the caller. Keep the parser, reader,
and borrowed input alive while parsing. Initialized adapters must stay at
stable addresses. The parser does not retain decoded values for rereading.

**Input boundaries**

Parsing consumes one root value and leaves trailing bytes unread.
`json_value_at_end` reports iterator completion, not stream EOF. Applications
requiring a complete document should allow only JSON whitespace between the
root value and EOF.

**Errors**

Operations return `JSON_OK` or an error code documented in
[`json.h`](include/json99/json.h). Failures may leave input consumed and
output partially written. There is no rollback. Stop traversal and release
owned resources.

## Examples

These examples use `TRY` to return errors to the caller. It is a local helper,
not part of the library. Use explicit error handling when cleanup or other
work must happen before returning.

```c
#define TRY(call)                 \
  do {                            \
    int try_result_ = (call);     \
    if (try_result_ != JSON_OK) { \
      return try_result_;         \
    }                             \
  } while (0)
```

### Read a value from memory

Initialize a buffer reader and parser, then read a boolean:

```c
#include <json99/json.h>
#include <json99/json_buffer_reader.h>

int main(void) {
  const char input[] = "true";
  JsonBufferReader reader;
  JsonParser parser;
  JsonValue it;
  bool value;

  json_buffer_reader_init(&reader, input, sizeof(input) - 1);
  TRY(json_parser_init((JsonReader*) &reader, 8, &parser, &it));

  if (!json_value_is_bool(&it)) {
    return 1;
  }

  TRY(json_value_get_bool(&it, &value));

  if (!value || !json_value_at_end(&it)) {
    return 1;
  }

  return 0;
}
```

### Read from a file

Use `JsonFileReader` from `<json99/json_file_reader.h>` with the
`json99_stdio` library:

```c
JsonFileReader reader;
json_file_reader_init(&reader, file);
int result = json_parser_init((JsonReader*) &reader, 16, &parser, &it);
```

`file` is an open binary `FILE*`. The reader borrows it at its current
position. The caller is responsible for closing the file.

### Traverse arrays and skip values

Read the first integer from an array and skip the rest:

```c
int read_first_number(JsonValue* it, int64_t* out_number) {
  JsonValue child;

  if (!json_value_is_array(it)) {
    return JSON_ERROR_INVALID;
  }

  TRY(json_value_enter_container(it, &child));

  if (json_value_at_end(&child)) {
    return JSON_ERROR_INVALID;
  }

  TRY(json_value_get_int64(&child, out_number));

  return json_value_leave_container(it, &child);
}
```

For `[42, {"unused": [true, null]}]`, the result is `42`.

### Decode strings into fixed storage

Read an array of strings using the same caller-provided buffer for each one.
Reset the writer before each read so the next string starts at the beginning:

```c
#include <json99/json_buffer_writer.h>

// Application function that processes the bytes before the next read.
void process_bytes(const void* bytes, size_t size);

int read_strings(JsonValue* it, void* buffer, size_t capacity) {
  if (!json_value_is_array(it)) {
    return JSON_ERROR_INVALID;
  }

  JsonBufferWriter writer;
  json_buffer_writer_init(&writer, buffer, capacity);

  JsonValue child;
  TRY(json_value_enter_container(it, &child));

  while (!json_value_at_end(&child)) {
    json_buffer_writer_reset(&writer);
    TRY(json_value_read_string(&child, (JsonWriter*) &writer));
    process_bytes(buffer, json_buffer_writer_get_size(&writer));
  }

  return json_value_leave_container(it, &child);
}
```

The decoded bytes have no terminator. Each string must fit within `capacity`,
or reading returns `JSON_ERROR_EOF`. Process or copy the bytes before the
next read overwrites them.

### Decode a string whose length is unknown

Allocate a C string, use it, and release it:

```c
#include <stdio.h>
#include <stdlib.h>

int print_string(JsonValue* it) {
  char* string;
  TRY(json_value_get_cstring(it, &string));
  int result = JSON_OK;
  if (puts(string) == EOF) {
    result = JSON_ERROR_IO;
  }
  free(string);
  return result;
}
```

For explicit scratch storage and copy-out operations, see
[`JsonDynamicBufferWriter`](include/json99/json_dynamic_buffer_writer.h).

### Compare a string without allocating

A custom writer can compare decoded bytes as they arrive. Use this helper
instead of allocating a string when matching a known value or object key:

```c
typedef struct StringMatch {
  const char* next;
  bool matches;
} StringMatch;

static int compare_byte(void* user, uint8_t byte) {
  StringMatch* match = user;
  if (match->matches) {
    if (*match->next != '\0' && (uint8_t) *match->next == byte) {
      match->next++;
    } else {
      match->matches = false;
    }
  }
  return JSON_OK;
}

int string_equals(JsonValue* it, const char* expected, bool* equal) {
  StringMatch match = {expected, true};
  JsonWriter writer = {&match, compare_byte};

  *equal = false;
  TRY(json_value_read_string(it, &writer));
  *equal = match.matches && *match.next == '\0';
  return JSON_OK;
}
```

Comparison uses decoded UTF-8, so `"en\u0061bled"` matches `"enabled"`.
Even a mismatch consumes and validates the whole string. Embedded zero bytes
never match the C string `expected`.

### Read object properties

Use `string_equals` from the previous example to read `enabled` without
allocating keys and skip any unknown properties:

```c
int string_equals(JsonValue* it, const char* expected, bool* equal);

int read_enabled(JsonValue* it, bool* enabled) {
  JsonValue child;

  if (!json_value_is_object(it)) {
    return JSON_ERROR_INVALID;
  }

  TRY(json_value_enter_container(it, &child));

  *enabled = false;
  while (!json_value_at_end(&child)) {

    bool matches;
    TRY(string_equals(&child, "enabled", &matches));

    if (matches) {
      TRY(json_value_get_bool(&child, enabled));
    } else {
      TRY(json_value_advance(&child));
    }
  }

  return json_value_leave_container(it, &child);
}
```

Missing `enabled` defaults to `false`. Repeated occurrences use the last value.
Keys containing U+0000 do not match `enabled` and are skipped.
