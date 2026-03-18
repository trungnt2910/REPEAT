repeat_add_library(repeat_arch_arm STATIC
    arm_backend.cpp
)

target_link_libraries(repeat_arch_arm PRIVATE repeat_llvm)
