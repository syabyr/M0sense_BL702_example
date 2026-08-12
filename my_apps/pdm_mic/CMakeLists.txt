set(TARGET_REQUIRED_LIBS usb_stack shell usb_stdio)

# Board config vendored into config/ so this app doesn't depend on the SDK's
# bl702_dapplus board directory (pinmux is custom I2S, clock/peripheral copied).
set(TARGET_REQUIRED_PRIVATE_OPTIONS
    "SHELL: -include ${CMAKE_CURRENT_LIST_DIR}/config/clock_config.h -include ${CMAKE_CURRENT_LIST_DIR}/config/peripheral_config.h -include ${CMAKE_CURRENT_LIST_DIR}/config/pinmux_config.h")

set(mains main.c)
generate_bin()
