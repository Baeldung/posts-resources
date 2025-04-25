#!/bin/bash

# Exit immediately if a command fails,
# treat unset variables as errors,
# and ensure any failure in a pipeline is caught
set -euo pipefail

# prepare-usb.sh — make a UEFI-bootable USB stick from a gnu-efi build
#
# Usage: ./prepare-usb.sh /dev/sdX path/to/your.efi
#
# WARNING: All data on the specified device will be destroyed.

# 0. Verify current directory
if [[ "$(basename "$PWD")" != "gnu-efi" ]]; then
  echo "Error: Please run this script from the 'gnu-efi' directory." >&2
  exit 1
fi

# 1. Check for device and EFI-file arguments
if [[ -z "${1:-}" || -z "${2:-}" ]]; then
  echo "Usage: $0 /dev/sdX path/to/EFI-file.efi" >&2
  exit 1
fi

DEVICE="$1"
EFI_FILE="$2"

# 1b. Verify that the EFI file exists
if [[ ! -f "$EFI_FILE" ]]; then
  echo "Error: EFI file '$EFI_FILE' not found." >&2
  exit 1
fi

# 1c. Compute partition name: add 'p' before number if device name ends with a digit
if [[ "$DEVICE" =~ [0-9]$ ]]; then
  EFI_PART="${DEVICE}p1"
else
  EFI_PART="${DEVICE}1"
fi

# 1d. Verify no partitions on the device are mounted
echo "→ Checking for mounted partitions on ${DEVICE}..."
if mount | grep -qE "^${DEVICE}[0-9]+"; then
  echo "Error: one or more partitions on ${DEVICE} are currently mounted. Please unmount them before proceeding." >&2
  exit 1
fi

# 2. Confirm destructive action
echo "WARNING: This will permanently erase all data on ${DEVICE}."
read -rp "Are you sure you want to continue? [y/N] " CONFIRM
if [[ ! "$CONFIRM" =~ ^[Yy]$ ]]; then
  echo "Aborted."
  exit 0
fi

echo "→ Preparing ${DEVICE}..."

# 3. Create GPT table + 100 MiB FAT32 EFI partition
sudo parted "$DEVICE" --script \
  mklabel gpt \
  mkpart primary fat32 1MiB 100MiB \
  set 1 boot on

# 3b. Force kernel to re-read partition table
sudo partprobe "$DEVICE"
sleep 1

# 4. Format as FAT32
sudo mkfs.fat -F32 "$EFI_PART"

# 5. Mount, copy EFI payload, cleanup
TMPDIR=$(mktemp -d)
sudo mount "$EFI_PART" "$TMPDIR"
sudo mkdir -p "$TMPDIR/EFI/BOOT"
sudo cp "$EFI_FILE" "$TMPDIR/EFI/BOOT/BOOTX64.EFI"
sudo umount "$TMPDIR"
rmdir "$TMPDIR"

echo "✔ USB stick (${DEVICE}) is now UEFI-bootable!"

