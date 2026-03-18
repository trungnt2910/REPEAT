set(REPEAT_TEST_LIB_C "${CMAKE_CURRENT_SOURCE_DIR}/test_lib.c")
set(REPEAT_TEST_LIB_SO "${CMAKE_CURRENT_BINARY_DIR}/test_lib.so")

set(REPEAT_TEST_LIB_S "${CMAKE_CURRENT_BINARY_DIR}/test_lib.s")
set(REPEAT_TEST_LIB_OBJ "${CMAKE_CURRENT_BINARY_DIR}/test_lib.obj")
set(REPEAT_PE_NATIVE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/pe_native.obj")
# Output DLL directly to the parent tests/ binary directory so the test executable can find it
set(REPEAT_TEST_IMAGE_DLL "${CMAKE_CURRENT_BINARY_DIR}/../test_image.dll")

add_custom_command(
    OUTPUT ${REPEAT_TEST_LIB_SO}
    COMMAND ${REPEAT_CLANG} -g -shared -fPIC -target x86_64-pc-linux-gnu
            -ffreestanding -fuse-ld=lld -nostdlib -o ${REPEAT_TEST_LIB_SO} ${REPEAT_TEST_LIB_C}
    DEPENDS ${REPEAT_TEST_LIB_C}
    COMMENT "Compiling test ELF library test_lib.so"
)

set(REPEAT_TOOL_EXE "" CACHE STRING
    "Path to host repeat translator executable (for cross-compiling)"
)

if(REPEAT_TOOL_EXE STREQUAL "")
    set(REPEAT_TOOL_COMMAND $<TARGET_FILE:repeat>)
    set(REPEAT_TOOL_DEP repeat)
else()
    set(REPEAT_TOOL_COMMAND ${REPEAT_TOOL_EXE})
    set(REPEAT_TOOL_DEP "")
endif()

add_custom_command(
    OUTPUT ${REPEAT_TEST_LIB_S}
    COMMAND ${REPEAT_TOOL_COMMAND} ${REPEAT_TEST_LIB_SO} -o ${REPEAT_TEST_LIB_S}
    DEPENDS ${REPEAT_TOOL_DEP} ${REPEAT_TEST_LIB_SO}
    COMMENT "Repackaging ELF library to Assembly test_lib.s"
)

add_custom_command(
    OUTPUT ${REPEAT_TEST_LIB_OBJ}
    COMMAND ${REPEAT_CLANG} -target x86_64-pc-windows-msvc -g -gcodeview -c
            ${REPEAT_TEST_LIB_S} -o ${REPEAT_TEST_LIB_OBJ}
    DEPENDS ${REPEAT_TEST_LIB_S}
    COMMENT "Assembling COFF object test_lib.obj"
)

add_custom_command(
    OUTPUT ${REPEAT_PE_NATIVE_OBJ}
    COMMAND ${REPEAT_CLANG} -target x86_64-pc-windows-msvc -c
            ${CMAKE_CURRENT_SOURCE_DIR}/pe_native.c -o ${REPEAT_PE_NATIVE_OBJ}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/pe_native.c
    COMMENT "Compiling PE-native object pe_native.obj"
)

add_custom_command(
    OUTPUT ${REPEAT_TEST_IMAGE_DLL}
    COMMAND ${REPEAT_CLANG} -target x86_64-pc-windows-msvc -fuse-ld=lld
            -shared -nostdlib -Wl,/noentry
            -Wl,/export:get_val
            -Wl,/export:get_global_ptr
            -Wl,/export:pe_call_elf
            -Wl,/export:elf_call_pe
            -o ${REPEAT_TEST_IMAGE_DLL}
            ${REPEAT_TEST_LIB_OBJ} ${REPEAT_PE_NATIVE_OBJ}
    DEPENDS ${REPEAT_TEST_LIB_OBJ} ${REPEAT_PE_NATIVE_OBJ}
    COMMENT "Linking PE DLL test_image.dll with cross-calls"
)

# Exposes the DLL generation as a target to allow unit tests to declare dependency on it.
add_custom_target(repeat_build_test_image DEPENDS ${REPEAT_TEST_IMAGE_DLL})
