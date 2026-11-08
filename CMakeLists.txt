cmake_minimum_required(VERSION 3.20)
project(SpektraLab LANGUAGES C CXX)

if(MSVC)
    # Sources and wire strings are UTF-8, independent of the Windows code page.
    add_compile_options(/utf-8)
endif()

include(CTest)
option(SPEKTRALAB_BUILD_QT_UI "Build the native Qt Quick Windows application" ${WIN32})
if(SPEKTRALAB_BUILD_QT_UI)
    set(SPEKTRALAB_BUILD_NATIVE_RAW ON CACHE BOOL "" FORCE)
    set(SPEKTRALAB_BUILD_DESKTOP ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(engine)
if(SPEKTRALAB_BUILD_QT_UI)
    add_subdirectory(windows_UI)
endif()
