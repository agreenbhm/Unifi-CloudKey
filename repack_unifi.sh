#!/bin/bash

# Check if the dpkg file and ace.jar file are provided as arguments
if [ $# -ne 2 ]; then
    echo "Usage: $0 <path_to_dpkg_file> <path_to_ace.jar>"
    exit 1
fi

DPKG_FILE="$1"
ACE_JAR="$2"
TMP_DIR=$(mktemp -d)

# Extract the dpkg file
dpkg-deb -R "$DPKG_FILE" "$TMP_DIR"

# Remove the preinst script if it exists
if [ -f "$TMP_DIR/DEBIAN/preinst" ]; then
    rm "$TMP_DIR/DEBIAN/preinst"
    echo "Removed preinst script."
else
    echo "No preinst script found."
fi

# Overwrite the ace.jar file in the package
ACE_JAR_PATH="$TMP_DIR/usr/lib/unifi/lib/ace.jar"
if [ -f "$ACE_JAR" ]; then
    cp "$ACE_JAR" "$ACE_JAR_PATH"
    echo "Replaced ace.jar in the package."
else
    echo "Error: ace.jar not found. Exiting."
    rm -rf "$TMP_DIR"
    exit 1
fi

# Repackage the dpkg file
NEW_DPKG_FILE="${DPKG_FILE%.deb}-modified.deb"
dpkg-deb -b "$TMP_DIR" "$NEW_DPKG_FILE"

# Clean up temporary directory
rm -rf "$TMP_DIR"

# Install the new dpkg file
#sudo dpkg -i "$NEW_DPKG_FILE"

echo "Modified dpkg file: $NEW_DPKG_FILE"
