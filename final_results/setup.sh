#!/bin/bash
# One-time setup on the cluster login node, in ~/IMT2023016/final_results:
#   bash setup.sh
# 1. removes Windows line endings from the scripts here
# 2. checks that the programs of every version are built (it does not build or change them)
# 3. makes the log and scratch folders writable for the job (the qsub wrapper runs jobs as another user)

ROOT=/home/student4/IMT2023016
FR=$ROOT/final_results
cd "$FR" || { echo "missing $FR"; exit 1; }

find "$FR" -maxdepth 1 -type f \( -name '*.sh' -o -name '*.pbs' -o -name '*.py' \) -exec sed -i 's/\r$//' {} +

fail=0
check() {   # check <label> <code folder> <expected number of programs>
    local n
    n=$(find "$2" -mindepth 2 -maxdepth 2 -type f -perm -u+x ! -name '*.sh' ! -name '*.py' 2>/dev/null | wc -l)
    if [ "$n" -ge "$3" ]; then echo "ok      $1: $n programs built in $2"
    else echo "MISSING $1: only $n programs built in $2 (expected at least $3)"; fail=1; fi
}
check "v1  (paper_algorithms)"    "$ROOT/run_updated/code/v1" 6
check "v2  (paper_algorithms_v2)" "$ROOT/run_updated/code/v2" 7
check "v3  (paper_algorithms_v3)" "$ROOT/run_updated/code/v3" 8
check "v4  (paper_algorithms_v4)" "$ROOT/paper_algorithms_v4" 8
check "v5  (paper_algorithms_v5)" "$ROOT/paper_algorithms_v5" 8
check "v6  (paper_algorithms_v6)" "$ROOT/paper_algorithms_v6" 8
check "ring (baselines)"          "$ROOT/baselines" 1

mkdir -p "$FR/tmp"
for d in floor baselines paper_algorithms paper_algorithms_v2 paper_algorithms_v3 paper_algorithms_v4 paper_algorithms_v5 paper_algorithms_v6; do
    mkdir -p "$FR/$d/logs"
    chmod 777 "$FR/$d" "$FR/$d/logs"
done
chmod 777 "$FR" "$FR/tmp"

echo ""
if [ $fail = 0 ]; then echo "Setup OK. Next:  qsub run_pass1.pbs"
else echo "Something is not built (see MISSING above). Build it first, then run this again."; fi
