#!/bin/bash
# Runs AllReduce programs of one version on the nodes of the current PBS job, one rank per node,
# and prints the result lines of every rank. Called by run_pass1.pbs and run_pass2.pbs; not submitted directly.
#
# Settings (environment variables):
#   CODE      folder with the version's programs, e.g. ~/IMT2023016/paper_algorithms_v4
#   TMPLOG    folder for the per-rank scratch logs (must be writable by the job user)
#   PROGRAMS  colon-separated program folders, e.g. Baseline_Barrier:LL
#   SIZES     colon-separated sizes in floats: 8 = 32 B, 16 = 64 B, 32 = 128 B, 8192 = 32 KB, 8388608 = 32 MB
#   ITERS     timed rounds per run
#   TIMEOUT   seconds per run before a hanging program is stopped (exit=124)
#   RANKS     optional: use only the first RANKS nodes (e.g. 2)

set -u
CODE="$CODE"
TMPLOG="$TMPLOG"
PROGRAMS="$PROGRAMS"
SIZES="$SIZES"
ITERS="${ITERS:-1000}"
TIMEOUT="${TIMEOUT:-120}"
mkdir -p "$TMPLOG"
rm -f "$TMPLOG"/*.log

N=$(sort -u "$PBS_NODEFILE" | wc -l)
if [ -n "${RANKS:-}" ] && [ "$RANKS" -lt "$N" ]; then N=$RANKS; fi
echo "CODE=$CODE"
echo "PROGRAMS=$PROGRAMS"
echo "SIZES=$SIZES floats   ITERS=$ITERS   TIMEOUT=${TIMEOUT}s   RANKS=$N"

# pbsdsh does not relay output here, so each task writes a file in the shared home folder.
for i in $(seq 0 $((N-1))); do
    pbsdsh -n "$i" -- bash -c "{ hostname; echo '---'; hostname -I 2>&1; } > '$TMPLOG/mesh_${i}.log'" &
done
wait
IPS=()
for i in $(seq 0 $((N-1))); do
    HOST=$(sed -n 1p "$TMPLOG/mesh_${i}.log")
    IP=$(grep -oE '10\.1\.2\.[0-9]+' "$TMPLOG/mesh_${i}.log" | head -1)
    IPS+=("$IP")
    echo "rank $i -> host $HOST -> ib0 $IP"
done

bin_for() {
    case "$1" in
        Baseline_Barrier)              bin=baseline_barrier_rdma;           ranks=$N; extra="" ;;
        LL)                            bin=ll_allreduce_rdma;               ranks=$N; extra="" ;;
        Sentinel)                      bin=sentinel_allreduce_rdma;         ranks=$N; extra="" ;;
        Twoshot_LL)                    bin=twoshot_ll_allreduce_rdma;       ranks=$N; extra="" ;;
        Twoshot_Sentinel)              bin=twoshot_sentinel_allreduce_rdma; ranks=$N; extra="" ;;
        LL128_Atomic)                  bin=ll128_atomic_allreduce_rdma;     ranks=$N; extra="" ;;
        Bidirectional_DoubleBuffering) bin=bidir_doublebuffer_rdma;         ranks=2;  extra="8" ;;  # 8 chunks per round
        Atomic_Probe)                  bin=atomic_probe;                    ranks=2;  extra="" ;;
        LL_NoWait_NegativeTest)        bin=ll_allreduce_rdma;               ranks=$N; extra="" ;;  # must FAIL
        Ring_AllReduce)                bin=ring_allreduce_rdma;             ranks=$N; extra="" ;;  # standard ring baseline
        *) return 1 ;;
    esac
}

K=0
run_program() {
    local dir="$1" nfloats="$2"
    bin_for "$dir" || { echo "!! unknown program '$dir'"; return; }
    if [ ! -x "$CODE/$dir/$bin" ]; then
        echo ""; echo "=== $dir ($ranks ranks, $nfloats floats) ==="; echo "!! not built: $CODE/$dir/$bin"; return
    fi
    local port=$((20000 + K * 50)); K=$((K + 1))
    local iplist="${IPS[@]:0:$ranks}"
    echo ""
    echo "=== $dir ($ranks ranks, $nfloats floats) ==="
    for i in $(seq 0 $((ranks-1))); do
        local log="$TMPLOG/${dir}_rank${i}.log"
        local cmd="timeout $TIMEOUT '$CODE/$dir/$bin' '$i' '$ranks' '$port' $iplist '$nfloats' $extra '$ITERS' > '$log' 2>&1; echo exit=\$? >> '$log'"
        pbsdsh -n "$i" -- bash -c "$cmd" &
    done
    wait
    for i in $(seq 0 $((ranks-1))); do
        echo "--- $dir rank $i ---"
        grep -E "Correctness|Average|min=|rror|mismatch|M=|exit=|SKIPPED|PROBE" "$TMPLOG/${dir}_rank${i}.log" 2>/dev/null \
            || echo "(missing log file / no result lines)"
    done
}

IFS=':' read -ra PROG_LIST <<< "$PROGRAMS"
IFS=':' read -ra SIZE_LIST <<< "$SIZES"
for NF in "${SIZE_LIST[@]}"; do
    echo ""
    echo "################ message = $NF floats ($((NF * 4)) bytes), $ITERS iters ################"
    for p in "${PROG_LIST[@]}"; do
        if [ "$p" = "Atomic_Probe" ] && [ "$NF" != "${SIZE_LIST[0]}" ]; then continue; fi
        run_program "$p" "$NF"
    done
done
echo ""
echo "Finished."
