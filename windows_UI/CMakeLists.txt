cmake_minimum_required(VERSION 3.22)

# Also usable as add_subdirectory(windows_UI) after the engine defines
# spk_desktop_host. The standalone mode never modifies the engine checkout.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(SpektraLabWindowsUI LANGUAGES C CXX)
    include(CTest)
    set(SPEKTRALAB_SOURCE_ROOT "" CACHE PATH "Source tree supplying engine/")
    if(NOT EXISTS "${SPEKTRALAB_SOURCE_ROOT}/engine/CMakeLists.txt")
        message(FATAL_ERROR "Set SPEKTRALAB_SOURCE_ROOT to the SpektraLab source tree")
    endif()
    set(SPEKTRALAB_BUILD_NATIVE_RAW ON CACHE BOOL "" FORCE)
    set(SPEKTRALAB_BUILD_DESKTOP ON CACHE BOOL "" FORCE)
    add_subdirectory("${SPEKTRALAB_SOURCE_ROOT}/engine" engine)
endif()

if(NOT TARGET spk_desktop_host)
    message(FATAL_ERROR "Build the engine's spk_desktop_host before windows_UI")
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)
add_library(spk_qt_post STATIC src/PostProcess.cpp)
target_compile_features(spk_qt_post PRIVATE cxx_std_20)
target_link_libraries(spk_qt_post PUBLIC spk_desktop_host Qt6::Core Qt6::Gui)

qt_add_executable(SpektraLabQt WIN32
    src/main.cpp src/PreviewController.hpp src/PreviewController.cpp
    src/FrameOutput.hpp src/FrameOutput.cpp
    src/EditStore.hpp src/EditStore.cpp
    src/FrameCanvas.hpp src/FrameCanvas.cpp)
