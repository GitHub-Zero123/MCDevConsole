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

# Release 模式下生成内联前端资源
find_package(Python3 COMPONENTS Interpreter)
find_program(NPM_EXECUTABLE npm)

if(Python3_FOUND AND NPM_EXECUTABLE)
    set(WEB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/client/web")
    set(WEB_DIST_DIR "${WEB_DIR}/dist")
    set(WEB_GENERATE_SCRIPT "${WEB_DIR}/generate.py")
    set(WEB_RESOURCE_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/client/cpp/include/MCDevConsole/WebResource.hpp")
    set(WEB_PACKAGE_JSON "${WEB_DIR}/package.json")
    
    # 生成内联资源（依赖 package.json 和脚本，确保前端先构建）
    add_custom_command(
        OUTPUT ${WEB_RESOURCE_HEADER}
        COMMAND ${NPM_EXECUTABLE} run build
        COMMAND ${Python3_EXECUTABLE} ${WEB_GENERATE_SCRIPT}
        WORKING_DIRECTORY ${WEB_DIR}
        DEPENDS ${WEB_GENERATE_SCRIPT} ${WEB_PACKAGE_JSON}
        COMMENT "构建前端并生成内联资源..."
    )
    
    add_custom_target(GenerateWebResource ALL DEPENDS ${WEB_RESOURCE_HEADER})
else()
    if(NOT Python3_FOUND)
        message(WARNING "未找到 Python3，跳过前端资源内联生成")
    endif()
    if(NOT NPM_EXECUTABLE)
        message(WARNING "未找到 npm，跳过前端资源内联生成")
    endif()
endif()

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

# Release 模式下添加依赖，确保先生成资源
if(TARGET GenerateWebResource)
    add_dependencies(MCDevConsole GenerateWebResource)
    target_compile_definitions(MCDevConsole PRIVATE MCDEVCONSOLE_EMBED_WEB_RESOURCE)
endif()