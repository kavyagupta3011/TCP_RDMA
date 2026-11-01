#!/bin/bash
# One-time setup on the cluster login node, in ~/IMT2023016/run_updated:
#   bash setup.sh
# 1. removes Windows line endings from the scripts and code
# 2. builds the programs of all three versions (code/v1, code/v2, code/v3)
# 3. creates ~/IMT2023016/results_updated/{floor,v1,v2,v3}/logs (an older one is renamed first)
#    and makes the folders writable for the job (the qsub wrapper runs jobs as student3)

R=/home/student4/IMT2023016/run_updated
OUT=/home/student4/IMT2023016/results_updated
cd "$R" || { echo "missing $R"; exit 1; }

find "$R" -type f \( -name '*.sh' -o -name '*.pbs' -o -name '*.py' -o -name '*.c' -o -name '*.h' -o -name Makefile \) \
    -exec sed -i 's/\r$//' {} +

fail=0
for v in v1 v2 v3; do
    [ -d "code/$v" ] || { echo "!! missing code/$v"; fail=1; continue; }
    for d in code/$v/*/; do
        [ -f "$d/Makefile" ] || continue
        if (cd "$d" && make -B > build.out 2>&1); then
            echo "built   $v/$(basename "$d")"
        else
            echo "FAILED  $v/$(basename "$d")"; cat "$d/build.out"; fail=1
        fi
        rm -f "$d/build.out"
    done
done

if [ -d "$OUT" ] && [ -n "$(find "$OUT" -type f 2>/dev/null | head -1)" ]; then
    old="${OUT}_old_$(date +%Y%m%d_%H%M%S)"
    mv "$OUT" "$old" && echo "previous results moved to $old"
fi
mkdir -p "$OUT"/floor/logs "$OUT"/v1/logs "$OUT"/v2/logs "$OUT"/v3/logs "$R"/tmp
chmod 777 "$OUT" "$OUT"/floor "$OUT"/v1 "$OUT"/v2 "$OUT"/v3 "$OUT"/*/logs "$R"/tmp

echo ""
if [ $fail = 0 ]; then
    echo "Setup OK. Next:  qsub run_everything.pbs"
else
    echo "Setup had errors (see above). Fix them before submitting."
fi
