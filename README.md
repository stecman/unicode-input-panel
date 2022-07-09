# UNICODE Input Terminal

Classic binary data entry meets modern Unicode: 144K characters at your
fingertips. A physical user interface to enter any UTF-8 sequence over USB.

A display shows a preview of the selected glyph(s), along with metadata about
the codepoint - all rendered entirely on-device. As 30MB+ of font files are
required to render all of Unicode, these are read from an SD card.

See the [Hackaday project page]() for more detail about the build.


## Getting started

Parts and wiring can be found in the schematic. The gist is:

 - Raspberry Pi Pico
 - A bunch of switches (8x latching toggle, 1x 3-position momentary, 1x 2-position momentary)
 - ST7789 based display (ideally 240x300, but other resolutions could be made to work)
 - SD card receptacle of some form that can be connected via SPI

A pre-built `.uf2` firmware image can be downloaded from the [releases page]()
if you don't want to build the code yourself.

A package of fonts can be downloaded from the [releases page](). These need to
be placed in a folder called `fonts` at the root of a FAT-formatted SD card. If
you want to bring your own fonts, see the *Preparing fonts* section towards the
end of this readme.


## Developing

You'll need an internet connection for the initial configuration as CMake will
automatically download several dependencies that need to be built from source
(FreeType, harfbuzz, libpng, zlib), and some of [Unicode's data tables](https://unicode.org/Public/UNIDATA/).

### Command-line / Linux

First:

 - Install `cmake` and `gcc-arm-none-eabi` from your package manager
 - Install the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
   and set the `PICO_SDK_PATH` environment variable to point to it.

Then:

```sh
# Configure the build
cd firmware
mkdir build
cd build
cmake ../

# Build firmware image
make -j4
```

This will give you a file `firmware.uf2` in the build directory, which can be
copied to the Pico (when started with the BOOTSEL button held).

### Using the Raspberry Pi Foundation's IDE

TODO: check this actually works
 
### Host builds

For fast development and debugging, builds can target the host computer. This
renders a virtual screen using SDL2.

```sh
# In a fresh build directory
cmake ../ -DPICO_PLATFORM=host
make -j4

# Run the application
./firmware
```

## Notes

### Preparing fonts

If you want to use fonts other than those in the [prepared bundle]() on the
device, there are some considerations: FreeType is fairly sparing with its
memory use, but it still requires loading some sections of a font entirely
into memory during initial load/open (`FT_Open_Face`).

- From observation, OTF fonts seem to require more memory to open than TTF fonts.
  The fonts in the prepared bundle were all converted to TTF using [otf2ttf](https://github.com/awesometoolbox/otf2ttf).

- Fonts with many thousands of glyphs like NotoSansJP can be too large to open
  on the Pico. A couple of these large fonts were split into smaller fonts with
  around 500 glyphs each, using the `pyftsubset` tool mentioned below.

  TODO: Note this probably discards information from the GSUB table

### Changing the embedded UI font

A compact version of Open Sans Regular is built into the firmware for use in the UI,
which only needs basic ASCII glyphs. This is about 5x smaller than the original,
avoiding wasted flash space.

Fonts can be stripped like this using the `pyftsubset` utility from [fonttols](https://github.com/fonttools/fonttools):

```sh
python3 -c 'for i in range(0x20, 0x7D): print("U+%04X" % i)' > ascii-codepoints.txt
pyftsubset SomeFont.ttf --output-file=SomeFont-stripped.ttf --unicodes-file=ascii-codepoints.txt
```

### Debugging memory issues

If you're getting out of memory panics, malloc debugging messages can be
enabled with `-DPICO_DEBUG_MALLOC=1` (prints detail of every allocation to
the serial port).


## Attribution

This project includes copies of:

 - [FatFS SD SPI for Raspberry Pi Pico](https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico),
   which contains [FatFS](http://elm-chan.org/fsw/ff/00index_e.html)). This is included as
   source in the repository because it need slight tweaks to build correctly.

 - A stripped down version of the [OpenSans](https://github.com/googlefonts/opensans)
   Regular font is included in this repository: Copyright 2020 The Open Sans Project Authors

 - The releases page has an archive with redistribution of the Noto Regular fonts
   ([noto-fonts](https://github.com/googlefonts/noto-fonts/) and
   [noto-emoji](https://github.com/googlefonts/noto-emoji)): Copyright 2018 The Noto Project Authors

The respective licenses for these are included with the associated files.
