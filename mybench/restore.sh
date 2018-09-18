#!/bin/sh

for f in .*.bak
do
    [ "$f" = ".*.bak" ] && continue
    bak="$f"
    f="${f#.}"
    f="${f%.bak}"
    echo "$bak -> $f"
    mv "$bak" "$f"
done
