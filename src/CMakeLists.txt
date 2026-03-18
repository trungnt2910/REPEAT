add_subdirectory(arch)

repeat_add_library(repeat_lib STATIC
    args.cpp
    debug/debug_helper.cpp
    debug/line_translator.cpp
    debug/type_translator.cpp
    debug/unwind_translator.cpp
    debug/var_translator.cpp
    repeat.cpp
    translator.cpp
)

llvm_map_components_to_libnames(REPEAT_LLVM_LIBS
    Object DebugInfoDWARF DebugInfoCodeView Support TargetParser
)
target_link_libraries(repeat_lib PUBLIC
    repeat_backend
    ${REPEAT_LLVM_LIBS}
)
target_link_libraries(repeat_lib PRIVATE
    repeat_llvm
)

repeat_add_executable(repeat
    main.cpp
)

target_link_libraries(repeat PRIVATE repeat_lib)
