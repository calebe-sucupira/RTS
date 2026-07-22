cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(
    $ENV{FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake
)

project(pico_freertos_cart C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(
    pico_freertos_cart
    src/main.c
    src/app_tasks.c
    src/motor_driver.c
    src/servo.c
    src/ultrasonic.c
)

target_include_directories(
    pico_freertos_cart
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/inc
)

target_link_libraries(
    pico_freertos_cart
    PRIVATE
        pico_stdlib
        hardware_clocks
        hardware_gpio
        hardware_pwm
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
)

target_compile_options(
    pico_freertos_cart
    PRIVATE
        -Wall
        -Wextra
)

pico_enable_stdio_usb(pico_freertos_cart 1)
pico_enable_stdio_uart(pico_freertos_cart 0)
pico_add_extra_outputs(pico_freertos_cart)
