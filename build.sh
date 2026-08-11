#!/bin/bash
# Add RISC-V toolchain to PATH
TOOLCHAIN_DIR="$HOME/develop/bl-riscv/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-apple-darwin/bin"
[ -d "$TOOLCHAIN_DIR" ] && export PATH="$TOOLCHAIN_DIR:$PATH"

# y for printf on usb_cdc_acm and n for printf on uart
SUPPORT_USBSTDIO_ENABLE=y


APP=rtos_demo
APP_DIR=m0sense_apps
if [ "$1" != "" ]; then

    if [ "$1" = "clean" ]; then
    	rm -rf m0sense_apps/**/submodule_commit_info.txt bl_mcu_sdk/{build,out}
        echo "clean the produced files!"
        exit
    fi

    if [ "$1" = "patch" ]; then
    	cd bl_mcu_sdk
        git switch -c patch
        git reset --hard origin/release_v1.4.5
        git am --keep-cr ../misc/sdk_patch/*.patch
        echo "Apply patch for you!"
        cd ..
        exit
    fi

    if [ ! -d "$1" -o ! -f "$1/CMakeLists.txt" ]; then
        echo "no this app project \"$1\"!"
        exit
    fi

    APP=${1##*/}
    APP_DIR=${1%%/$APP}

    # These apps use their own USB descriptors (not compatible with usb_stdio CDC ACM)
    # so USBSTDIO must be disabled
    case "$APP" in
        usb2dualuart|usb_daplink|rv_dap_plus)
            SUPPORT_USBSTDIO_ENABLE=n
            ;;
    esac

fi

_APP_DIR_FIRST="${APP_DIR%%/*}"
if [ "$_APP_DIR_FIRST" != "m0sense_apps" ] && [ "$_APP_DIR_FIRST" != "my_apps" ] && [ "$SUPPORT_USBSTDIO_ENABLE" = "y" ]; then
    echo "not support \`SUPPORT_USBSTDIO_ENABLE=y\` yet, please disable it in build.sh!"
    exit
fi

cd bl_mcu_sdk
if [ $SUPPORT_USBSTDIO_ENABLE = "y" -a $(git rev-parse HEAD) = $(git rev-parse origin/release_v1.4.5) ]; then
git am --signoff --keep-cr ../misc/sdk_patch/*.patch
echo "Apply patch for you!"
fi

case "$APP" in
    usb2dualuart)
        BOARD=bl702_dualuart
        ;;
    usb_daplink)
        BOARD=bl702_daplink
        ;;
    rv_dap_plus)
        BOARD=bl702_dapplus
        ;;
    *)
        BOARD=bl702_iot
        ;;
esac

make APP=$APP APP_DIR=../$APP_DIR BOARD=$BOARD SUPPORT_FLOAT=y SUPPORT_USBSTDIO_ENABLE=$SUPPORT_USBSTDIO_ENABLE
cd ..

TARGET=bl_mcu_sdk/out/$APP_DIR/$APP/${APP}_bl702.bin
ls -alh $TARGET

# extra
if [ ! "$APP" = "m0sense_boot" ]; then
    UF2_CVT="misc/utils/uf2_convert"
    $UF2_CVT $TARGET uf2_demos/${APP}.uf2
fi