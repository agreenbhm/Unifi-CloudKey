#!/bin/bash
set -euo pipefail

# Always patch the JAR from this exact package, rather than replacing it with
# application classes and a dependency manifest from another UniFi release.
if [ $# -ne 1 ]; then
    echo "Usage: $0 <path_to_dpkg_file>"
    exit 1
fi

DPKG_FILE="$1"
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

# Extract the dpkg file
dpkg-deb -R "$DPKG_FILE" "$TMP_DIR"

# Remove the preinst script if it exists
if [ -f "$TMP_DIR/DEBIAN/preinst" ]; then
    rm "$TMP_DIR/DEBIAN/preinst"
    echo "Removed preinst script."
else
    echo "No preinst script found."
fi

# Patch only the architecture rejection in the package's own launcher class.
ACE_JAR_PATH="$TMP_DIR/usr/lib/unifi/lib/ace.jar"
PATCH_CLASSES=$(mktemp -d)
trap 'rm -rf "$TMP_DIR" "$PATCH_CLASSES"' EXIT
ASM_EXPORTS=(--add-exports java.base/jdk.internal.org.objectweb.asm=ALL-UNNAMED
    --add-exports java.base/jdk.internal.org.objectweb.asm.tree=ALL-UNNAMED)
javac "${ASM_EXPORTS[@]}" -d "$PATCH_CLASSES" "$SCRIPT_DIR/tools/PatchUnifi.java"
java "${ASM_EXPORTS[@]}" -cp "$PATCH_CLASSES" PatchUnifi "$ACE_JAR_PATH" "$TMP_DIR/ace-patched.jar"
mv "$TMP_DIR/ace-patched.jar" "$ACE_JAR_PATH"
if [ -f "$TMP_DIR/DEBIAN/md5sums" ]; then
    ACE_MD5=$(md5sum "$ACE_JAR_PATH" | cut -d ' ' -f 1)
    awk -v checksum="$ACE_MD5" '$2 == "usr/lib/unifi/lib/ace.jar" { print checksum "  " $2; next } { print }' \
        "$TMP_DIR/DEBIAN/md5sums" > "$TMP_DIR/DEBIAN/md5sums.updated"
    mv "$TMP_DIR/DEBIAN/md5sums.updated" "$TMP_DIR/DEBIAN/md5sums"
fi

# Repackage the dpkg file
NEW_DPKG_FILE="${DPKG_FILE%.deb}-modified.deb"
dpkg-deb -b "$TMP_DIR" "$NEW_DPKG_FILE"

# Install the new dpkg file
#sudo dpkg -i "$NEW_DPKG_FILE"

echo "Modified dpkg file: $NEW_DPKG_FILE"
