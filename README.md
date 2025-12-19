# HID Bootloader for WCH CH32V203

USB HID bootloader for WCH CH32V203 (RISC-V) devices, built on TinyUSB.
It exposes a HID vendor-specific IN/OUT interface and accepts simple flash commands from a host tool.

## Features

- HID class device (no driver needed on most OSes)
- Simple flash protocol: erase, program, read, CRC, reset
- Double-reset entry (no dedicated BOOT pin required)
- Application jump to flash offset `0x00004000`

## Target hardware

- Default build targets CH32V203C8T6 with USBD
- Board config: `tinyusb/hw/bsp/ch32v20x/boards/ch32v203c_r0_1v0/`


## USB identifiers

- VID: `0xF055`
- PID: `0x6585`
- Product string: `HID Bootloader`

## Memory map

The bootloader resides at the start of flash and jumps to the application at
`0x00004000`.

Your application must be linked to run from `0x00004000` and provide its vector table there. If you change the offset, update both the bootloader and your app linker scripts.

## Bootloader entry

Two ways to enter the bootloader:

1. Double reset: press reset twice within ~500 ms. The first reset sets a magic value, the second reset keeps the bootloader active.
2. From the application: write `0x624C` to backup register `BKP_DR10` and reset.

The bootloader clears the magic value on entry, so normal single resets boot the application.

## Build

Prerequisites:

- `riscv-none-elf-gcc`
- `make`

Initialize submodules (TinyUSB):

```
git submodule update --init --recursive
```

Build:

```
make
```

Artifacts:

- `hidbootloader.elf`
- `hidbootloader.bin`

## Flash the bootloader

Flash `hidbootloader.bin` at address `0x00000000` using your preferred CH32
programmer (WCH-Link, RV-Link, etc.).

## Uploading firmware via HID

Use the host tool:

https://github.com/verylowfreq/hidupload

When building your application, output a binary linked for `0x00004000` and use that image with the uploader.

## License

MIT License (c) 2025 Mitsumine Suzu (@verylowfreq)


## Acknowledgements

- TinyUSB (https://github.com/hathach/tinyusb)
- WCH UF2 bootloader (https://github.com/ArcaneNibble/wch-uf2)
