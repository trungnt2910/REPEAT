repeat_add_library(repeat_arch_x86_64 STATIC
    x86_64_backend.cpp
)

target_link_libraries(repeat_arch_x86_64 PRIVATE repeat_llvm)
