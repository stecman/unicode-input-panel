#!/usr/bin/env python3

import argparse
import sys

from namedb import UnicodeNameDb


def build_name_tables(codepoint_name_map):
    """
    Build table to find and construct codepoint names

    This is lightly compressed by storing common prefixes in a second table.
    The codepoint for each string is inferred by index rather than storing
    each 32-bit codepoint in each name in the table. Gaps in the sequence
    of codepoints are stored in a third table.

    Returns several tables:

        name_table: A list of dicts of {
            "prefix_index": <index-into-prefix-table>,
            "name": <string>
        }

        prefix_table: A list of common prefix strings

        sequence_table: A list of dicts of {
            "codepoint": <uint32>,
            "name_index": <index-into-name-table>
        }
    """
    LOOK_BACK_COUNT = 32

    builder_table = []

    for codepoint, name in codepoint_name_map.items():
        current = {
            "codepoint": codepoint,
            "name": name,
            "prefix": None,
        }

        if builder_table:
            found = {}

            for index, other in enumerate(reversed(builder_table)):
                if index >= LOOK_BACK_COUNT:
                    break

                prefix = find_common_prefix(current['name'], other['name'])

                if prefix:
                    if prefix in found:
                        found[prefix].append(other)
                    else:
                        found[prefix] = [other]


            if found:
                # Sort most-common to least-common to prefer re-use over other metrics
                ordered = list(sorted(found.items(), reverse=True, key=rank_prefix_pair))

                apply_prefix, apply_to = ordered[0]
                apply_to.append(current)

                for item in apply_to:
                    item['prefix'] = apply_prefix

        builder_table.append(current)

    # Build prefix table
    # This could be processed further to combine similar prefixes in the set (from outside
    # the look-back window), but the compression ratio is already "good enough" for us.
    prefixes = set([item['prefix'] for item in builder_table if item['prefix'] is not None])
    prefix_table = list(prefixes)
    prefix_table.sort()
    prefix_index = {p: i for i, p in enumerate(prefix_table)}

    # Build name and sequence jump tables
    name_table = []
    sequence_jumps = []

    previous_codepoint = None

    for index, item in enumerate(builder_table):
        if item['prefix']:
            name_table.append({
                "name": item['name'][len(item['prefix']):],
                "prefix_index": prefix_index[item['prefix']],
            })
        else:
            name_table.append({
                "name": item['name'],
                "prefix_index": None,
            })

        codepoint = item['codepoint']

        if (codepoint - 1) != previous_codepoint:
            sequence_jumps.append({
                "codepoint": codepoint,
                "name_index": index,
            })

        previous_codepoint = codepoint

    return name_table, prefix_table, sequence_jumps


def rank_prefix_pair(item):
    """
    Give a score for sorting possible prefixes
    Weighted towards re-using the most common match
    """
    prefix, items = item
    return len(prefix) * (len(items) ** 2)


def find_common_prefix(a, b, min_length=3):
    """
    Find a common prefix in A and B that ends at a word boundary
    """
    if a == "" or b == "":
        return None

    i = 0
    shortest = min(len(a), len(b))
    while i < shortest and a[i] == b[i]:
        i += 1

    # Break at spaces (don't allow partial word prefixes)
    word_boundary = a[:i].rfind(" ")

    # Also allow breaking at hyphens
    if word_boundary == -1:
        word_boundary = a[:i].rfind("-")

    # Include the word-boundary in the prefix
    word_boundary += 1

    if word_boundary >= min_length:
        return a[:word_boundary]
    else:
        return None


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--blocks', required=True, help='Path to Blocks.txt')
    parser.add_argument('--codepoints', required=True, help='Path to UnicodeData.txt')
    parser.add_argument('--include', required=True, help='Filename/path to #include in the generated code')

    parser.add_argument('--output', '-o', help='Output file for generated code')

    args = parser.parse_args()

    # Open the output first so we can exit early on failure
    if args.output == '-':
        handle = sys.stdout
    else:
        handle = open(args.output, 'w')

    # Load Unicode metadata
    #
    # This provides codepoint and block names with prefix redundancy stripped out:
    # eg. "EGYPTIAN HIEROGLYPH A001" (U+13000) becomes "A001", as the  block name that
    # codepoint resides in is already called "Egyptian Hieroglyphs". This is done to
    # minimise the amount of informational text we need to fit on the tiny screen.
    name_db = UnicodeNameDb(args.codepoints, args.blocks)
    name_db.load()

    # Build tables for codepoint name lookup
    # This technically already fits on the RP2040's 2MB flash as raw strings
    # at ~621KB but sharing prefixes reduces the storage required to ~181KB.
    name_table, prefix_table, sequence_table = build_name_tables(
        {cp: names[1] for cp, names in name_db.codepoints.items()}
    )

    max_name_length = max([len(names[1]) for names in name_db.codepoints.values()])

    # Build list of (slightly modified) block names
    blocks_table = []

    for start, end, name in name_db.blocks:
        name = name.replace('Miscellaneous', 'Misc.')

        blocks_table.append({
            "start": start,
            "end": end,
            "name": name
        })

    # Generate code
    handle.write('#include "%s"\n' % args.include)
    handle.write('#include <string.h>\n')
    handle.write('\n')

    handle.write('#ifdef __cplusplus\nextern "C" {\n#endif\n\n')

    # Reserve a buffer for concatenating prefix and name to return one string
    buffer_size = max_name_length + 2
    handle.write('char _uc_name_buffer[%d];\n' % buffer_size)
    handle.write('char* uc_name_buffer = (char*) &_uc_name_buffer;\n')
    handle.write('const uint16_t uc_name_buffer_len = %d;\n\n' % buffer_size)

    ##

    handle.write('const uint32_t uc_blocks_len = %d;\n' % len(blocks_table))
    handle.write('const struct UCBlockRange uc_blocks[] = {\n')

    for row in blocks_table:
        handle.write('    { 0x%X, 0x%X, "%s" },\n' % (
            row['start'],
            row['end'],
            row['name'],
        ))

    handle.write('};\n\n')

    ##

    handle.write('const uint32_t uc_name_indices_len = %d;\n' % len(sequence_table))
    handle.write('const struct UCIndex uc_name_indices[] = {\n')

    for row in sequence_table:
        handle.write('    { 0x%X, %d },\n' % (
            row['codepoint'],
            row['name_index'],
        ))

    handle.write('};\n\n')

    ##

    handle.write('const uint32_t uc_name_prefixes_len = %d;\n' % len(prefix_table))
    handle.write('const char* uc_name_prefixes[] = {\n')

    for name in prefix_table:
        handle.write('    "%s",\n' % name)

    handle.write('};\n\n')

    ##

    handle.write('const uint32_t uc_names_len = %d;\n' % len(name_table))
    handle.write('const struct UCPrefixedString uc_names[] = {\n')

    for row in name_table:
        if row['prefix_index'] is None:
            index_value = '0xFFFF'
        else:
            index_value = row['prefix_index']

        if row['name']:
            string_value = '"%s"' % row['name']
        else:
            string_value = 'NULL'

        handle.write('    { %s, %s },\n' % (
            index_value,
            string_value,
        ))

    handle.write('};\n\n')

    handle.write('#ifdef __cplusplus\n}\n#endif\n\n')

    handle.close()