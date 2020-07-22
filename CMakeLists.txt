CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

project(kcp LANGUAGES C)

include(CTest)
include(GNUInstallDirs)

add_library(kcp STATIC ikcp.c)

install(FILES ikcp.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS kcp
    EXPORT kcp-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT kcp-targets
    FILE kcp-config.cmake
    NAMESPACE kcp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kcp
)

if (BUILD_TESTING)
    enable_language(CXX)
    
    add_executable(kcp_test test.cpp)
    if(MSVC AND NOT (MSVC_VERSION LESS 1900))
        target_compile_options(kcp_test PRIVATE /utf-8)
    endif()
endif ()
