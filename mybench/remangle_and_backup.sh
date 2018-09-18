#!/bin/sh

for f in $(grep -Rl product_emptiness_check)
do
    [ "$f" = "remangle_and_backup.sh" ] && continue
    [ "$f" = "bench.py" ] && continue
    cp "$f" ".$f.bak"
    cat "$f" | ../product_remangler | sed "s+/home/clement+$HOME+g" \
                                    | sed "s+/work/cgillard+$HOME+g" > tmp
    mv tmp "$f"
    echo "$f"
done
