#!/usr/bin/env python3

import argparse
import os
import re
import sys
import typing

def convert_to_c(blocks_txt_path: str, output: typing.IO, includes: list[str]):
    """
    Convert the Unicode Blocks.txt file to C code

    Code is returned as a tuple of (implementation)
    """

    for path in includes:
        output.write('#include "%s"\n' % path)

        output.write("""
//
// This is a generated file. See ./scripts/generate-unicode-data.py
//

""")

    output.write("const struct UnicodeNamedBlock uc_blocks_table[] = {\n")

    block_count = 0
    string_bytes = 0

    last_end = None

    with open(blocks_txt_path, 'r') as handle:
        for line in handle:
            if line.startswith('#') or line.strip() == "":
                continue

            match = re.match(r'^([0-9A-Fa-f]+)\.\.([0-9A-Fa-f]+);\W*(.*)$', line)
            output.write('    { 0x%s, 0x%s, "%s" },\n' % match.group(1, 2, 3) )

            block_count += 1
            string_bytes += len(match.group(3)) + 1 # Plus one for NULL terminator

    output.write("};\n\n")

    output.write("const uint16_t uc_blocks_length = %d;\n\n" % block_count)

    # Combined size is codepoints and name strings
    total_size = string_bytes + (block_count * 4 * 2)

    return block_count, total_size


def _struct_entry():
    return


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('datafile', help="Blocks.txt file from unicode.org")
    parser.add_argument('outfile', help='Path to write the generated source to')
    parser.add_argument('--include', action='append', help='Header to #include in the generated source')

    args = parser.parse_args()


    if args.outfile == '-':
        handle = sys.stdout
    else:
        handle = open(args.outfile, 'w')

    stats = convert_to_c(blocks_txt_path=args.datafile, output=handle, includes=args.include)

    # Output stats
    sys.stderr.write("Found %d named unicode blocks using %s bytes" % stats)