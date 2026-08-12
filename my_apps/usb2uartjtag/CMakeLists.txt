set(BSP_COMMON_DIR ${CMAKE_SOURCE_DIR}/bsp/bsp_common)
set(TARGET_REQUIRED_LIBS usb_stack)

# Board config is vendored into config/ so this app doesn't depend on the
# SDK's bl702_dualuart board directory. The three files are force-included
# before bl702_config.h pulls in the SDK versions; their #ifndef guards then
# suppress those SDK copies (same mechanism as usb_daplink).
set(TARGET_REQUIRED_PRIVATE_OPTIONS
    "SHELL: -include ${CMAKE_CURRENT_LIST_DIR}/config/clock_config.h -include ${CMAKE_CURRENT_LIST_DIR}/config/peripheral_config.h -include ${CMAKE_CURRENT_LIST_DIR}/config/pinmux_config.h")

set(TARGET_REQUIRED_SRCS ${BSP_COMMON_DIR}/usb/usb_dc.c
                        ${CMAKE_CURRENT_LIST_DIR}/usbd_ftdi.c
                        ${CMAKE_CURRENT_LIST_DIR}/uart_interface.c
                        ${CMAKE_CURRENT_LIST_DIR}/jtag_process.c
                        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptor.c)
set(mains main.c)

# Use project-local linker script with separate ITCM+DTCM (8K each).
# The SDK's stock bl702_flash.ld packs code+data+stack into one 8K DTCM,
# which overflows for this JTAG app. The fixed script is kept in misc/ to
# avoid modifying the shared SDK submodule.
set(LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/../../misc/bl702_flash_fixed.ld)

generate_bin()
