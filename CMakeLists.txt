cmake_minimum_required(VERSION 3.22)

project(json99
  VERSION 0.1.0
  LANGUAGES C
)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(JSON99_BUILD_TESTING "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(JSON99_WITH_STDIO "Build the file reader" ON)
option(JSON99_WITH_HEAP "Build the dynamic buffer writer" ON)

function(json_target_defaults target)

  set_target_properties(${target}
    PROPERTIES
      C_STANDARD 99
      C_STANDARD_REQUIRED ON
      C_EXTENSIONS OFF
  )

  target_include_directories(${target} PUBLIC include)

  target_compile_options(${target}
    PRIVATE
      "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:"
        "-Werror;"
        "-Wpedantic;"
        "-Wall;"
        "-Wextra;"
        "-Werror=vla;"
        "-Werror=alloca;"
        "-Werror=implicit-function-declaration;"
        "-Wstrict-prototypes;"
        "-Wmissing-prototypes;"
        "-fno-fast-math;"
        "-ffp-contract=off;"
      ">"
  )

endfunction()

add_library(json99
  src/json.c
  src/json_number_writer.c
  src/json_buffer_reader.c
  src/json_buffer_writer.c
)
json_target_defaults(json99)
set_target_properties(json99
  PROPERTIES
    VERSION "${PROJECT_VERSION}"
    SOVERSION 0
)

if(JSON99_WITH_STDIO)
  add_library(json99_stdio
    src/json_file_reader.c
  )
  json_target_defaults(json99_stdio)
  set_target_properties(json99_stdio
    PROPERTIES
      VERSION "${PROJECT_VERSION}"
      SOVERSION 0
  )
endif()

if(JSON99_WITH_HEAP)
  add_library(json99_heap
    src/json_heap.c
    src/json_dynamic_buffer_writer.c
  )
  json_target_defaults(json99_heap)
  target_link_libraries(json99_heap
    PUBLIC
      json99
  )
  set_target_properties(json99_heap
    PROPERTIES
      VERSION "${PROJECT_VERSION}"
      SOVERSION 0
  )
endif()

include(GNUInstallDirs)

install(
  TARGETS json99
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(
  FILES
    include/json99/json.h
    include/json99/json_number_writer.h
    include/json99/json_buffer_reader.h
    include/json99/json_buffer_writer.h
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/json99"
)

if(JSON99_WITH_STDIO)
  install(
    TARGETS json99_stdio
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )
  install(
    FILES include/json99/json_file_reader.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/json99"
  )
endif()

if(JSON99_WITH_HEAP)
  install(
    TARGETS json99_heap
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )
  install(
    FILES include/json99/json_dynamic_buffer_writer.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/json99"
  )
endif()

set(JSON99_PKGCONFIG_LIBS "")
if(JSON99_WITH_STDIO)
  string(APPEND JSON99_PKGCONFIG_LIBS "-ljson99_stdio ")
endif()
if(JSON99_WITH_HEAP)
  string(APPEND JSON99_PKGCONFIG_LIBS "-ljson99_heap ")
endif()
string(APPEND JSON99_PKGCONFIG_LIBS "-ljson99")

configure_file(json99.pc.in json99.pc @ONLY)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/json99.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

include(CTest)
if(JSON99_BUILD_TESTING)
  add_subdirectory(tests)
endif()
