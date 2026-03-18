repeat_add_library(repeat_arch_aarch64 STATIC
    aarch64_backend.cpp
)

target_link_libraries(repeat_arch_aarch64 PRIVATE repeat_llvm)
