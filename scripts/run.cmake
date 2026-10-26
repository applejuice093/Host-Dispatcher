cmake_minimum_required(VERSION 3.21)

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(BUILD_DIR "${PROJECT_ROOT}/build")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
    RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "CMake configuration failed (exit ${configure_result}).")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --config Release --target host-dispatcher
    RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Could not build the GUI target. Install a C++17 compiler and Qt 6 Widgets 6.2+, then retry. "
        "If Qt is installed in a custom location, set CMAKE_PREFIX_PATH before running this command."
    )
endif()

if(WIN32)
    set(APP_NAME "host-dispatcher.exe")
else()
    set(APP_NAME "host-dispatcher")
endif()

set(APP_CANDIDATES
    "${BUILD_DIR}/${APP_NAME}"
    "${BUILD_DIR}/Release/${APP_NAME}"
    "${BUILD_DIR}/Debug/${APP_NAME}"
    "${BUILD_DIR}/RelWithDebInfo/${APP_NAME}"
    "${BUILD_DIR}/MinSizeRel/${APP_NAME}"
)
foreach(candidate IN LISTS APP_CANDIDATES)
    if(EXISTS "${candidate}")
        set(APP_PATH "${candidate}")
        break()
    endif()
endforeach()

if(NOT APP_PATH)
    message(FATAL_ERROR "Build succeeded, but ${APP_NAME} was not found under ${BUILD_DIR}.")
endif()

if(WIN32)
    file(STRINGS "${BUILD_DIR}/CMakeCache.txt" qt6_dir_entry REGEX "^Qt6_DIR:[^=]*=")
    if(qt6_dir_entry)
        string(REGEX REPLACE "^[^=]*=" "" qt6_dir "${qt6_dir_entry}")
        get_filename_component(qt6_prefix "${qt6_dir}/../../.." ABSOLUTE)
        set(windeployqt "${qt6_prefix}/bin/windeployqt.exe")
    endif()
    if(NOT EXISTS "${windeployqt}")
        message(FATAL_ERROR "Could not find windeployqt.exe beside the selected Qt installation; install the Qt deployment tools.")
    endif()
    execute_process(
        COMMAND "${windeployqt}" --release --no-translations --compiler-runtime "${APP_PATH}"
        RESULT_VARIABLE deploy_result
    )
    if(NOT deploy_result EQUAL 0)
        message(FATAL_ERROR "Qt runtime deployment failed (exit ${deploy_result}).")
    endif()
endif()

message(STATUS "Launching ${APP_PATH}")
execute_process(
    COMMAND "${APP_PATH}"
    WORKING_DIRECTORY "${BUILD_DIR}"
    RESULT_VARIABLE app_result
)
if(NOT app_result EQUAL 0)
    message(FATAL_ERROR "The application exited with status ${app_result}.")
endif()
