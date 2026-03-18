include(FetchContent)

set(REPEAT_ORIGINAL_C_FLAGS "${CMAKE_C_FLAGS}")
set(REPEAT_ORIGINAL_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${REPEAT_THIRD_PARTY_COMPILE_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${REPEAT_THIRD_PARTY_COMPILE_FLAGS}")

# ========================================================
# GoogleTest
# ========================================================
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# ========================================================
# LLVM
# ========================================================

# Split URL to satisfy the 100-column formatting limit
set(REPEAT_LLVM_URL_BASE "https://github.com/llvm/llvm-project/releases/download")
set(REPEAT_LLVM_URL_FILE "/llvmorg-22.1.7/llvm-project-22.1.7.src.tar.xz")
string(CONCAT REPEAT_LLVM_URL "${REPEAT_LLVM_URL_BASE}" "${REPEAT_LLVM_URL_FILE}")

FetchContent_Declare(
    llvm_minimal
    URL "${REPEAT_LLVM_URL}"
    URL_HASH SHA256=5cc4a3f12bba50b6bdfb4b61bdc852117a0ff2517807c3902fc13267fb93562e
    SOURCE_SUBDIR llvm
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    EXCLUDE_FROM_ALL
)

# Force LLVM to build with RTTI enabled so it can link with our project
set(LLVM_ENABLE_RTTI ON CACHE BOOL "" FORCE)

# Avoid dynamic dependencies
set(LLVM_USE_STATIC_ZSTD ON CACHE BOOL "" FORCE)
set(ZLIB_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(llvm_minimal)

# Carry LLVM includes and compile definitions to targets without
# polluting global include_directories and definitions.
add_library(repeat_llvm INTERFACE)
target_include_directories(repeat_llvm INTERFACE
    ${llvm_minimal_SOURCE_DIR}/llvm/include
    ${llvm_minimal_BINARY_DIR}/include
)
target_compile_definitions(repeat_llvm INTERFACE ${LLVM_DEFINITIONS})

set(CMAKE_C_FLAGS "${REPEAT_ORIGINAL_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${REPEAT_ORIGINAL_CXX_FLAGS}")
unset(REPEAT_ORIGINAL_C_FLAGS)
unset(REPEAT_ORIGINAL_CXX_FLAGS)
