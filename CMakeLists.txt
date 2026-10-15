cmake_minimum_required(VERSION 3.24)
project(STM32_Csim VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(CSIM_GUI "Build the WebView desktop application" ON)
set(UNICORN_ARCH arm CACHE STRING "Only emulate ARM" FORCE)
set(UNICORN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(UNICORN_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/unicorn EXCLUDE_FROM_ALL)

# Firmware is a separate ARM compilation, never a host executable.
find_program(FIRMWARE_CLANG NAMES clang REQUIRED)
find_program(LLVM_OBJCOPY NAMES llvm-objcopy REQUIRED)
set(FIRMWARE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/firmware/main.c" CACHE FILEPATH "Bare-metal main.c")
get_filename_component(FIRMWARE_SOURCE "${FIRMWARE_SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
function(arm_firmware name source)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/${name}.elf ${CMAKE_BINARY_DIR}/${name}.bin
    COMMAND ${FIRMWARE_CLANG} --target=arm-none-eabi -mcpu=cortex-m4 -mthumb
      -mfloat-abi=soft -std=c17 -O0 -g -ffreestanding -fno-builtin -nostdlib
      -Wall -Wextra -Werror -fuse-ld=lld
      -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f446re.ld
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware/startup.c ${source}
      -o ${CMAKE_BINARY_DIR}/${name}.elf
    COMMAND ${LLVM_OBJCOPY} -O binary ${CMAKE_BINARY_DIR}/${name}.elf ${CMAKE_BINARY_DIR}/${name}.bin
    DEPENDS firmware/startup.c firmware/stm32f446re.ld ${source} VERBATIM)
  add_custom_target(${name} ALL DEPENDS ${CMAKE_BINARY_DIR}/${name}.bin)
endfunction()
arm_firmware(firmware ${FIRMWARE_SOURCE})
arm_firmware(test_firmware ${CMAKE_CURRENT_SOURCE_DIR}/tests/firmware_probe.c)
arm_firmware(blink_firmware ${CMAKE_CURRENT_SOURCE_DIR}/firmware/main.c)
arm_firmware(fault_firmware ${CMAKE_CURRENT_SOURCE_DIR}/tests/firmware_fault.c)
function(csim_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()
add_library(csim_core STATIC src/stm32/f446/rcc.c src/stm32/f446/gpio.c
  src/stm32/f446/device.c src/circuit/circuit.c src/emulator/firmware.c src/emulator/cpu.c)
target_include_directories(csim_core PUBLIC src)
target_link_libraries(csim_core PUBLIC unicorn)
csim_warnings(csim_core)
include(CTest)
add_executable(csim_tests tests/test_core.c)
target_link_libraries(csim_tests PRIVATE csim_core)
csim_warnings(csim_tests)
add_dependencies(csim_tests blink_firmware test_firmware fault_firmware)
add_test(NAME core_and_arm_execution COMMAND csim_tests ${CMAKE_BINARY_DIR}/test_firmware.bin ${CMAKE_BINARY_DIR}/blink_firmware.bin ${CMAKE_BINARY_DIR}/fault_firmware.bin)
if(CSIM_GUI)
  set(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
  set(WEBVIEW_BUILD_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
  add_subdirectory(third_party/webview EXCLUDE_FROM_ALL)
  find_package(Threads REQUIRED)
  add_executable(stm32-csim src/main.c src/app/webview_bridge.c src/app/platform.c)
  target_link_libraries(stm32-csim PRIVATE csim_core webview::core_static Threads::Threads)
  csim_warnings(stm32-csim)
  add_dependencies(stm32-csim firmware)
  # Assets next to the executable permit launching from any working directory.
  add_custom_target(csim_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/web $<TARGET_FILE_DIR:stm32-csim>/web
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/firmware.bin $<TARGET_FILE_DIR:stm32-csim>/firmware.bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FIRMWARE_SOURCE} $<TARGET_FILE_DIR:stm32-csim>/main.c
    DEPENDS stm32-csim firmware
    COMMENT "Refresh firmware and web assets beside the application")
  option(CSIM_GUI_TESTS "Run a native window smoke test with CTest (requires desktop)" OFF)
  if(CSIM_GUI_TESTS)
    add_test(NAME native_webview_smoke COMMAND stm32-csim --smoke-test)
    set_tests_properties(native_webview_smoke PROPERTIES TIMEOUT 45)
  endif()
endif()
