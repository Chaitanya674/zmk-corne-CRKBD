# Corne OLED fix — what was wrong and what changed

## 1. The all-blue panel after the swap

ZMK's stock `app/boards/shields/corne/corne.dtsi` hard-codes the display node:

```
oled: ssd1306@3c {
    compatible = "solomon,ssd1306fb";
    width = <128>;
    height = <32>;
    multiplex-ratio = <31>;
    segment-remap;
    com-invdir;
    com-sequential;
    inversion-on;        <-- this is the one that bit you
    prechargep = <0x22>;
};
```

Two things follow from that.

**`inversion-on` is why your colours were backwards.** It sends the SSD1306
inverse-display command, so a `0` in the framebuffer *lights* the pixel. Your
`status_screen.c` correctly drew a black background and a white helmet — the
panel then flipped it, which is exactly the "light blue background, darker blue
trooper" you saw. Nothing in the C file was wrong about that.

**The geometry is fixed at 128x32.** If the OLED you swapped in is not that
exact part (a 0.96" 128x64, or a 1.3" SH1106), the framebuffer only ever
touches a fraction of the panel. Combine that with inverted polarity and every
untouched pixel is lit — a solid blue screen. That is your current symptom, and
it is a devicetree problem, not a C problem.

There was no `.overlay` file anywhere in your config, so nothing was overriding
the stock node.

**Fix:** `oled.dtsi` now deletes `inversion-on` and lets you pick the panel
geometry. It is pulled in by both `corne_left.overlay` and
`corne_right.overlay`. Open it and choose Option A, B or C to match the
physical part.

## 2. Nothing on the right half

The OLED node lives in the shared `corne.dtsi`, so the right half already has a
display defined — that is not the gap. Work down this list in order:

1. **Flash both halves.** `corne_right-nice_nano_v2.uf2` has to go on the right
   controller. The new screen prints `L` or `R`, so you can now tell at a
   glance whether the right half is running the right firmware.
2. **External power.** You have `CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER=y`. The
   Corne OLED's VCC sits on that same rail, so turning RGB off cuts power to
   the screen. Your old keymap had no way to turn it back on. The RGB layer now
   has `EP_ON` / `EP_OFF` / `EP_TOG` on the top-left three keys — press `EP_ON`
   and see if the right screen wakes up. If it does, that was the whole story.
   If you would rather not risk it, comment out the `EXT_POWER` line in
   `corne.conf`.
3. **Hardware.** Reseat the right OLED, check all four pins for continuity, and
   confirm the module is addressed `0x3c` (a few are `0x3d`). Swap the two OLED
   modules between halves — if the fault follows the module, it is the module.
4. **Logs.** Uncomment the logging block at the bottom of `corne.conf`, flash
   the right half, and read the USB console. A failed I2C probe of the SSD1306
   shows up there immediately.

## 3. Changes to `status_screen.c`

- Every shape now calls `lv_obj_remove_style_all()` first. A bare
  `lv_obj_create()` inherits the LVGL default theme — background fill, border,
  scrollbars — and on a 1-bit panel all of that turns into stray lit pixels.
  Your original eyes and divider were carrying an invisible themed border.
- Screen background is explicitly opaque black; the helmet, vents and text are
  white, i.e. lit. With `inversion-on` removed that renders as a blue trooper
  on a dark panel.
- Compile-time side detection via `CONFIG_ZMK_SPLIT_ROLE_CENTRAL`, which ZMK
  sets on the central (left) half only. Each `.uf2` gets its own `L`/`R` label.
- The circle-with-two-dots is now an actual helmet shape: shell outline, brow
  ridge, two lenses, centre ridge, mouth vent, cheek vents.
- Added `#include <stdio.h>` for `snprintf` and `ARG_UNUSED` on the unused
  event argument.

## Flashing

```
config/
├── corne.conf
├── corne.keymap
├── corne_left.overlay
├── corne_right.overlay
├── oled.dtsi
└── status_screen.c
```

Drop these into your `zmk-config/config/` folder, keeping whatever CMake wiring
you already use to compile `status_screen.c` (it is already working, since the
left screen renders). Build both halves, flash both, and power-cycle each one.

## If it still comes out inverted

Comment out `/delete-property/ inversion-on;` in `oled.dtsi` and write
`inversion-on;` instead. That single property is the entire polarity control —
do not try to compensate for it by swapping black and white in the C file, or
you will just move the problem.

## LVGL version note

This is LVGL 8.x API, matching current ZMK. If you are on a build that has moved
to LVGL 9, rename `lv_obj_clear_flag` to `lv_obj_remove_flag` — nothing else in
the file changes.
