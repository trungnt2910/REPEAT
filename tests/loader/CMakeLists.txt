if(WIN32)
    set(REPEAT_LOADER_SOURCES win32/pe_loader.cpp)
    set(REPEAT_LOADER_INCLUDE_DIR win32)
else()
    set(REPEAT_LOADER_SOURCES posix/pe_loader.cpp)
    set(REPEAT_LOADER_INCLUDE_DIR posix)
endif()

repeat_add_library(repeat_pe_loader STATIC
    ${REPEAT_LOADER_SOURCES}
)
target_include_directories(repeat_pe_loader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/${REPEAT_LOADER_INCLUDE_DIR}
)
