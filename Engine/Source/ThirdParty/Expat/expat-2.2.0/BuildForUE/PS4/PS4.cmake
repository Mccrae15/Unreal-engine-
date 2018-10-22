include(../../CMake/PlatformScripts/PS4/PS4.cmake)

# Put our build output one level up so we can easily delete the temporary files and only check-in the final libs
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")

set(SKIP_PRE_BUILD_COMMAND 1)

#set(CMAKE_CXX_FLAGS "-Zi -wd4005 -wd4311" CACHE TYPE INTERNAL FORCE)
#set(CMAKE_C_FLAGS "-Zi -wd4005 -wd4311" CACHE TYPE INTERNAL FORCE)

# Hard code the config since CMake cannot figure out our toolchain
set(HARDCODED_CONFIG 1)

set(HAVE_DLFCN_H 0)
set(HAVE_FCNTL_H 1)
set(HAVE_INTTYPES_H 1)
set(HAVE_MEMORY_H 1)
set(HAVE_STDINT_H 1)
set(HAVE_STDLIB_H 1)
set(HAVE_STRINGS_H 0)
set(HAVE_STRING_H 1)
set(HAVE_SYS_STAT_H 1)
set(HAVE_SYS_TYPES_H 1)
set(HAVE_UNISTD_H 0)

set(HAVE_GETPAGESIZE 0)
set(HAVE_BCOPY 0)
set(HAVE_MEMMOVE 1)
set(HAVE_MMAP 0)

set(STDC_HEADERS 1)

set(BYTEORDER 1234)

set(OFF_T 0)
set(SIZE_T 0)

configure_file(expat_config.h.cmake expat_config.h)
add_definitions(-DHAVE_EXPAT_CONFIG_H)

