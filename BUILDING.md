# Building json99

The [README](README.md#building) covers the default build. Run the commands
below from the repository root.

## Installation

```sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/your/install/prefix
cmake --build build
cmake --install build
```

Installed consumers can use `pkg-config --cflags --libs json99`. Set
`PKG_CONFIG_PATH` when installing outside the default search paths.

## Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | `OFF` | Build shared libraries instead of static libraries |
| `JSON99_BUILD_TESTING` | `ON` for standalone builds | Build and register json99 tests |
| `JSON99_WITH_HEAP` | `ON` | Build `json99_heap`, the allocating string getters |
| `JSON99_USE_SYSTEM_DEPENDENCIES` | `OFF` | Find installed dependencies through pkg-config |
| `SIO99_WITH_STDIO` | `ON` in the bundled build | Build the file reader |
| `SIO99_WITH_HEAP` | Follows `JSON99_WITH_HEAP` unless explicitly set | Build the dynamic buffer writer |

For a build without file or heap adapters:

```sh
cmake -S . -B build-core -DJSON99_WITH_HEAP=OFF \
  -DSIO99_WITH_HEAP=OFF -DSIO99_WITH_STDIO=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

`JSON99_WITH_HEAP` requires the `sio99_heap` target. Dependency tests default
to OFF when bundled unless explicitly enabled with `SIO99_BUILD_TESTING` or
`UTF99_BUILD_TESTING`.

## Dependencies

sio99 provides byte readers and writers. utf99 provides Unicode decoding and
validation. Both are pinned Git submodules under `src`. Populate an existing
clone with:

```sh
git submodule update --init --recursive
```

For each dependency, CMake first reuses an existing target named `sio99` or
`utf99`. Otherwise it builds the bundled submodule, or finds the installed
package when `JSON99_USE_SYSTEM_DEPENDENCIES=ON`.

sio99 is added before utf99 so utf99 reuses the same target. Its nested sio99
submodule is not built separately. A parent project can supply both:

```cmake
add_subdirectory(src/sio99)
add_subdirectory(src/utf99)
add_subdirectory(src/json99)
target_link_libraries(my_application PRIVATE json99)
```

Link `json99_heap` for allocating getters and `sio99_stdio` for file reading.
Those targets carry their core dependencies. json99 publicly links sio99 and
utf99, including their public headers and libraries.

For installed dependencies:

```sh
cmake -S . -B build -DJSON99_USE_SYSTEM_DEPENDENCIES=ON
```

This requires pkg-config and installed sio99 and utf99 development files.
The heap adapter must also be installed when `JSON99_WITH_HEAP=ON`.
Installing a bundled build installs its dependencies. Existing or installed
dependencies remain managed by their owner.

## Migrating existing callers

Use `SioReader` and `SioWriter` in place of `JsonReader` and `JsonWriter`.
Use the `SioBufferReader`, `SioBufferWriter`, `SioFileReader`, and
`SioDynamicBufferWriter` adapters and their `sio_*` functions. Cast reader
adapters to `SioReader*`, as their embedded interface now supports seeking.

`json99_stdio` and `JSON99_WITH_STDIO` are replaced by `sio99_stdio` and
`SIO99_WITH_STDIO`. `json99_heap` retains the allocating JSON getters.
The sio99 dynamic writer's copy functions take no capacity argument. Callers
must provide sufficient output storage, and `copy_bytes` returns `void`.

json99 retains its own error values. Reader EOF maps to `JSON_ERROR_EOF`.
Other reader failures and all byte writer failures map to `JSON_ERROR_IO`.
I/O implementations can expose their own detailed errors. `JsonNumberWriter`,
`JsonUnicodeWriter`, and `JsonUtf8Writer` remain JSON-specific interfaces.
`JsonUtf8Writer` delegates conversion to utf99, maps invalid input to
`JSON_ERROR_INVALID`, and retains its latched-error behavior.
