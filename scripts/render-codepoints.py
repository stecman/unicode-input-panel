import argparse
import difflib
import glob
import os
import re

from concurrent.futures import ProcessPoolExecutor
from multiprocessing import cpu_count

import uharfbuzz as hb

from fontTools.misc.transform import Offset
from fontTools.pens.freetypePen import FreeTypePen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont
from PIL import Image, ImageOps


def build_font_map(fontfiles: list[str]):
    """
    Build a map of all available glyphs in a list of fonts
    Returns a dictionary mapping each available codepoint to the first font file that contains it
    """
    fontmap = {}

    for path in fontfiles:
        font = TTFont(path)

        cmap = font.get('cmap')
        for table in cmap.tables:  # type: ignore
            if not table.isUnicode():
                continue

            for code in table.cmap:
                if code not in fontmap:
                    fontmap[code] = path

    return fontmap


def make_output_path(codepoint):
    raw = hex(codepoint)[2:].upper()
    if (len(raw) % 2) != 0:
        raw = '0' + raw

    dir = ''

    while len(raw) > 2:
        dir += raw[:2] + '/'
        raw = raw[2:]

    if dir == '':
        dir = '00/'

    return dir, raw


def glyph_to_image(output_path, font_path, codepoint, with_metadata=True):
    print("Rendering %d -> %s" % (codepoint, output_path))

    target_size = (240, 150)
    display_size = (240, 240)

    size = target_size[1]

    # Load font with uharfbuzz to allow rendering specific font size
    # (The fonttools TTFont class doesn't seem to support this and draws at 1000px)
    blob = hb.Blob.from_file_path(font_path)
    face = hb.Face(blob)
    font = hb.Font(face) 
    font.scale = (size, size)

    # Draw glyph to pixel buffer
    buf = hb.Buffer()
    pen = FreeTypePen(None)
    #hb.shape(font, buf, {"kern": True, "liga": True})
    font.draw_glyph_with_pen(font.get_nominal_glyph(codepoint), pen)
    im = pen.image(width=target_size[0], height=target_size[1], contain=True)

    # Centre all glyphs horizontally, even if it's not technically correct
    bbox = im.getbbox()
    if bbox:
        im = im.crop((bbox[0], 0, bbox[2], im.size[1]))

    #im.thumbnail(target_size, resample=Image.ANTIALIAS)

    padded = Image.new(mode='RGBA', size=target_size, color=(255, 255, 255, 255)) # type: ignore
    padded.paste(im, ((target_size[0] - im.size[0]) // 2, 0), im.convert('RGBA'))
    im = padded

    # Change to white text on black background
    im = ImageOps.invert(im.convert('RGB'))


    # Add metadata text to image if enabled
    if with_metadata:
        im = render_metadata_text(im, codepoint, display_size)
    
    im.save(output_path)


def render_metadata_text(glyph_im, codepoint, size):
    im = Image.new(mode='RGB', size=size, color=(0, 0, 0))

    # Add the rendered glyph in the centre
    paste_offset = (abs(size[0] - glyph_im.size[0]) // 2, abs(size[1] - glyph_im.size[1]) // 2 + 7)
    im.paste(glyph_im, paste_offset)

    block_name, char_name = name_db.get(codepoint)
    code_display = 'U+%04X' % codepoint
    dec_display = '%d' % codepoint

    global text_pen
    im.paste(text_pen.render_line(block_name, width=size[0], size=14, color='white'), (0, 0))  # type: ignore
    im.paste(text_pen.render_line(char_name, width=size[0], size=14, color='white'), (0, 17))  # type: ignore

    line = text_pen.render_line(code_display, width=size[0], size=18, color='green', trim=True)
    offset = (0, size[1] - line.size[1])
    im.paste(line, offset)

    # TODO: Fix text not masked, so paste kills other text on same line
    line = text_pen.render_line(dec_display, width=size[0], size=18, color='blue', trim=True)
    offset = (size[0] - line.size[0], size[1] - line.size[1])
    im.paste(line, offset)

    return im


class TextLineRenderer:
    def __init__(self, font_path):
        self.blob = hb.Blob.from_file_path(font_path)
        self.face = hb.Face(self.blob)

    def render_line(self, text, width, size=18, color='white', trim=False) -> Image:
        """
        Render one line of text. Does not support line breaks or layout

        Args:
            text (str): Line of text to render
            width (int): Width in pixels to truncate output to
            size (int): Font height in pixels. This is also used as the canvas height.
            color (str|tuple): PIL compatible color value to color text after rendering
            trim (bool): If the resulting canvas should be cropped to only rendered pixels
        """
        canvas = (width, size)

        buf = hb.Buffer()
        buf.direction = 'ltr'
        buf.add_str(text)
        buf.guess_segment_properties()

        font = hb.Font(self.face)  # The font has to be loaded here for multiprocessing
        font.scale = (size, size)

        hb.shape(font, buf, {"kern": True, "liga": True})

        x, y = 0, 0
        pen = FreeTypePen(None)
        for info, pos in zip(buf.glyph_infos, buf.glyph_positions):
            gid = info.codepoint
            transformed = TransformPen(pen, Offset(x + pos.x_offset, y + pos.y_offset))
            font.draw_glyph_with_pen(gid, transformed)
            x += pos.x_advance
            y += pos.y_advance

        # Render text (black with alpha) and place on a white background
        rendered = pen.image(width=0, height=0, contain=True).convert('RGBA')
        im = Image.new(mode='RGBA', size=canvas, color=(0, 0, 0, 0))
        im.paste(color, (0, 0), mask=rendered)

        if trim:
            bbox = im.getbbox()
            if bbox:
                im = im.crop((bbox[0], 0, bbox[2], im.size[1]))

        return im


if __name__ == '__main__':

    parser = argparse.ArgumentParser(
        'render-codepoints',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="""Given a set of fonts, render every codepoint to an image file.

Images are stored in hierarchy of directories named using the bytes in a codepoint.
If a codepoint is in multiple fonts, the first font with that codepoint is used.
"""
    )

    parser.add_argument('-f', '--fonts', required=True, type=str, help="Directory of fonts to read from")
    parser.add_argument('-o', '--outdir', required=True, type=str, help='Directory to write rendered images to')
    parser.add_argument('--metadata-font', required=True, help='Path to font file for metadata rendering')
    parser.add_argument('--code-data', required=True, help='Path to UnicodeData.txt')
    parser.add_argument('--block-data', required=True, help='Path to Blocks.txt')

    parser.add_argument('--overwrite', action='store_true', help='Overwrite existing files instead of skipping them')
    parser.add_argument('--serial', action='store_true', help='Run jobs in a single process instead of multi-processing')

    args = parser.parse_args()

    text_pen = TextLineRenderer(args.metadata_font)

    from namedb import UnicodeNameDb
    name_db = UnicodeNameDb(args.code_data, args.block_data)

    os.makedirs(args.outdir, exist_ok=True)
    fontfiles = []
    fontfiles += glob.glob(os.path.join(args.fonts, '*.otf'))
    fontfiles += glob.glob(os.path.join(args.fonts, '*.ttf'))

    table = build_font_map(fontfiles)

    print('Found %d glyphs in %d fonts' % (len(table), len(fontfiles)))


    count = 0
    with ProcessPoolExecutor(max_workers=cpu_count()) as executor:
        for codepoint, font_path in table.items():

            dirname, filename = make_output_path(codepoint)

            # Ensure the folder hierarchy exists before submitting
            abs_path = os.path.join(args.outdir, dirname)
            output_path = os.path.join(abs_path, filename + '.png')
            os.makedirs(abs_path, exist_ok=True)

            # Skip existing
            if not args.overwrite and os.path.exists(output_path):
                continue

            fn_args = (output_path, font_path, codepoint)

            if args.serial:
                glyph_to_image(*fn_args)
            else:
                executor.submit(glyph_to_image, *fn_args)

            count += 1
            if count == 128:
                continue
                break

