set(REPEAT_ASAN_ARCH ${REPEAT_ARCH})
# For some weird reasons, i686 MinGW provides libclang_rt.asan_dynamic-i386.dll.
# Note the i386, not i686.
if(REPEAT_ASAN_ARCH MATCHES "i686")
    set(REPEAT_ASAN_ARCH "i386")
endif()

set(REPEAT_ASAN_ARCH_SUPPORTED TRUE)

# Disable ASAN for ARM-based targets since our LLVM builds
# do not provide the required runtime libraries.
if(REPEAT_ASAN_ARCH MATCHES "armv7|aarch64")
    set(REPEAT_ASAN_ARCH_SUPPORTED FALSE)
endif()

# Enabling sanitizers forces the executable to be dynamically linked to these.
set(REPEAT_SANITIZER_DLLS "")
if(MINGW AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND REPEAT_ASAN_ARCH_SUPPORTED)
    list(APPEND REPEAT_SANITIZER_DLLS
        "libc++.dll"
        "libclang_rt.asan_dynamic-${REPEAT_ASAN_ARCH}.dll"
        "libunwind.dll"
    )
endif()

set(REPEAT_SANITIZER_PREVENT_STATIC_LINKING FALSE)
set(REPEAT_SANITIZER_FLAGS "")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(REPEAT_ASAN_ARCH_SUPPORTED)
        set(REPEAT_SANITIZER_FLAGS "${REPEAT_SANITIZER_FLAGS} -fsanitize=address")
        # On most ELF platforms, ASAN does not support static linking and will error out if we do.
        if(NOT WIN32)
            set(REPEAT_SANITIZER_PREVENT_STATIC_LINKING TRUE)
        endif()
    endif()

    set(REPEAT_SANITIZER_FLAGS "${REPEAT_SANITIZER_FLAGS} -fsanitize=undefined")
endif()

function(repeat_target_add_sanitizers name)
    separate_arguments(FLAGS_LIST NATIVE_COMMAND "${REPEAT_SANITIZER_FLAGS}")

    target_compile_options(${name} PRIVATE "${FLAGS_LIST}")
    target_link_options(${name} PRIVATE "${FLAGS_LIST}")
endfunction()

function(repeat_install_sanitizers name)
    # For MinGW targets, copy the DLLs to the same directory as the executable to avoid weird
    # missing DLL errors.
    # UNIX targets should already have them in the library path.
    foreach(DLL_NAME ${REPEAT_SANITIZER_DLLS})
        # Ask clang where the libraries are.
        # It expects a path relative to the toolchain root.
        # We must use the ${ARCH}-w64-mingw32 folder;
        # the top-level bin/ is specific to the host architecture.
        # The actual .dlls are in bin/; lib/ contains the glue libraries.
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER}
                    --print-file-name=${REPEAT_ARCH}-w64-mingw32/bin/${DLL_NAME}
            OUTPUT_VARIABLE DLL_PATH_${DLL_NAME}
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if(DLL_PATH_${DLL_NAME})
            add_custom_command(TARGET ${name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL_PATH_${DLL_NAME}}"
                    "$<TARGET_FILE_DIR:${name}>/${DLL_NAME}"
            )

            set_property(TARGET ${name} APPEND PROPERTY
                ADDITIONAL_CLEAN_FILES "$<TARGET_FILE_DIR:${name}>/${DLL_NAME}"
            )

            install(FILES "${DLL_PATH_${DLL_NAME}}" DESTINATION bin/${REPEAT_INSTALL_ARCH})
        endif()
    endforeach()
endfunction()

# Ensure that third-party dependencies are also built with the same sanitizers.
# Otherwise, we may get false positives with standard library containers.
# See: https://github.com/google/sanitizers/wiki/AddressSanitizerContainerOverflow#false-positives
set(REPEAT_THIRD_PARTY_COMPILE_FLAGS
    "${REPEAT_THIRD_PARTY_COMPILE_FLAGS} ${REPEAT_SANITIZER_FLAGS}"
)
