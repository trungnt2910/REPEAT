# Custom wrappers to ensure all target executables and libraries consistently use the project
# headers, sanitizers, and warning flags.

function(_repeat_target_add_warning_flags target)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE -Wall -Werror -Wpedantic)
    endif()
endfunction()

function(repeat_add_executable target)
    add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    # ASAN on some platforms does not support static linking and will error out if we do.
    if(NOT REPEAT_SANITIZER_PREVENT_STATIC_LINKING)
        target_link_options(${target} PRIVATE -static)
    endif()
    _repeat_target_add_warning_flags(${target})
    repeat_target_mingw_pdb(${target})
    repeat_target_add_sanitizers(${target})
    repeat_install_sanitizers(${target})
endfunction()

function(repeat_add_library target type)
    add_library(${target} ${type} ${ARGN})
    target_include_directories(${target} PUBLIC ${CMAKE_SOURCE_DIR}/include)
    _repeat_target_add_warning_flags(${target})
    repeat_target_add_sanitizers(${target})
endfunction()
