#!/bin/bash
# Instala Vocodex Pro (no toca Voxora) y quita el bloqueo de macOS.
cd "$(dirname "$0")"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

rm -rf "$VST3_DIR/Vocodex Pro.vst3" "$AU_DIR/Vocodex Pro.component"
cp -R "Vocodex Pro.vst3" "$VST3_DIR/"
cp -R "Vocodex Pro.component" "$AU_DIR/"

xattr -cr "$VST3_DIR/Vocodex Pro.vst3" "$AU_DIR/Vocodex Pro.component"
codesign --force --deep -s - "$VST3_DIR/Vocodex Pro.vst3" "$AU_DIR/Vocodex Pro.component" 2>/dev/null

echo ""
echo "Vocodex Pro instalado."
echo "Abre FL Studio > Options > Manage plugins > Find installed plugins."
echo "Puedes cerrar esta ventana."
