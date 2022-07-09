"""
Convert PNG images to raw bitmap data for dumb loading on a device.
This increases the storage requirement ~10x for small PNGs, but simplifies use on the device.
"""

from PIL import Image

import argparse
import os
import struct


def truncate_channels(im):
    """
    Zero the lower two bits of all channels in the image

    This can be used to preview colours similar to what a ST7789 display will show.

    Returns a new image
    """
    # Get a writable copy of the pixel data
    raw = list(im.getdata())

    for i in range(len(raw)):
        raw[i] = (raw[i][0] & 0xFC, raw[i][1] & 0xFC, raw[i][2] & 0xFC)

    trunc = Image.new(mode='RGB', size=im.size)
    trunc.putdata(raw)

    return trunc


if __name__ == '__main__':

    parser = argparse.ArgumentParser(
        'to-rgb6bpp',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="""Convert input images to RGB 666 for ST7789 display

Writes raw pixel data to a .rgb file next to the input file for reading into
memory of a ST7789 screen directly from an SD card. This is packed the same
as an 8-bpp image, but the lower two bits are effectively zeroed.
""")

    parser.add_argument('images', metavar='IMAGE', nargs='+', help="Image file to convert")

    args = parser.parse_args()

    for path in args.images:
        print(path)
        if not os.path.exists(path):
            print("File not found: %s" % path)
            continue

        glyph = Image.open(path).convert('RGBA')
        
        # Ensure the glyph is on a black background as the device does no alpha blending
        im = Image.new(mode='RGB', size=glyph.size, color=(0, 0, 0))
        im.paste(glyph, (0, 0), glyph)

        # Write raw rgb format with width and height data
        # This could be BMP with a little more work, but this is ok for now.
        name, ext = os.path.splitext(path)
        dest = name + '.rgb'
        with open(dest, 'wb') as handle:
            handle.write(struct.pack('BB', im.width, im.height))  # Images are known to be <= 240x240
            for pixel in im.getdata():
                handle.write(struct.pack('BBB', *pixel))
