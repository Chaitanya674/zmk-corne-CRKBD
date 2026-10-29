# Corne (nice!nano v2, ZMK main): notes

## Power and split

- The halves talk over **Bluetooth only**. ZMK's wired split needs two data
  wires, and the Corne's TRRS jack has one.
- There are **no batteries**. The TRRS cable carries power only: the right
  half runs off the left nice!nano's switched external power rail (VCC).
- USB goes into the **left** half (the central). The right half cannot talk
  to the computer.
- Never plug or unplug the TRRS cable while anything is powered.
- `CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER=n` keeps that rail on when RGB is
  toggled off. With it on, RGB off switched the rail off, which killed the
  right half and the OLEDs, and the saved state kept them dead on every boot.
- The keymap has no `EP_OFF`/`EP_TOG` for the same reason. `EP_ON` (RGB
  layer, top-left) is the recovery key.

## OLEDs

Both panels are 0.91" SSD1306 128x32, ZMK's stock Corne definition, so
there are no `.overlay` files.

A **solid blue panel** came from `status_screen.c` filling the screen with
`lv_color_black()`. On this stack (LVGL 9 + Zephyr SSD1306) black is a
**lit** pixel and white is a **dark** one, whether or not `inversion-on` is
set. Always draw with the `PX_ON` / `PX_OFF` macros in `status_screen.c`.

Set `STATUS_SCREEN_DEBUG_FRAME 1` to draw a 1px outline around the panel
edge when checking the screen's size and position.

## After changing `corne.conf`

Flash both halves. If the halves stop pairing or a saved state looks stuck,
flash `settings_reset` to each half, then `corne_left` / `corne_right`.
