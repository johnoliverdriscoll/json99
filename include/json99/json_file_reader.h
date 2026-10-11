#ifndef JSON_FILE_READER_H
#define JSON_FILE_READER_H

#include <stdio.h>
#include <json99/json.h>

/**
 * Caller-owned reader borrowing a `FILE` stream. May be cast to `JsonReader*`
 * for use with the parser. Keep this object at a stable address while in use
 * because its callback context points back to it. The caller is responsible
 * for closing the stream.
 */
typedef struct JsonFileReader JsonFileReader;

/**
 * Initializes `out_reader` to read from the current position of `file`.
 * `file` must be a non-`NULL` stream open for reading in binary mode.
 * Peeking uses `fgetc`/`ungetc`, so seeking is not required. The reader
 * does not rewind or close the stream.
 */
void json_file_reader_init(
  JsonFileReader* out_reader,
  FILE* file
);

/** Adapter state, initialized by `json_file_reader_init`. */
struct JsonFileReader {
  /** Reader interface passed to the parser. */
  JsonReader reader;
  /** Borrowed stream that must remain open while reading. */
  FILE* file;
};

#endif /* JSON_FILE_READER_H */
