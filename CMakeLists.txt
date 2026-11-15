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
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(switchbot_ble_scan C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(switchbot_ble_scan
    main.c
)
pico_set_program_name(switchbot_ble_scan "switchbot_ble_scan")
pico_set_program_version(switchbot_ble_scan "0.1")

# BTstack 設定ヘッダのパスを通す
target_include_directories(switchbot_ble_scan PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(switchbot_ble_scan
    pico_stdlib
    pico_cyw43_arch_none   # Wi-Fiなし・BLE専用モード
    pico_btstack_ble       # BLE サポート
    pico_btstack_cyw43     # CYW43 向け BTstack HAL
)

# USB CDC シリアルを有効化、UARTは無効
pico_enable_stdio_usb(switchbot_ble_scan 1)
pico_enable_stdio_uart(switchbot_ble_scan 0)

# UF2 / bin / hex を生成
pico_add_extra_outputs(switchbot_ble_scan)
