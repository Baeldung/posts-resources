#!/bin/bash

# Note: Ensure you have sufficient permissions to read from and write to the framebuffer device.
# This script directly interacts with the hardware and may make your system unstable. Use with caution.

FBDEVICE=/dev/fb0

# Check if framebuffer device exists and is writable
if [ ! -w "$FBDEVICE" ]; then
  echo "Cannot write to $FBDEVICE. Try to execute $0 with root permissions."
  exit 1
fi

# Get screen size and bits per pixel
WIDTH=$(cat /sys/class/graphics/fb0/virtual_size | cut -d"," -f1)
HEIGHT=$(cat /sys/class/graphics/fb0/virtual_size | cut -d"," -f2)

# Define colors in BGRA format (adjust these values if your framebuffer format differs)
LIGHT_YELLOW="\xff\xda\xba\x2d"  # Light yellow color
DARK_BROWN="\xff\x33\x1a\x0b"    # Dark brown color

# Create checkerboard pattern
# Define the size of each square and the number of squares
SQUARE_SIZE=50
BOARD_SIZE=8

# Calculate the total size of the board
TOTAL_SIZE=$((SQUARE_SIZE * BOARD_SIZE))

# Calculate the starting position to center the board
START_X=$(( (WIDTH - TOTAL_SIZE) / 2 ))
START_Y=$(( (HEIGHT - TOTAL_SIZE) / 2 ))

# Loop over each pixel and set the color
for ((y=0; y<HEIGHT; y++)); do
    for ((x=0; x<WIDTH; x++)); do
        # Check if the current pixel is within the bounds of the board
        if [ $x -ge $START_X ] && [ $x -lt $(($START_X + TOTAL_SIZE)) ] &&
           [ $y -ge $START_Y ] && [ $y -lt $(($START_Y + TOTAL_SIZE)) ]; then
            # Calculate the square indices
            SQUARE_X=$(( (x - START_X) / SQUARE_SIZE ))
            SQUARE_Y=$(( (y - START_Y) / SQUARE_SIZE ))
            # Determine the color based on the square
            if [ $(( (SQUARE_X + SQUARE_Y) % 2 )) -eq 0 ]; then
                COLOR=$LIGHT_YELLOW
            else
                COLOR=$DARK_BROWN
            fi
        else
            # Outside the board, set to black
            COLOR="\x00\x00\x00\x00"
        fi
        # Write the color to the framebuffer
        echo -en $COLOR
    done
done > $FBDEVICE

# Wait for 10 seconds
sleep 10

# Clear the screen and restore the terminal
clear


