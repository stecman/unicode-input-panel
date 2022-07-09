import re
import difflib

class UnicodeNameDb:
    def __init__(self, unicode_data_path, unicode_blocks_path):
        self.unicode_data_path = unicode_data_path
        self.unicode_blocks_path = unicode_blocks_path
        self.has_loaded = False

        self.blocks = None
        self.blocks_last_codepoint = None

        self.codepoints = {}

    def get(self, codepoint):
        """
        Get the block name and shortened character name for a codepoint
        Returns (block_name: str, codepoint_name: str)
        """
        if not self.has_loaded:
            self.load()

        if 0xE000 <= codepoint <= 0xF8FF:
            return "Private Use Area", "[Not assigned by Unicode]"

        if 0xF0000 <= codepoint <= 0xFFFFF:
            return "Supl. Private Use Area A", "[Not assigned by Unicode]"

        if 0x100000 <= codepoint <= 0x10FFFF:
            return "Supl. Private Use Area B", "[Not assigned by Unicode]"

        try:
            return self.codepoints[codepoint]
        except KeyError:
            return "NOT REGISTERED", "Codepoint %X" % codepoint

    def load(self):
        self.has_loaded = True

        block_index = 0
        blocks = []

        # Read block range names
        with open(self.unicode_blocks_path, 'r') as handle:
            for line in handle:
                if line.startswith('#') or line.strip() == "":
                    continue

                match = re.match(r'^([0-9A-Fa-f]+)\.\.([0-9A-Fa-f]+);\W*(.*)$', line)
                start, end, name = match.group(1, 2, 3)  # type: ignore
                blocks.append((int(start, 16), int(end, 16), name))

        # Store the max codepoint defined by the blocks list
        self.blocks_last_codepoint = blocks[-1][1]
        self.blocks = blocks

        # Read codepoint names
        with open(self.unicode_data_path, 'r') as handle:
            for line in handle:
                fields = line.split(';')

                codepoint = int(fields[0], 16)
                name = fields[1]

                if name == '<control>':
                    name = fields[10]

                elif name.endswith('>'):
                    # Skip markers that aren't actually codepoint names
                    continue

                # Move to the next block if our codepoint is past the end of this block
                while blocks[block_index][1] < codepoint:
                    block_index += 1

                group_name = blocks[block_index][2]


                # Try to shorten the character name if it repeats the group name
                matcher = difflib.SequenceMatcher(None, group_name.lower(), name.lower())
                pos_a, pos_b, size = matcher.find_longest_match(0, len(group_name), 0, len(name))

                if size >= 3 and pos_b == 0:
                    words_a = group_name[pos_a:].lower().split(" ")
                    words_b = name.lower().split(" ")
                    trim_chars = 0

                    for a, b in zip(words_a, words_b):
                        if a == b or a == (b + 's') or a == (b + "-1"):
                            # This assumes there are single spaces, but should be ok...
                            trim_chars += len(b) + 1

                    short_name = name[trim_chars:]

                    # Fix cases like "alchemical symbols" where the naming scheme is "[block name] FOR XYZ"
                    # These names become "FOR XYZ", which is a bit awkward, so just drop the leading 'for'.
                    if short_name.startswith('FOR '):
                        short_name = short_name[4:]

                else:
                    short_name = name

                # Shorten specific words
                group_name = group_name.replace('Miscellaneous', 'Misc.')

                self.codepoints[codepoint] = (group_name, short_name or "")