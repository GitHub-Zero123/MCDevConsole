cmake_minimum_required(VERSION 3.15)

project(MCDevConsole LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    if(MSVC)
        add_compile_options(/utf-8 /EHsc)
    endif()
endif()

set(MCDEVCONSOLE_ROOT_NAMESPACE MCDevConsole)
set(MCDEVCONSOLE_WEBVIEW2_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/include")
set(MCDEVCONSOLE_WEBVIEW2_LOADER_LIB
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/_downloads/webview2_pkg/build/native/x64/WebView2LoaderStatic.lib")

add_executable(MCDevConsole WIN32
    src/client/cpp/src/main.cpp
    src/client/cpp/src/AppWindow.cpp
    src/client/cpp/src/Protocol.cpp
    src/client/cpp/src/WebViewHost.cpp
    src/client/cpp/src/NetworkServer.cpp
)

target_compile_features(MCDevConsole PRIVATE cxx_std_20)

target_include_directories(MCDevConsole
    PRIVATE
        ${MCDEVCONSOLE_WEBVIEW2_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/cpp/include
)

if(MSVC)
    set_property(TARGET MCDevConsole PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    target_compile_definitions(MCDevConsole
        PRIVATE
            UNICODE
            _UNICODE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            MCDEVCONSOLE_ROOT_NAMESPACE=${MCDEVCONSOLE_ROOT_NAMESPACE}
    )
endif()

target_link_libraries(MCDevConsole
    PRIVATE
        user32
        gdi32
        ole32
        oleaut32
        ws2_32
        ${MCDEVCONSOLE_WEBVIEW2_LOADER_LIB}
)