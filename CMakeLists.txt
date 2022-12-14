cmake_minimum_required(VERSION 3.5)

if(DEFINED ENV{EXTRA_COMPONENT_DIRS})
  # Turn space-separated, quote-aware environment var into CMake list
  separate_arguments(
    EXTRA_COMPONENT_DIRS UNIX_COMMAND "$ENV{EXTRA_COMPONENT_DIRS}")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nodemcu)

# Useful when tidying up code to catch all the warnings.
#idf_build_set_property(COMPILE_OPTIONS "-Werror" "-Werror=deprecated-declarations" APPEND)
