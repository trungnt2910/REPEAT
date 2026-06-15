cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/ELF.cmake)

add_subdirectory(loader)
add_subdirectory(lib)

set(REPEAT_MOCK_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/data")
file(MAKE_DIRECTORY "${REPEAT_MOCK_DATA_DIR}")

set(REPEAT_GOLDEN_CLI_FILES
    cv_unsupported_loc_stderr.txt
    help_stdout.txt
    invalid_elf_stderr.txt
    missing_input_stderr.txt
    unknown_option_stderr.txt
)
foreach(cli_file ${REPEAT_GOLDEN_CLI_FILES})
    configure_file(
        data/output/cli/${cli_file}
        data/output/cli/${cli_file}
        COPYONLY
    )
endforeach()

set(REPEAT_GOLDEN_ASM_FILES
    cv_advanced_types.s
    cv_anon_namespaces.s
    cv_arrays.s
    cv_enums.s
    cv_float.s
    cv_func_ptr.s
    cv_huge_range.s
    cv_inheritance.s
    cv_inlined_vars.s
    cv_lines.s
    cv_local_functions.s
    cv_local_variables.s
    cv_methods.s
    cv_moving_var.s
    cv_namespaces.s
    cv_nested_inlines.s
    cv_recursive.s
    cv_scope_end.s
    cv_spill.s
    cv_static_methods.s
    cv_structs.s
    cv_subprogram.s
    cv_templates.s
    cv_unions.s
    cv_unsupported_loc.s
    cv_vars.s
    relocs_elf_aarch64.s
    relocs_elf_arm.s
    seh_leaf.s
    seh_normal.s
    valid_elf.s
)
foreach(golden_file ${REPEAT_GOLDEN_ASM_FILES})
    configure_file(
        data/output/asm/${golden_file}
        data/output/asm/${golden_file}
        COPYONLY
    )
endforeach()

configure_file(data/elf/invalid_elf.txt data/invalid_elf.txt COPYONLY)
configure_file(data/elf/corrupt_eh_frame.so data/corrupt_eh_frame.so COPYONLY)

set(REPEAT_MOCK_ELF_FILES "")

repeat_add_mock_elf(bss_elf.so SRC data/bss_elf.c)
repeat_add_mock_elf(relocs_elf.so SRC data/relocs_elf.c)
repeat_add_mock_elf(symbols_elf.so SRC data/symbols_elf.c)
repeat_add_mock_elf(valid_elf.so SRC data/valid_elf.c)

repeat_add_mock_elf(cv_advanced_types.so SRC data/cv_advanced_types.cpp CXX OPT -O0)
repeat_add_mock_elf(cv_anon_namespaces.so SRC data/cv_anon_namespaces.cpp CXX)
repeat_add_mock_elf(cv_arrays.so SRC data/cv_arrays.c)
repeat_add_mock_elf(cv_enums.so SRC data/cv_enums.c)
repeat_add_mock_elf(cv_float.so SRC data/cv_float.c OPT -O2)
repeat_add_mock_elf(cv_func_ptr.so SRC data/cv_func_ptr.c)
repeat_add_mock_elf(cv_huge_range.so SRC data/cv_huge_range.c OPT -O2)
repeat_add_mock_elf(cv_inheritance.so SRC data/cv_inheritance.cpp CXX)
repeat_add_mock_elf(cv_inlined_vars.so SRC data/cv_inlined_vars.c)
repeat_add_mock_elf(cv_lines.so SRC data/cv_lines.c)
repeat_add_mock_elf(cv_local_functions.so SRC data/cv_local_functions.c)
repeat_add_mock_elf(cv_local_variables.so SRC data/cv_local_variables.c)
repeat_add_mock_elf(cv_methods.so SRC data/cv_methods.cpp CXX)
repeat_add_mock_elf(cv_moving_var.so SRC data/cv_moving_var.c OPT -O2)
repeat_add_mock_elf(cv_namespaces.so SRC data/cv_namespaces.cpp CXX)
repeat_add_mock_elf(cv_nested_inlines.so SRC data/cv_nested_inlines.cpp CXX OPT -O0)
repeat_add_mock_elf(cv_recursive.so SRC data/cv_recursive.c)
repeat_add_mock_elf(cv_scope_end.so SRC data/cv_scope_end.c)
repeat_add_mock_elf(cv_spill.so SRC data/cv_spill.c OPT -O2)
repeat_add_mock_elf(cv_static_methods.so SRC data/cv_static_methods.cpp CXX)
repeat_add_mock_elf(cv_structs.so SRC data/cv_structs.c)
repeat_add_mock_elf(cv_subprogram.so SRC data/cv_subprogram.c)
repeat_add_mock_elf(cv_templates.so SRC data/cv_templates.cpp CXX)
repeat_add_mock_elf(cv_unions.so SRC data/cv_unions.c)
repeat_add_mock_elf(cv_unsupported_loc.so SRC data/cv_unsupported_loc.c OPT -O2)
repeat_add_mock_elf(cv_vars.so SRC data/cv_vars.c)

repeat_add_mock_elf(seh_leaf.so SRC data/seh_leaf.c OPT -O2)
repeat_add_mock_elf(seh_normal.so SRC data/seh_normal.c)

repeat_add_mock_elf(valid_elf_i386.so SRC data/valid_elf.c ARCH i386-pc-linux-gnu)
repeat_add_mock_elf(relocs_elf_i386.so SRC data/relocs_elf.c ARCH i386-pc-linux-gnu)

repeat_add_mock_elf(valid_elf_aarch64.so SRC data/valid_elf.c ARCH aarch64-linux-gnu)
repeat_add_mock_elf(relocs_elf_aarch64.so SRC data/relocs_elf.c ARCH aarch64-linux-gnu)

repeat_add_mock_elf(valid_elf_arm.so SRC data/valid_elf.c ARCH arm-linux-gnueabi)
repeat_add_mock_elf(relocs_elf_arm.so SRC data/relocs_elf.c ARCH arm-linux-gnueabi)

add_custom_target(repeat_build_unit_mock_data DEPENDS ${REPEAT_MOCK_ELF_FILES})

repeat_add_test_executable(repeat_tests
    Given_Arguments.cpp
    Given_DebugInfo.cpp
    Given_Loading.cpp
    Given_MemoryLayout.cpp
    Given_Relocations.cpp
    Given_Repeat.cpp
    Given_Symbols.cpp
    Given_Unwind.cpp
    Given_X86_64.cpp
    test_main.cpp
)

target_include_directories(repeat_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(repeat_tests PRIVATE
    repeat_lib
    repeat_pe_loader
)

add_dependencies(repeat_tests
    repeat_build_unit_mock_data
    repeat_build_test_image
)
