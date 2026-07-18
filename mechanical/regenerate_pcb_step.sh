#!/usr/bin/env bash
# Regenerate mechanical/orrery_v1_pcb.step from the KiCad board.
# Run this whenever the PCB layout changes and you want the STEP model
# in the FreeCAD assembly to match.
#
# Usage: ./mechanical/regenerate_pcb_step.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KICAD_CLI="/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"

if [[ ! -x "$KICAD_CLI" ]]; then
  echo "kicad-cli not found at $KICAD_CLI — install KiCad or edit this script"
  exit 1
fi

"$KICAD_CLI" pcb export step \
  --force --drill-origin \
  -o "$REPO_ROOT/mechanical/orrery_v1_pcb.step" \
  "$REPO_ROOT/hardware/orrery_v1/orrery_v1.kicad_pcb"

echo "Wrote $REPO_ROOT/mechanical/orrery_v1_pcb.step"
