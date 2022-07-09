# UNICODE UTF-8 STANDARD INPUT TERMINAL

This user manual describes operation of your new, state of the art, UTF-8
binary input terminal. While a relatively simple data entry device, it has
several functions are not necessarily obvious, so all users are encouraged
to read this short manual.

## Power-up

When the device is turned on, it scans all fonts installed in the `fonts/`
directory on the SD card for fast access later. The Unicode logo is displayed
on screen while this scan is in progress, which takes around 30 seconds with
the standard font package. The colouring of the logo represents the progress
of this scan. When the logo is fully coloured, all fonts have been processed.


## Data Entry

In all modes:

 - The most significant byte is selected first, then right-shifted by briefly
   engaging the SHIFT switch. For example, to make the value `0x1234` you
   would first select `0x12` on the DATA INPUT swtiches, then press the SHIFT
   switch momentarily, then select `0x34` on the DATA INPUT switches: your buffer
   now has the desired value and can be sent.

 - Holding SHIFT will enable SHIFT LOCK. This automatically copies all upper
   bytes of the current codepoint during multiple character entry and SEND
   operations, allowing multiple codepoints in the same range be entered
   rapidly.

 - Pressing the SEND switch will output the current buffer, then clear it.
 
   If the host system isn't using using the custom direct-input driver, the
   first SEND operation after each USB initialisation will prompt for the HID
   input fallback mode to use. This is required as different operating
   systems and desktop environments use different keyboard sequences for
   direct codepoint entry.

 - Pressing the MODE switch momentarily will switch between the input modes
   described below. 


## Modes

### Codepoint Mode

 - A glyph is always rendered based on the current input value
 - As multiple characters cannot be 

### UTF-8 Mode

 - Bytes are entered directly in UTF-8 encoding
 - The most significant byte is selected first, then right-shifted by briefly engaging the SHIFT switch.
 - A numeric/hex prompt is shown until a valid [1-4 byte UTF-8 encoding](https://en.wikipedia.org/wiki/UTF-8#Encoding) is entered
 - A glyph preview is rendered when the lowest byte of the encoding is being selected
 - Multiple characters can be entered by performing a normal SHIFT and continuing on the next character's encoding.

### Numeric Mode

 - The buffer is treated as a single numeric value, up to 64-bits
   (right-aligned to always include the current DATA INPUT byte).
 - Hexadecimal, decimal, and binary forms of the value are displayed


## Notes

### Glyph display

Glyph and glyph sequences are always displayed centered on screen. This means
individual glyphs lose some positional information, which is not technically
accurate, but provides consistent positioning for glyphs of all shapes and sizes.