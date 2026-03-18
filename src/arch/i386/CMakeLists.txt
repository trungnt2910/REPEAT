repeat_add_library(repeat_arch_i386 STATIC
    i386_backend.cpp
)

target_link_libraries(repeat_arch_i386 PRIVATE repeat_llvm)
