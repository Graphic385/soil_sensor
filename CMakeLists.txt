# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

# Set board to Pico 2
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Import Pico SDK
include(pico_sdk_import.cmake)

# Import pico-extras (needed for powman example functions)
set(PICO_EXTRAS_FETCH_FROM_GIT ON CACHE BOOL "Automatically fetch pico-extras if missing")
include(pico_extras_import.cmake)

project(soil_sensor C CXX ASM)

# Initialize Pico SDK
pico_sdk_init()

# Add executable
add_executable(soil_sensor
    soil_sensor.c
)

pico_set_program_name(soil_sensor "soil_sensor")
pico_set_program_version(soil_sensor "0.1")

# Enable USB stdio
pico_enable_stdio_uart(soil_sensor 0)
pico_enable_stdio_usb(soil_sensor 1)

# -------------------------------
# Required libraries for powman OFF
# -------------------------------
#
# You MUST link:
#   hardware_powman     → powman controller
#   pico_powman_example → helper functions (powman_example_init, off_for_ms)
#   pico_stdlib         → stdlib
#   hardware_adc        → ADC sensor readings
#
# DO NOT link hardware_rtc (RP2350 has no RTC!)
# -------------------------------

target_link_libraries(soil_sensor
    pico_stdlib
    pico_printf
    hardware_adc
    hardware_clocks
    hardware_powman
)

target_include_directories(soil_sensor PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Create UF2, ELF etc.
pico_add_extra_outputs(soil_sensor)
