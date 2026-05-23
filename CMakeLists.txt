# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico-i2s-usb VERSION 0.1.0 LANGUAGES C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1.0

add_executable(pico-i2s-usb 
    src/main.c 
    src/i2s_audio.c
    src/dma_audio.c
    src/usb_audio.c
    src/usb_descriptors.c
)

pico_set_program_name(pico-i2s-usb "pico-i2s-usb")
pico_set_program_version(pico-i2s-usb "${PROJECT_VERSION}")

# Generate PIO headers.
# Note: i2s_rx_controller.pio is retained on disk as an educational artifact
# (see README "Clock Architecture") but is intentionally NOT built — the Pico
# cannot reliably act as I2S Controller on this hardware.
pico_generate_pio_header(pico-i2s-usb ${CMAKE_CURRENT_LIST_DIR}/src/i2s_rx_target.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico-i2s-usb 1)
pico_enable_stdio_usb(pico-i2s-usb 0)

# Add the standard library and hardware libraries to the build
target_link_libraries(pico-i2s-usb
        pico_stdlib
        hardware_pio
        hardware_dma
        hardware_clocks
        hardware_pwm
        tinyusb_device
        tinyusb_board
)

# Add the standard include files to the build
target_include_directories(pico-i2s-usb PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
)

pico_add_extra_outputs(pico-i2s-usb)

