#!/usr/bin/env python3

from fontTools import ttLib
from string import ascii_lowercase

import argparse
import glob
import itertools
import math
import os
import subprocess
import sys


def iter_ligatures(font: ttLib.TTFont):
    """
    Iterate ligature sequences supported by a font
    (Effectively lists all emoji ZWJ sequences the font encodes)
    """
    if not font.has_key('GSUB'):
        return

    for lookup in font['GSUB'].table.LookupList.Lookup:
        if lookup.LookupType == 4:
            for ligatureSubst in lookup.iterSubTables():
                for glyph, ligatureSets in ligatureSubst.value.ligatures.items():
                    for ligset in ligatureSets:
                        seq = [glyph]
                        seq.extend(ligset.Component)

                        yield ligset.LigGlyph, seq


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('fontfile', help='Input font to split')
    parser.add_argument('outputdir', help='Output directory. A suffix will be added to the input name for each split file')

    parser.add_argument('--file-count', type=int, help='Split the font evenly into N files')
    parser.add_argument('--glyph-count', type=int, help='Split the font so there are a maximum of N glyphs in each font')

    args = parser.parse_args()

    if not (bool(args.file_count) ^ bool(args.glyph_count)):
        print("Please supply either --file-count=N or --glyph-count=N to split the font")
        sys.exit(1)


    font = ttLib.TTFont(args.fontfile)
    all_glyphs = font.getGlyphNames()
    zwj_glyph = font.getBestCmap().get(0x200D)

    related_glyph_pools = []

    # Glyphs in substitutions need to be included in the same font file
    for glyphid, sequence in iter_ligatures(font):
        related = set(sequence + [glyphid])

        # Ignore ZWJ characters when grouping
        related.discard(zwj_glyph)

        overlap = set()

        for index, pool in enumerate(related_glyph_pools):
            for glyph in related:
                if glyph in pool:
                    overlap.add(index)

        if overlap:
            merged = set()

            for pool in [related_glyph_pools[i] for i in overlap]:
                merged.update(pool)
                related_glyph_pools.remove(pool)

            related_glyph_pools.append(merged)

        else:
            related_glyph_pools.append(related)

    # Ensure all pools contain the ZWJ glyph, which is needed for all emoji ligatures
    if zwj_glyph is not None:
        for pool in related_glyph_pools:
            pool.add(zwj_glyph)


    if args.file_count:
        per_file = math.ceil(len(all_glyphs) / args.file_count)
    else:
        per_file = args.glyph_count

    output_count = math.ceil(len(all_glyphs) / per_file)
    print("Font has %d glyphs. Will use %d glyphs per file = %d files" % (len(all_glyphs), per_file, output_count))

    base_name, extension = os.path.splitext(os.path.basename(args.fontfile))
    part_num = 0

    # Remove any existing parts so there's no mix-up with different split outputs
    existing = glob.glob(os.path.join(args.outputdir, f'{base_name}-p*{extension}'))
    if existing:
        print("Removing existing parts for this font...")
        for path in existing:
            os.unlink(path)

    filename_pad = int(math.ceil(math.log10(output_count)))

    for i in range(0, len(all_glyphs), per_file):
        part_str = str(part_num).zfill(filename_pad)
        output_name = f'{base_name}-p{part_str}{extension}'

        print("Processing %s" % output_name)

        subset = all_glyphs[i:i + per_file]
        dependencies = set()

        for glyphid in subset:
            for index, pool in enumerate(related_glyph_pools):
                if glyphid in pool:
                    dependencies.add(index)

        if dependencies:
            for index in dependencies:
                subset.extend(related_glyph_pools[index])

            print("  Added %d extra glyphs in to prevent loss of ligatures" % (len(subset) - per_file))

        cmd = [
            'pyftsubset', args.fontfile,
            '--output-file=%s' % os.path.join(args.outputdir, output_name),
            '--glyphs=%s' % ','.join(subset),
        ]

        subprocess.check_call(cmd)

        part_num += 1