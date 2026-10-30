cmake_minimum_required(VERSION 3.22)

project(json99
  VERSION 0.1.0
  LANGUAGES C
)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(JSON99_BUILD_TESTING "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(JSON99_WITH_HEAP "Build allocating string getters" ON)
option(JSON99_USE_SYSTEM_DEPENDENCIES "Find installed sio99 and utf99" OFF)

if(NOT TARGET sio99)
  if(JSON99_USE_SYSTEM_DEPENDENCIES)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SIO99 REQUIRED IMPORTED_TARGET GLOBAL sio99)
    add_library(sio99 ALIAS PkgConfig::SIO99)
    foreach(adapter heap stdio)
      find_library(JSON99_SIO99_${adapter}_LIBRARY
        NAMES sio99_${adapter}
        HINTS ${SIO99_LIBRARY_DIRS}
      )
      if(JSON99_SIO99_${adapter}_LIBRARY)
        add_library(sio99_${adapter} UNKNOWN IMPORTED GLOBAL)
        set_target_properties(sio99_${adapter} PROPERTIES
          IMPORTED_LOCATION "${JSON99_SIO99_${adapter}_LIBRARY}"
          INTERFACE_LINK_LIBRARIES sio99
        )
      endif()
    endforeach()
  else()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/sio99/CMakeLists.txt")
      message(FATAL_ERROR
        "The sio99 submodule is missing. Run: "
        "git submodule update --init --recursive"
      )
    endif()
    function(json_add_sio99)
      if(NOT DEFINED SIO99_BUILD_TESTING)
        set(SIO99_BUILD_TESTING OFF)
      endif()
      if(NOT DEFINED SIO99_WITH_HEAP)
        set(SIO99_WITH_HEAP ${JSON99_WITH_HEAP})
      endif()
      add_subdirectory(src/sio99)
    endfunction()
    json_add_sio99()
  endif()
endif()

if(NOT TARGET utf99)
  if(JSON99_USE_SYSTEM_DEPENDENCIES)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(UTF99 REQUIRED IMPORTED_TARGET GLOBAL utf99)
    add_library(utf99 ALIAS PkgConfig::UTF99)
  else()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/utf99/CMakeLists.txt")
      message(FATAL_ERROR
        "The utf99 submodule is missing. Run: "
        "git submodule update --init --recursive"
      )
    endif()
    function(json_add_utf99)
      if(NOT DEFINED UTF99_BUILD_TESTING)
        set(UTF99_BUILD_TESTING OFF)
      endif()
      add_subdirectory(src/utf99)
    endfunction()
    json_add_utf99()
  endif()
endif()

if(JSON99_WITH_HEAP AND NOT TARGET sio99_heap)
  message(FATAL_ERROR
    "JSON99_WITH_HEAP requires sio99_heap. Enable SIO99_WITH_HEAP, "
    "install the sio99 heap adapter, or disable JSON99_WITH_HEAP."
  )
endif()

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
  src/json99/json.c
  src/json99/json_number_writer.c
  src/json99/json_utf8_writer.c
)
json_target_defaults(json99)
target_link_libraries(json99 PUBLIC sio99 utf99)
set_target_properties(json99
  PROPERTIES
    VERSION "${PROJECT_VERSION}"
    SOVERSION 0
)

if(JSON99_WITH_HEAP)
  add_library(json99_heap
    src/json99/json_heap.c
  )
  json_target_defaults(json99_heap)
  target_link_libraries(json99_heap
    PUBLIC
      json99
      sio99_heap
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
    include/json99/json_unicode_writer.h
    include/json99/json_utf8_writer.h
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/json99"
)

if(JSON99_WITH_HEAP)
  install(
    TARGETS json99_heap
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )
endif()

set(JSON99_PKGCONFIG_LIBS "")
if(JSON99_WITH_HEAP)
  string(APPEND JSON99_PKGCONFIG_LIBS "-ljson99_heap ")
endif()
string(APPEND JSON99_PKGCONFIG_LIBS "-ljson99")

configure_file(json99.pc.in json99.pc @ONLY)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/json99.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

if(JSON99_BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

