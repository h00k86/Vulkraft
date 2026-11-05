#!/bin/sh
# Run from the repository root; pass the installed msdf-atlas-gen executable.
set -eu
"${1:-msdf-atlas-gen}" -font assets/fonts/OpenSans-Italic.ttf \
  -chars '[0x20,0x7e],[0xa0,0x17f],0x20ac,0xfffd' \
  -type msdf -dimensions 1024 1024 -size 64 -pxrange 8 \
  -imageout assets/fonts/font_utf8.png -json assets/fonts/font_utf8.json