target_compile_features(SpektraLabQt PRIVATE cxx_std_20)
target_link_libraries(SpektraLabQt PRIVATE
    spk_desktop_host Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set(spektralab_qml_files qml/Main.qml qml/Theme.qml qml/Section.qml qml/ActionButton.qml qml/RailHandle.qml qml/Histogram.qml
        qml/MaterialPanel.qml qml/StockList.qml qml/EditorToolbar.qml qml/ProcessPanel.qml qml/LatitudePlot.qml
        qml/RailSlider.qml qml/RailToggle.qml qml/RailMenu.qml qml/RailText.qml
        qml/PostPanel.qml qml/CurveEditor.qml qml/ColorWheel.qml qml/ColorBalanceEditor.qml qml/HistoryPanel.qml)
qt_add_qml_module(SpektraLabQt URI SpektraLab VERSION 1.0 QML_FILES ${spektralab_qml_files})
if(MINGW)
    target_compile_options(SpektraLabQt PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(SpektraLabQt PRIVATE -Wl,--gc-sections $<$<CONFIG:Release>:-s>)
endif()
set_target_properties(SpektraLabQt PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/app")

# The one resource directory is explicitly adjacent to the executable. No
# lookup through another checkout and no Python in the application.
add_custom_target(spektralab_qt_resources ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "$<TARGET_FILE_DIR:spk_raw_render>/resources" "$<TARGET_FILE_DIR:SpektraLabQt>/resources"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "$<TARGET_FILE_DIR:spk_raw_render>/licenses" "$<TARGET_FILE_DIR:SpektraLabQt>/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../modern_UI/Spektrafilm/Spektrafilm/Resources/FilmCovers" "$<TARGET_FILE_DIR:SpektraLabQt>/resources/film_covers"
    VERBATIM)
add_dependencies(spektralab_qt_resources spektralab_vulkan_resources)
add_dependencies(SpektraLabQt spektralab_qt_resources)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(spk_qt_history_ui_test tests/history_ui.cpp src/PreviewController.hpp src/PreviewController.cpp
        src/FrameOutput.cpp src/EditStore.cpp src/FrameCanvas.hpp src/FrameCanvas.cpp)
    target_include_directories(spk_qt_history_ui_test PRIVATE src)
    target_compile_features(spk_qt_history_ui_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_history_ui_test PRIVATE spk_qt_post Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    qt_add_qml_module(spk_qt_history_ui_test URI SpektraLab VERSION 1.0
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/history-test-module/SpektraLab" QML_FILES ${spektralab_qml_files})
    qt_add_executable(spk_qt_canvas_compare_test tests/canvas_compare.cpp src/FrameCanvas.hpp src/FrameCanvas.cpp)
    target_include_directories(spk_qt_canvas_compare_test PRIVATE src)
    target_compile_features(spk_qt_canvas_compare_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_canvas_compare_test PRIVATE Qt6::Core Qt6::Gui Qt6::Quick)
    add_test(NAME spk_qt_canvas_compare COMMAND spk_qt_canvas_compare_test)
    set_tests_properties(spk_qt_canvas_compare PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    qt_add_executable(spk_qt_frame_output_test tests/frame_output.cpp src/FrameOutput.cpp)
    target_include_directories(spk_qt_frame_output_test PRIVATE src)
    target_compile_features(spk_qt_frame_output_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_frame_output_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    add_test(NAME spk_qt_frame_output COMMAND spk_qt_frame_output_test "$<TARGET_FILE_DIR:SpektraLabQt>/resources")

    qt_add_executable(spk_qt_library_workflow_test tests/library_workflow.cpp
        src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_include_directories(spk_qt_library_workflow_test PRIVATE src)
    target_compile_features(spk_qt_library_workflow_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_library_workflow_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    qt_add_executable(spk_qt_edit_store_test tests/edit_store.cpp src/EditStore.cpp)
    target_include_directories(spk_qt_edit_store_test PRIVATE src)
    target_compile_features(spk_qt_edit_store_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_edit_store_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    add_test(NAME spk_qt_edit_store COMMAND spk_qt_edit_store_test)
    if(WIN32)
        # Specify each test's DLL search path: CTest on this Windows host can
        # restore the pre-launch PATH between tests. Ambient PATH alone makes
        # the second Qt test fail to load even when it passes in isolation.
        set(qt_test_runtime_path "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
        if(MINGW)
            get_filename_component(qt_test_compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
            list(APPEND qt_test_runtime_path "PATH=path_list_prepend:${qt_test_compiler_directory}")
        endif()
        set_tests_properties(spk_qt_frame_output spk_qt_edit_store spk_qt_canvas_compare PROPERTIES
            ENVIRONMENT_MODIFICATION "${qt_test_runtime_path}")
    endif()
    qt_add_executable(spk_qt_editing_workflow_test tests/editing_workflow.cpp
        src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_include_directories(spk_qt_editing_workflow_test PRIVATE src)
    target_compile_features(spk_qt_editing_workflow_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_editing_workflow_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    qt_add_executable(spk_qt_color_workflow_test tests/color_workflow.cpp
        src/PreviewController.hpp src/PreviewController.cpp src/FrameOutput.cpp src/EditStore.cpp)
    target_include_directories(spk_qt_color_workflow_test PRIVATE src)
    target_compile_features(spk_qt_color_workflow_test PRIVATE cxx_std_20)
    target_link_libraries(spk_qt_color_workflow_test PRIVATE spk_desktop_host Qt6::Core Qt6::Gui)
    # Full-RAW fixtures and a fresh output directory are explicit test arguments;
    # no absent-fixture skip can masquerade as a green CTest run.
endif()
foreach(qt_output_target SpektraLabQt spk_qt_frame_output_test spk_qt_library_workflow_test spk_qt_editing_workflow_test spk_qt_color_workflow_test)
    if(TARGET ${qt_output_target})
        target_link_libraries(${qt_output_target} PRIVATE spk_qt_post)
    endif()
endforeach()
