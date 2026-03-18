# Mock ELF binaries across x86_64, i386, arm, and aarch64 are needed as test inputs for the suite.
# Accumulates generated mock ELF paths for dependency tracking in tests.
function(repeat_add_mock_elf FILENAME)
    cmake_parse_arguments(ARG
        "CXX"
        "SRC;ARCH;OPT"
        ""
        ${ARGN}
    )

    set(PREBUILT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/data/elf/${FILENAME}")
    set(OUT_FILE "${REPEAT_MOCK_DATA_DIR}/${FILENAME}")

    if(EXISTS "${PREBUILT_FILE}")
        add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy "${PREBUILT_FILE}" "${OUT_FILE}"
            DEPENDS "${PREBUILT_FILE}"
            COMMENT "Copying prebuilt mock ELF ${FILENAME}"
            VERBATIM
        )
    else()
        if(ARG_CXX)
            set(COMPILER "${REPEAT_CLANGXX}")
        else()
            set(COMPILER "${REPEAT_CLANG}")
        endif()

        if(NOT ARG_ARCH)
            set(ARG_ARCH "x86_64-pc-linux-gnu")
        endif()

        set(FLAGS -shared -fPIC -target ${ARG_ARCH} -fuse-ld=lld -ffreestanding -nostdlib -g
                  "-fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=."
                  "-fdebug-compilation-dir=.")

        if(ARG_OPT)
            list(APPEND FLAGS ${ARG_OPT})
        endif()

        add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${COMPILER} ${FLAGS} -o ${OUT_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/${ARG_SRC}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${ARG_SRC}
            COMMENT "Compiling ${ARG_ARCH} mock ELF ${FILENAME}"
        )
    endif()

    set(REPEAT_MOCK_ELF_FILES ${REPEAT_MOCK_ELF_FILES} ${OUT_FILE} PARENT_SCOPE)
endfunction()
