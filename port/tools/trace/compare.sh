#!/bin/sh
# Plays the runs in runs.txt with this tree's game_trace and compares each with the reference
# (golden.txt: a hash of each run's lines, made at the commit in REFERENCE's first line, with the
# commits on its next lines picked onto it). On a difference it
# builds the reference commit in a temporary worktree, plays the run there too and prints the
# first frame that differs, both lines.
#
# usage: tools/trace/compare.sh [-quick] <rom> [build dir, default build] [run names...]
#        tools/trace/compare.sh -golden <rom> [build dir]   (writes golden.txt from this tree)
# -quick: only the runs marked quick in runs.txt (5 of them, ~40 s: a check after each step;
# all of them at the end of a piece of work).
set -e
here=$(cd "$(dirname "$0")" && pwd)
port=$(cd "$here/../.." && pwd)
golden=0
quick=0
if [ "$1" = "-golden" ]; then golden=1; shift; fi
if [ "$1" = "-quick" ]; then quick=1; shift; fi
rom=$1; shift || true
[ -n "$rom" ] || { echo "usage: $0 [-golden] <rom> [build dir] [runs...]"; exit 2; }
build=${1:-$port/build}; [ $# -gt 0 ] && shift
only="$*"
trace=$build/game_trace
[ -x "$trace" ] || { echo "no $trace (build it first)"; exit 2; }
out=$(mktemp -d)
fail=0
refbuilt=""
[ $golden = 1 ] && : > "$here/golden.txt"
grep -v '^#' "$here/runs.txt" | while read -r name frames args; do
	[ -n "$name" ] || continue
	case "$args" in *quick*) ;; *) [ $quick = 1 ] && continue ;; esac
	args=$(echo "$args" | sed 's/ *quick//')
	if [ -n "$only" ] && ! echo " $only " | grep -q " $name "; then continue; fi
	"$trace" "$rom" "$frames" $args > "$out/$name.txt"
	sum=$(sha256sum < "$out/$name.txt" | cut -c1-64)
	if [ $golden = 1 ]; then
		echo "$name $sum" >> "$here/golden.txt"
		echo "$name: $frames frames"
		continue
	fi
	want=$(awk -v n="$name" '$1 == n { print $2 }' "$here/golden.txt")
	if [ "$sum" = "$want" ]; then echo "$name: same ($frames frames)"; continue; fi
	echo "$name: DIFFERENT"
	if [ -z "$refbuilt" ]; then
		ref=$(head -n 1 "$here/REFERENCE")
		git -C "$port" worktree add -q --detach "$out/ref" "$ref"
		for c in $(tail -n +2 "$here/REFERENCE"); do git -C "$out/ref" cherry-pick -n "$c" > /dev/null; done
		cmake -S "$out/ref/port" -B "$out/refbuild" -DCMAKE_BUILD_TYPE=Release -DRR_VIEWER=OFF > /dev/null
		cmake --build "$out/refbuild" --target game_trace -j4 > /dev/null
		refbuilt=1
	fi
	"$out/refbuild/game_trace" "$rom" "$frames" $args > "$out/$name.ref.txt"
	diff "$out/$name.ref.txt" "$out/$name.txt" | head -4 | cut -c1-2000
	echo "$name" >> "$out/failed"
done
if [ -d "$out/ref" ]; then git -C "$port" worktree remove --force "$out/ref"; fi
if [ -f "$out/failed" ]; then echo "differs: $(tr '\n' ' ' < "$out/failed")"; rm -rf "$out"; exit 1; fi
rm -rf "$out"
