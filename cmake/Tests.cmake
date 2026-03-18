enable_testing()

include(GoogleTest)

function(repeat_add_test_executable target)
    repeat_add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE gtest)
    add_test(NAME ${target} COMMAND ${target})
    gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST)
endfunction()
