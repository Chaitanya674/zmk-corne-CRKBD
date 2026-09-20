# Tells `west build` to compile status_screen.c into the firmware.
#
# Without this file, config/status_screen.c is just a text file sitting
# next to corne.conf and corne.keymap — ZMK's build never picks it up.
# CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM=y in corne.conf only tells ZMK
# to *call* zmk_display_status_screen() instead of drawing its own
# built-in screen; it does not tell the build system *where* to find
# that function's definition. This file is what does that.
target_sources(app PRIVATE status_screen.c)
