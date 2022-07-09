#!/bin/bash

python3 scripts/render-codepoints.py -f data/fonts/ -o /tmp/fonts --metadata-font data/fonts/NotoSansMono-Regular.otf --code-data data/UnicodeData.txt --block-data data/Blocks.txt $@
