"""PlatformIO post-build step: writes firmware-merged.bin next to firmware.bin.

The merged image contains bootloader + partition table + boot_app0 + app and
is flashed at offset 0x0 - one file, one command, also works with browser
flashers (ESP Web Tools / esptool-js):

  esptool.py --chip esp32 --baud 460800 write_flash 0x0 firmware-merged.bin
"""
import os

Import("env")  # noqa: F821


def merge_bin(source, target, env):  # noqa: ARG001
    build = env.subst("$BUILD_DIR")
    app = os.path.join(build, env.subst("${PROGNAME}.bin"))
    out = os.path.join(build, "firmware-merged.bin")
    app_offset = env.subst("$ESP32_APP_OFFSET") or "0x10000"
    images = []
    for offset, path in env.get("FLASH_EXTRA_IMAGES", []):
        images += [offset, env.subst(path)]
    images += [app_offset, app]
    cmd = [
        env.subst("$PYTHONEXE"), env.subst("$OBJCOPY"), "--chip", "esp32", "merge_bin",
        "-o", out, "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB",
    ] + images
    print("merge_bin:", " ".join(images))
    if env.Execute(" ".join(f'"{c}"' for c in cmd)):
        raise SystemExit("merge_bin failed")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)  # noqa: F821
