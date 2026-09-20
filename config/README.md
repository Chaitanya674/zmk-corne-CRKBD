# Corne OLED — round 2

## What I got wrong last time

I assumed both halves had the same panel and gave you one shared `oled.dtsi`.
With a 0.91" 128x32 on one side and a 1.3" on the other, a single definition
cannot be correct for both — one half was always going to be wrong. That file
is gone; each half now has its own independent panel definition.

## What your 1.3" panel almost certainly is

A 1.3" I2C OLED module is an **SH1106 at 128x64**, not an SSD1306 at 128x32.
That mismatch explains the solid blue screen precisely:

- The SH1106 has **132 columns of RAM** driving a 128-column panel, so
  everything needs a 2-column offset the SSD1306 config never applies.
- The SH1106 **does not implement SSD1306's horizontal addressing mode**. The
  driver sets it, the SH1106 ignores it, and the page pointer never advances
  the way the driver assumes. Pixel data lands in the wrong rows.
- ZMK's stock node carries `inversion-on`, so every byte the driver never
  wrote correctly renders as **lit**, not dark.

Put together: a mostly-untouched framebuffer displayed inverted = a fully lit
panel. It was never going to be fixable from `status_screen.c`.

`corne_left.overlay` now declares `compatible = "sinowealth,sh1106"`, 128x64,
`segment-offset = <2>`, `multiplex-ratio = <63>`, drops `com-sequential`
(64-row panels use alternating COM pins) and drops `inversion-on`.

`corne_right.overlay` keeps the stock 128x32 SSD1306 geometry and only drops
`inversion-on`.

**If the 1.3" is on your RIGHT half, swap the contents of the two files.** I
guessed left because that is the side you said you swapped.

## Turn on the debug frame first

Do not flash the artwork yet. Open `status_screen.c` and set:

```c
#define STATUS_SCREEN_DEBUG_FRAME 1
```

Flash both halves. Each screen should draw a 1px outline hugging all four
edges, a solid tick in the top-left corner, and text in the middle reading
`L 128x64` or `R 128x32`. Read the result:

| What you see | What it means |
|---|---|
| Clean rectangle touching all four edges | Geometry correct — set the define back to 0 |
| Rectangle floating in a lit field | Polarity still inverted — see below |
| Image shifted sideways, wrapping at an edge | Wrong `segment-offset`: try 0 or 2 |
| Outline only fills half the height | Wrong `height` / `multiplex-ratio` |
| Tick in the wrong corner | Flip `segment-remap` and/or `com-invdir` |
| Reported size ≠ what you configured | **Your .overlay is not being applied** |
| Nothing at all | Panel is not being driven — power or I2C |

That last row is the one to watch. If the left screen says `128x32` when the
overlay says 64, the overlay file is not reaching the build, and no amount of
editing it will help.

### If the overlay is not being applied

ZMK picks up `config/<shield>.overlay` per shield name. Check that:

- The files are named exactly `corne_left.overlay` and `corne_right.overlay`
  and sit in `config/`, next to `corne.keymap` — not in a subfolder.
- Your `build.yaml` shields are `corne_left` and `corne_right`. If you build
  with extra shields listed, the names still have to match exactly.
- You are not also carrying a `config/boards/shields/corne/` folder from an
  earlier attempt, which would shadow the upstream shield entirely.

### If polarity is still inverted

Every half has exactly one control for this. In that half's `.overlay`,
replace `/delete-property/ inversion-on;` with `inversion-on;`. Do not try to
compensate by swapping black and white in the C file.

### If the 1.3" still shows nothing under SH1106

Some 1.3" modules are genuinely SSD1306-based, and a few need the internal
reference enabled. Try, in order:

1. Keep 128x64 but change `compatible` back to `"solomon,ssd1306fb"` and set
   `segment-offset = <0>`.
2. Add `use-internal-iref;` to the node.
3. Look at the back of the module for the controller marking, or count pins —
   most 1.3" I2C modules are SH1106.

## Right half still blank

The debug frame settles which layer this is. If the right screen shows nothing
even with the frame enabled, it is not a rendering problem. Work down:

1. **Confirm you flashed it.** The frame prints `R`. If it prints `L`, the
   left-half firmware is on the right controller.
2. **External power.** You have `CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER=y`, and the
   Corne OLED's VCC is on that rail — turning RGB off cuts the screen. The RGB
   layer now has `EP_ON` on the top-left key. Press it. If the screen comes
   back, that was the whole thing. To rule it out permanently, comment that
   line out in `corne.conf` and reflash.
3. **Swap the modules between halves.** If the dead screen follows the module,
   it is the module. If it stays with the half, it is that controller or its
   solder joints.
4. **I2C address.** A minority of modules are `0x3d`. Change `reg = <0x3c>;`
   and the node name suffix to match.
5. **Logs.** Uncomment the logging block in `corne.conf`, plug the right half
   in over USB, and read the console. A failed probe shows up immediately.

## Files

```
config/
├── corne.conf            # mono LVGL settings, dedicated display work queue
├── corne.keymap          # + EP_ON / EP_OFF / EP_TOG on the RGB layer
├── corne_left.overlay    # 1.3" SH1106 128x64
├── corne_right.overlay   # 0.91" SSD1306 128x32
└── status_screen.c       # resolution-adaptive, L/R aware, debug frame
```

`status_screen.c` now reads the panel size with `lv_disp_get_ver_res()` at
runtime and scales the helmet 2x on the 64px panel, so one source file serves
both mismatched halves.
