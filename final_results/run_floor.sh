#!/bin/bash
# Network floor between the first two nodes of the job: ib_write_lat (one RDMA write, one way)
# and ib_write_bw (bandwidth), every size from 2 B to 8 MiB. Called by run_pass1.pbs.
#   TMPLOG  folder for scratch logs (writable by the job user)

set -u
TMPLOG="$TMPLOG"
mkdir -p "$TMPLOG"
rm -f "$TMPLOG"/*.log

for i in 0 1; do
    pbsdsh -n "$i" -- bash -c "{ hostname; hostname -I; ibv_devinfo; echo '--- load'; uptime; ps -eo user,pid,etime,pcpu,comm --sort=-pcpu | head -8; } > '$TMPLOG/info_${i}.log' 2>&1" &
done
wait
SERVER_IP=$(grep -oE '10\.1\.2\.[0-9]+' "$TMPLOG/info_1.log" | head -1)
DEV=$(grep -m1 -oE 'hca_id:[[:space:]]*[^[:space:]]+' "$TMPLOG/info_0.log" | awk '{print $2}')
echo "=== node info ==="
cat "$TMPLOG/info_0.log"; echo "---"; cat "$TMPLOG/info_1.log"
echo "server (node 1) ib0 address: $SERVER_IP   device: $DEV"

run_pair() {
    local tool="$1" port="$2"; shift 2
    pbsdsh -n 1 -- bash -c "timeout 120 $tool -d $DEV -p $port $* > '$TMPLOG/${tool}_server.log' 2>&1" &
    sleep 2
    pbsdsh -n 0 -- bash -c "timeout 120 $tool -d $DEV -p $port $* $SERVER_IP > '$TMPLOG/${tool}_client.log' 2>&1" &
    wait
    echo ""
    echo "=== $tool $* (client side) ==="
    cat "$TMPLOG/${tool}_client.log"
}

run_pair ib_write_lat 18515 -a -F
run_pair ib_write_bw  18516 -a -F
echo ""
echo "Finished."
