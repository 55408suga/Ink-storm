#!/usr/bin/env bash
# Extract 6 evenly-spaced keyframe PNGs and a 3x2 contact_sheet.png from a demo video.
set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <input.mp4> <output_dir> [count=6]" >&2
    exit 2
fi

INPUT="$1"
OUTDIR="$2"
COUNT="${3:-6}"

if [[ ! -f "$INPUT" ]]; then
    echo "ERROR: input video not found: $INPUT" >&2
    exit 1
fi

FFMPEG="${FFMPEG:-$(command -v ffmpeg || true)}"
FFPROBE="${FFPROBE:-$(command -v ffprobe || true)}"
if [[ -z "$FFMPEG" || -z "$FFPROBE" ]]; then
    echo "ERROR: ffmpeg and ffprobe are required." >&2
    exit 1
fi

mkdir -p "$OUTDIR"
DURATION="$("$FFPROBE" -v error -show_entries format=duration -of default=nw=1:nk=1 "$INPUT")"
if [[ -z "$DURATION" || "$DURATION" == "N/A" ]]; then
    echo "ERROR: could not read duration from $INPUT" >&2
    exit 1
fi

for i in $(seq 1 "$COUNT"); do
    TS="$(python3 -c "d=float('$DURATION'); n=int('$COUNT'); i=int('$i'); print(f'{d * (i - 0.5) / n:.3f}')")"
    OUT="$(printf '%s/snap_%02d.png' "$OUTDIR" "$i")"
    "$FFMPEG" -y -loglevel error -ss "$TS" -i "$INPUT" -frames:v 1 "$OUT"
done

"$FFMPEG" -y -loglevel error -pattern_type glob -i "$OUTDIR/snap_*.png" \
    -vf "scale=640:-1,tile=3x2:padding=6:margin=6:color=0x111118" \
    "$OUTDIR/contact_sheet.png"

echo "Wrote $COUNT snapshots and $OUTDIR/contact_sheet.png (duration=${DURATION}s)"
