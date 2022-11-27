CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)

if(DEFINED PROJECT_NAME)
    set(KCP_IS_TOP_PROJECT OFF)
else()
    set(KCP_IS_TOP_PROJECT ON)
endif()

project(kcp LANGUAGES C)

cmake_policy(SET CMP0054 NEW)

option(KCP_BUILD_SHARED_LIBS "Build KCP as a shared lib" OFF)
option(KCP_BUILD_INSTALL "Build install target" ${KCP_IS_TOP_PROJECT})
option(KCP_BUILD_TESTS "Build tests" ${KCP_IS_TOP_PROJECT})

if(KCP_BUILD_SHARED_LIBS)
    if(WIN32)
        set(exports_def_file "${CMAKE_CURRENT_BINARY_DIR}/exports.def")
        set(exports_def_contents
"EXPORTS 
    ikcp_create
    ikcp_release
    ikcp_setoutput
    ikcp_recv
    ikcp_send
    ikcp_update
    ikcp_check
    ikcp_input
    ikcp_flush
    ikcp_peeksize
    ikcp_setmtu
    ikcp_wndsize
    ikcp_waitsnd
    ikcp_nodelay
    ikcp_log
    ikcp_allocator
    ikcp_getconv
")

        file(WRITE "${exports_def_file}" "${exports_def_contents}")
        add_library(kcp SHARED ikcp.h ikcp.c "${exports_def_file}")
    else()
        add_library(kcp SHARED ikcp.h ikcp.c)
    endif()
else()
    add_library(kcp STATIC ikcp.h ikcp.c)
endif()

if(KCP_BUILD_INSTALL)
    include(GNUInstallDirs)

    set_target_properties(kcp PROPERTIES PUBLIC_HEADER ikcp.h)

    install(TARGETS kcp
        EXPORT kcp-targets
        PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT kcp-targets
        FILE kcp-config.cmake
        NAMESPACE kcp::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kcp
    )
endif()

if(KCP_BUILD_TESTS)
    enable_language(CXX)
    
    add_executable(kcp_test test.cpp)
    if(MSVC AND NOT (MSVC_VERSION LESS 1900))
        target_compile_options(kcp_test PRIVATE /utf-8)
    endif()
endif()
