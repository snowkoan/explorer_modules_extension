cmake_minimum_required(VERSION 3.22)
project(ExplorerModulesNamespace LANGUAGES CXX RC)

# This has to be set before the first call to add_library() (!)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(ExplorerModulesNamespace SHARED
    src/ExplorerModulesNamespace.def
    src/dllmain.cpp
    src/ClassFactory.cpp
    src/DllNotification.cpp
    src/IidNames.cpp
    src/Log.cpp
    src/ModuleFolder.cpp
    src/ModuleHelpers.cpp
    src/Pidl.cpp
    src/EnumIDList.cpp
    resources/namespace.rc
)

target_include_directories(ExplorerModulesNamespace PRIVATE src)

target_compile_features(ExplorerModulesNamespace PRIVATE cxx_std_20)

target_compile_definitions(ExplorerModulesNamespace PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if (MSVC)
    target_compile_options(ExplorerModulesNamespace PRIVATE
        /W4 /WX /sdl /wd4324 /Zi
    )
    target_link_options(ExplorerModulesNamespace PRIVATE
        /DEBUG
    )
endif()

target_link_libraries(ExplorerModulesNamespace PRIVATE
    Shlwapi
    Psapi
    Ole32
    Shell32
    RuntimeObject
    Version
)
