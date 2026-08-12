set(BSP_COMMON_DIR ${CMAKE_SOURCE_DIR}/bsp/bsp_common)
set(CMSIS_DAP_DIR ${CMAKE_CURRENT_LIST_DIR}/DAP)
set(APP_SRC_DIR   ${CMAKE_CURRENT_LIST_DIR}/dap_only)

set(TARGET_REQUIRED_LIBS usb_stack)
set(TARGET_REQUIRED_PRIVATE_INCLUDE ${CMSIS_DAP_DIR}/Include
                                    ${APP_SRC_DIR}/include)

# Board config vendored into config/ so this app doesn't depend on the SDK's
# bl702_dapplus board directory (see usb_daplink for the same pattern).
set(TARGET_REQUIRED_PRIVATE_OPTIONS
    "SHELL: -include ${CMAKE_CURRENT_LIST_DIR}/config/clock_config.h -include ${CMAKE_CURRENT_LIST_DIR}/config/peripheral_config.h -include ${CMAKE_CURRENT_LIST_DIR}/config/pinmux_config.h")

set(TARGET_REQUIRED_SRCS ${BSP_COMMON_DIR}/usb/usb_dc.c
                         ${APP_SRC_DIR}/usb.c
                         ${CMSIS_DAP_DIR}/Source/DAP.c
                         ${CMSIS_DAP_DIR}/Source/JTAG_DP.c
                         ${CMSIS_DAP_DIR}/Source/SW_DP.c)

set(mains ${APP_SRC_DIR}/main.c)
generate_bin()
