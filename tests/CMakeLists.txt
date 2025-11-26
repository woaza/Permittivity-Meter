cmake_minimum_required(VERSION 3.20)
project(perm_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(CORE_DIR ${PROJECT_ROOT}/PermittivityMeterV2/PermitivityMeterV2/Core)
set(SRC_DIR ${CORE_DIR}/Src)
set(INC_DIR ${CORE_DIR}/Inc)

set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.c
    ${SRC_DIR}/bsp_rf.c
    ${SRC_DIR}/bsp_ui.c
    ${SRC_DIR}/bsp_lcd.c
    ${SRC_DIR}/bt_manager.c
    ${SRC_DIR}/debug_log.c
    ${SRC_DIR}/fsm_main.c
    ${SRC_DIR}/math_model.c
    ${SRC_DIR}/rf_measure.c
    ${SRC_DIR}/rf_trace.c
    ${SRC_DIR}/usb_cdc_bridge.c
    ${SRC_DIR}/mocks/mock_board.c
)

add_executable(perm_host_tests ${TEST_SOURCES})

target_include_directories(perm_host_tests PRIVATE
    ${INC_DIR}
    ${INC_DIR}/mocks
)

target_compile_definitions(perm_host_tests PRIVATE UNIT_TESTS=1)

if (MSVC)
    target_compile_options(perm_host_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(perm_host_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(perm_host_tests PRIVATE m)
endif()

add_test(NAME perm_host_tests COMMAND perm_host_tests)
