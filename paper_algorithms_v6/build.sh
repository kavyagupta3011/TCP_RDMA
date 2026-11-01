#!/bin/bash
# Builds v6 on the cluster login node and prepares the result folder for the comparison job.
# Run from ~/IMT2023016/paper_algorithms_v6:   bash build.sh
# v3 (the paper_algorithms_v3 link) and v5 must already be built; they are not rebuilt here.

ROOT=/home/student4/IMT2023016
OUT=$ROOT/results_v6
cd "$ROOT" || exit 1

find paper_algorithms_v6 -type f \( -name '*.sh' -o -name '*.pbs' -o -name '*.py' -o -name '*.c' -o -name '*.h' -o -name Makefile \) \
    -exec sed -i 's/\r$//' {} +

fail=0
for d in paper_algorithms_v6/*/; do
    [ -f "$d/Makefile" ] || continue
    if (cd "$d" && make -B > build.out 2>&1); then
        echo "built   v6/$(basename "$d")"
    else
        echo "FAILED  v6/$(basename "$d")"; cat "$d/build.out"; fail=1
    fi
    rm -f "$d/build.out"
done
for v in paper_algorithms_v3 paper_algorithms_v5; do
    [ -x $v/Twoshot_LL/twoshot_ll_allreduce_rdma ] || { echo "!! $v is not built"; fail=1; }
done

# the job runs as another user, so the result folders must be writable by everyone
mkdir -p "$OUT/logs" "$OUT/tmp"
chmod 777 "$OUT" "$OUT/logs" "$OUT/tmp"

if [ $fail = 0 ]; then echo; echo "OK. Next:  cd $ROOT/paper_algorithms_v6/jobs && qsub compare_v3_v5_v6.pbs"; fi
