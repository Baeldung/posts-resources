#!/bin/bash
# baeldung-generate-font-header.sh
#
# This script generates a C header file with a font bitmap array
# directly from the BDF font file "baeldung-font-9x16.bdf".
# It extracts the bitmap data for ASCII characters (0x20 through 0x7F)
# and fills all other entries (control characters and codes 0x80–0xFF) with zeros.
# Each ASCII glyph entry will include a comment with the corresponding character.
#
# Usage: ./baeldung-generate-font-header.sh

# Configuration
FONT_BDF="baeldung-font-9x16.bdf"
OUTPUT_HEADER="baeldung-font9x16.h"
GLYPH_WIDTH=9
GLYPH_HEIGHT=16

# Check if the BDF file exists
if [ ! -f "$FONT_BDF" ]; then
    echo "Error: BDF font file '$FONT_BDF' not found."
    exit 1
fi

TEMP_DATA="glyphs.dat"

echo "Extracting glyph bitmaps from BDF file..."
awk -v height="$GLYPH_HEIGHT" '
BEGIN {
    in_bitmap = 0;
    num_lines = 0;
}
{
    if ($1 == "STARTCHAR") {
        in_glyph = 1;
        num_lines = 0;
        delete bitmap;
    }
    if (in_glyph && $1 == "ENCODING") {
        enc = $2;
    }
    if ($1 == "BITMAP") {
        in_bitmap = 1;
        next;
    }
    if (in_bitmap && $1 ~ /^[0-9A-Fa-f]+$/) {
        bitmap[num_lines] = $1;
        num_lines++;
        next;
    }
    if ($1 == "ENDCHAR") {
        if (enc >= 32 && enc <= 127) {
            out = "";
            pad = height - num_lines;
            for (i = 0; i < pad; i++) {
                out = out "0x00,";
            }
            for (i = 0; i < num_lines; i++) {
                val = sprintf("%02X", strtonum("0x" bitmap[i]));
                out = out "0x" toupper(val) ",";
            }
            print enc ";" out;
        }
        in_bitmap = 0;
        in_glyph = 0;
        num_lines = 0;
        delete bitmap;
    }
}
' "$FONT_BDF" > "$TEMP_DATA"

# Helper function to generate a comma-separated list of zeros.
generate_zeros() {
    local count=$1
    local zeros=""
    for i in $(seq 1 "$count"); do
        zeros="${zeros}0x00,"
    done
    zeros=${zeros%,}
    echo "$zeros"
}

echo "Generating C header file..."
{
    echo "#ifndef FONT${GLYPH_WIDTH}X${GLYPH_HEIGHT}_H"
    echo "#define FONT${GLYPH_WIDTH}X${GLYPH_HEIGHT}_H"
    echo ""
    echo "#include <efi.h>"
    echo ""
    echo "// This is a minimal ${GLYPH_WIDTH}x${GLYPH_HEIGHT} font table for ASCII characters 0x20 through 0x7F."
    echo "// The undefined characters (outside that range) are filled with zeros."
    echo "UINT8 Font${GLYPH_WIDTH}X${GLYPH_HEIGHT}[256][${GLYPH_HEIGHT}] = {"
    echo ""

    # For control characters (0x00 - 0x1F)
    for code in $(seq 0 31); do
        hex_code=$(printf "0x%02X" "$code")
        zeros=$(generate_zeros "$GLYPH_HEIGHT")
        echo "    [${hex_code}] = { ${zeros} },"
    done

    # For ASCII characters 0x20 to 0x7F, include comments with the letter.
    while IFS=";" read -r enc bitmap; do
        code=$enc
        hex_code=$(printf "0x%02X" "$code")
        if [ "$code" -eq 32 ]; then
            char="space"
        elif [ "$code" -eq 127 ]; then
            char="DEL"
        else
            # Get the character using printf (some shells require this double evaluation)
            char=$(printf "\\$(printf '%03o' "$code")")
        fi
        # Remove trailing comma from bitmap string.
        bitmap_clean=${bitmap%,}
        echo "    [${hex_code}] = { ${bitmap_clean} }, // '$char'"
    done < "$TEMP_DATA"

    # For codes 0x80 to 0xFF, fill with zeros.
    for code in $(seq 128 255); do
        hex_code=$(printf "0x%02X" "$code")
        zeros=$(generate_zeros "$GLYPH_HEIGHT")
        echo "    [${hex_code}] = { ${zeros} },"
    done

    echo "};"
    echo ""
    echo "#endif // FONT${GLYPH_WIDTH}X${GLYPH_HEIGHT}_H"
} > "$OUTPUT_HEADER"

rm "$TEMP_DATA"

echo "Header file generated: $OUTPUT_HEADER"

