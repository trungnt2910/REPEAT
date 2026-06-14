# CMake does not recognize PDB generator properties for clang in MinGW mode.
function(_repeat_get_target_pdb_name name pdb_name)
    set(${pdb_name} "$<TARGET_FILE_DIR:${name}>/$<TARGET_FILE_BASE_NAME:${name}>.pdb" PARENT_SCOPE)
endfunction()

function(repeat_target_pdb name)
    if(WIN32)
        _repeat_get_target_pdb_name(${name} PDB_NAME)

        # Enable PDBs for use with VS Code Debugger.
        target_compile_options(${name} PRIVATE -gcodeview)

        set_property(TARGET ${name} APPEND PROPERTY
            ADDITIONAL_CLEAN_FILES ${PDB_NAME}
        )
    endif()
endfunction()

function(repeat_target_mingw_pdb name)
    if(WIN32)
        _repeat_get_target_pdb_name(${name} PDB_NAME)

        target_link_options(${name} PRIVATE -Wl,--pdb=${PDB_NAME})
        # Strip out any stray DWARF without affecting CodeView generation.
        # This forces tools (e.g. sanitizers) to use the more complete CodeView info,
        # allowing a full stack trace and other debugging features.
        target_link_options(${name} PRIVATE -Wl,-s)

        repeat_target_pdb(${name})
    endif()
endfunction()

if(WIN32)
    set(REPEAT_THIRD_PARTY_COMPILE_FLAGS "${REPEAT_THIRD_PARTY_COMPILE_FLAGS} -gcodeview")
endif()
