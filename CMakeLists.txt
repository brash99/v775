cmake_minimum_required(VERSION 3.10)

project(v775)

add_library(v775
  SHARED
  caen775Lib.c
)

add_compile_options(-Wall -Wno-unused -g)

set(linuxvme_lib "$ENV{LINUXVME_LIB}")
set(linuxvme_inc "$ENV{LINUXVME_INC}")
set(linuxvme_bin "$ENV{LINUXVME_BIN}")

include_directories(
  ${PROJECT_SOURCE_DIR}
  ${linuxvme_inc}
)

install(TARGETS v775
  DESTINATION ${linuxvme_lib}
)

install(FILES c775Lib.h
  DESTINATION ${linuxvme_inc}
)
