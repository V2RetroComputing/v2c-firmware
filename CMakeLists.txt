cmake_minimum_required(VERSION 3.16)

include(cmake/pico_sdk_import.cmake)

project(iic_firmware)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONFIG_SYSCLOCK=252 -DPICO_FLASH_SPI_CLKDIV=8 -DOVERCLOCKED=1 -DPICO_XOSC_STARTUP_DELAY_MULTIPLIER=128 -DPICO_USE_MALLOC_MUTEX=1")

add_executable(iic_firmware)
target_sources(iic_firmware PUBLIC
    main.c iic.c vgaout.c render.c font/rom_videx.o
)

pico_generate_pio_header(iic_firmware ${CMAKE_CURRENT_LIST_DIR}/vidin.pio)
pico_generate_pio_header(iic_firmware ${CMAKE_CURRENT_LIST_DIR}/vidout.pio)

target_link_libraries(iic_firmware
    pico_multicore
    pico_stdlib
    hardware_resets
    hardware_irq
    hardware_pio
    hardware_dma
    hardware_flash
)

pico_add_extra_outputs(iic_firmware)

pico_enable_stdio_usb(iic_firmware 0)
pico_enable_stdio_uart(iic_firmware 0)
