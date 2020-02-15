cmake_minimum_required(VERSION 2.8.6)

project(kcp LANGUAGES C)

include(CTest)
include(GenerateExportHeader)
include(GNUInstallDirs)

add_library(kcp ikcp.c)
generate_export_header(kcp
    BASE_NAME IKCP
    EXPORT_FILE_NAME ikcp_export.h
)
target_include_directories(kcp PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)

install(FILES ikcp.h ikcp_export.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS kcp
    EXPORT kcp-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT kcp-targets
    FILE kcp-config.cmake
    NAMESPACE kcp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kcp
)

if (BUILD_TESTING)
    enable_language(CXX)
    
    add_executable(kcp_test test.cpp)
    target_link_libraries(kcp_test kcp)
    if(MSVC AND NOT (MSVC_VERSION LESS 1900))
        target_compile_options(kcp_test PRIVATE /utf-8)
    endif()
endif ()
