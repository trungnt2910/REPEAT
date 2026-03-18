# Clang is required as a cross-compiler to generate mock ELF test inputs.
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(REPEAT_CLANG "${CMAKE_C_COMPILER}")
else()
    find_program(REPEAT_CLANG clang)
endif()

if(NOT REPEAT_CLANG)
    message(FATAL_ERROR "Could not find clang (required for building tests).")
endif()

# Clang++ is required to compile C++ based mock ELF test inputs.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(REPEAT_CLANGXX "${CMAKE_CXX_COMPILER}")
else()
    find_program(REPEAT_CLANGXX clang++)
endif()

if(NOT REPEAT_CLANGXX)
    message(FATAL_ERROR "Could not find clang++ (required for building tests).")
endif()
