# UNICODE UTF-8 STANDARD INPUT TERMINAL

This user manual describes operation of your new, state of the art, UTF-8
binary input terminal. While this is a relatively simple data entry device,
several functions are not necessarily obvious, so all users are encouraged
to read this short manual.

## Power-up

When the device is turned on, it scans all fonts installed in the `fonts/`
directory on the SD card for fast access later. The Unicode logo is displayed
on screen while this scan is in progress, which takes around 30 seconds with
the standard font package. The colouring of the logo represents the progress
of this scan. When the logo is fully coloured, all fonts have been processed
and the device is ready to use.

The USB interface of the device will not activate until after this initial
loading is complete.


## Data Entry

In all modes:

 - The most significant byte is selected first, then right-shifted by briefly
   engaging the SHIFT switch. For example, to make the value `0x1234` you first
   select `0x12` on the DATA INPUT switches, press the SHIFT switch momentarily,
   then select `0x34` on the DATA INPUT switches: your buffer now has the desired
   value and can be sent.

 - Pressing the SEND switch will output the current buffer to the host computer
   system. The buffer will be reset to the current DATA INPUT byte selection after
   sending unless shift-lock is engaged.

 - Holding SHIFT enables shift-lock. Shift-lock retains the most significant bytes
   of the buffer across SEND operations, allowing multiple codepoints in the same
   range to be entered rapidly. For example with shift lock enabled, you can enter
   `0x1F40D`, press send, then simply flip DATA INPUT bit 6 and press send again
   for `0x1F44D`.

 - Pressing the MODE switch momentarily will cycle between the input and view modes
   described below. The buffer and shift-lock will be retained while switching modes.

 - Holding CLEAR resets the buffer to the current DATA INPUT selection and disables
   shift-lock. The selected mode remains active.


## Modes

### Codepoint Mode (HEX, DEC)

 - Entry of a codepoint value via DATA INPUT and SHIFT
 - Codepoint value is displayed in hexadecimal (HEX) or decimal (DEC), centred
   at the bottom of the screen.
 - The corresponding glyph is rendered if the codepoint is found in any of the
   available fonts.
 - Unicode block and codepoint names are displayed at the top of the screen
   when codepoint is valid.

### UTF-8 Mode (UTF-8)

 - Entry of a [1-4 byte UTF-8 sequence](https://en.wikipedia.org/wiki/UTF-8#Encoding) via DATA INPUT and SHIFT
 - If an invalid encoding is entered, binary literals show problematic bits in red.
 - The SHIFT operation is automatically limited to the encoded sequence length.
 - Glyph is rendered when a valid UTF-8 sequences is entered, if found in the available fonts.

Changing to this mode will show the UTF-8 encoding of the codepoint selected in
the previous mode. The low byte may not match the DATA INPUT switches to
achieve this. When the DATA INPUT changes, the low byte will be updated.

### Numeric / Programmer Mode (LITERAL)

 - Always shows a large hexadecimal representation of the input buffer
 - Pressing SEND writes the literal hex value like "0xF1"


## Notes

### Glyph display

Glyphs are always drawn so their visible components are centred on screen,
rather than using a fixed baseline. This means glyphs lose some positional
information, which is not technically accurate, but provides consistent
positioning for glyphs of all shapes and sizes, regardless of how their
script handles baselines.
