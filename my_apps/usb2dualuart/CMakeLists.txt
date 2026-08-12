set(BSP_COMMON_DIR ${CMAKE_SOURCE_DIR}/bsp/bsp_common)
set(TARGET_REQUIRED_LIBS usb_stack)
set(TARGET_REQUIRED_PRIVATE_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/ftdi)

# Board config vendored into config/ so this app doesn't depend on the SDK's
# bl702_dualuart board directory (see usb_daplink for the same pattern).
set(TARGET_REQUIRED_PRIVATE_OPTIONS
    "SHELL: -include ${CMAKE_CURRENT_LIST_DIR}/config/clock_config.h -include ${CMAKE_CURRENT_LIST_DIR}/config/peripheral_config.h -include ${CMAKE_CURRENT_LIST_DIR}/config/pinmux_config.h")

set(TARGET_REQUIRED_SRCS ${BSP_COMMON_DIR}/usb/usb_dc.c
                        ${CMAKE_CURRENT_LIST_DIR}/ftdi/ftdi_eeprom.c
                        ${CMAKE_CURRENT_LIST_DIR}/ftdi/usbd_ftdi.c
                        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptor.c
                        ${CMAKE_CURRENT_LIST_DIR}/uart_interface.c)
set(mains main.c)
generate_bin()
