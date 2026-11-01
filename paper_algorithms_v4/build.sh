#!/bin/bash
# Builds v3 and v4 on the cluster login node and prepares the folder for the comparison job.
# Run from ~/IMT2023016/paper_algorithms_v4:   bash build.sh
# (strips Windows line endings first, then builds every program in both versions)

ROOT=/home/student4/IMT2023016
OUT=$ROOT/results_v4
cd "$ROOT" || exit 1

find paper_algorithms_v3 paper_algorithms_v4 -type f \( -name '*.sh' -o -name '*.pbs' -o -name '*.py' -o -name '*.c' -o -name '*.h' -o -name Makefile \) \
    -exec sed -i 's/\r$//' {} +

fail=0
for v in paper_algorithms_v3 paper_algorithms_v4; do
    for d in $v/*/; do
        [ -f "$d/Makefile" ] || continue
        if (cd "$d" && make -B > build.out 2>&1); then
            echo "built   $(basename $v)/$(basename "$d")"
        else
            echo "FAILED  $(basename $v)/$(basename "$d")"; cat "$d/build.out"; fail=1
        fi
        rm -f "$d/build.out"
    done
done

# the job runs as another user, so the result folders must be writable by everyone
mkdir -p "$OUT/logs" "$OUT/tmp"
chmod 777 "$OUT" "$OUT/logs" "$OUT/tmp"

if [ $fail = 0 ]; then echo; echo "OK. Next:  cd $ROOT/paper_algorithms_v4/jobs && qsub compare_v3_v4.pbs"; fi
