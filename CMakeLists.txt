cmake_minimum_required(VERSION 3.28)
project(repeat CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include these before third-party to get CodeView and Sanitizer support for every object.
# These files export a REPEAT_THIRD_PARTY_COMPILE_FLAGS which will be read by ThirdParty.cmake.
include(cmake/Architecture.cmake)
include(cmake/PDB.cmake)
include(cmake/Sanitizers.cmake)

include(cmake/ThirdParty.cmake)

include(cmake/Compiler.cmake)
include(cmake/Targets.cmake)
include(cmake/Tests.cmake)

add_subdirectory(src)

add_subdirectory(tests)
