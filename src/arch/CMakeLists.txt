add_subdirectory(x86_64)
add_subdirectory(aarch64)
add_subdirectory(i386)
add_subdirectory(arm)

repeat_add_library(repeat_backend STATIC
    backend.cpp
)

target_link_libraries(repeat_backend PUBLIC
    repeat_arch_x86_64
    repeat_arch_aarch64
    repeat_arch_i386
    repeat_arch_arm
    repeat_llvm
)
