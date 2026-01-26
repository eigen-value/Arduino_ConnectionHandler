cmake_minimum_required(VERSION 3.16)

project(Arduino_ConnectionHandler VERSION 0.1.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ESP32_VER "3.3.4")

set(ARDUINO_SDK_PATH "C:/Users/Lucio/AppData/Local/Arduino15")
set(ARDUINO_SDK_LIB_PATH "${ARDUINO_SDK_PATH}/libraries")
set(ARDUINO_CORE_PATH "${ARDUINO_SDK_PATH}/packages/esp32/hardware/esp32/${ESP32_VER}/cores/esp32")
set(ARDUINO_VARIANT_PATH "${ARDUINO_SDK_PATH}/packages/esp32/hardware/esp32/${ESP32_VER}/variants/esp32s3")
set(ARDUINO_TOOLCHAIN_PATH "${ARDUINO_SDK_PATH}/packages/esp32/tools/esp32-arduino-libs/idf-release_v5.5-8410210c-v2/esp32s3/include")
set(ARDUINO_LIB_PATH "C:/Users/Lucio/Documents/Arduino/libraries")
set(ESP32_LIB_PATH "C:/Users/Lucio/AppData/Local/Arduino15/packages/esp32/hardware/esp32/${ESP32_VER}/libraries")

# Collect header files
file(GLOB_RECURSE HEADERS "src/*.h" "src/*.hpp")

# Create dummy source file for IDE support
set(DUMMY_SOURCE "${CMAKE_BINARY_DIR}/dummy.cpp")
file(WRITE ${DUMMY_SOURCE} "// Dummy file for IDE support\n")

# Create library with dummy source + headers
add_library(${PROJECT_NAME} ${DUMMY_SOURCE} ${HEADERS})

# Add after your existing target_include_directories
target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        # Arduino core paths
        ${ESP32_LIB_PATH}
        ${ARDUINO_CORE_PATH}
        ${ARDUINO_SDK_LIB_PATH}
        ${ARDUINO_VARIANT_PATH}
        ${ARDUINO_TOOLCHAIN_PATH}
        # Arduino libraries (common ones)
        # ${ARDUINO_LIB_PATH}
        ${ARDUINO_LIB_PATH}/MKRGSM/src
        ${ARDUINO_LIB_PATH}/TinyGSM/src
        ${ARDUINO_LIB_PATH}/ArduinoIoTCloud/src
        # ESP32 libraries
        # ${ESP32_LIB_PATH}/SPI/src

        # ESP32
        ${ARDUINO_SDK_PATH}/packages/esp32/hardware/esp32/3.3.4/libraries/WiFi/src
        ${ARDUINO_SDK_PATH}/packages/esp32/hardware/esp32/3.3.4/libraries/SD/src
        ${ARDUINO_SDK_PATH}/packages/esp32/hardware/esp32/3.3.4/libraries/FS/src
        ${ARDUINO_SDK_PATH}/packages/esp32/hardware/esp32/3.3.4/libraries/SPI/src
        ${ARDUINO_SDK_PATH}/packages/esp32/hardware/esp32/3.3.4/cores/esp32
)

# Add Arduino defines
target_compile_definitions(${PROJECT_NAME}
        PUBLIC
        ARDUINO=10819        # remove if not using Arduino Core for ESP32
        ARDUINO_ARCH_ESP32
        # ARDUINO_ESP32_DATALOGGER
        BOARD_HAS_TINY_GSM
)

# Mark headers as public headers
set_target_properties(${PROJECT_NAME} PROPERTIES
        PUBLIC_HEADER "${HEADERS}"
        LINKER_LANGUAGE CXX
)