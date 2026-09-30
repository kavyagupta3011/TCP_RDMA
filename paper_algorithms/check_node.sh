#!/bin/bash
# check_node.sh
#
# Run this on your professor's cluster node (the one with GPU + NVLink
# access) and paste the FULL output back. It gathers everything needed to:
#   - pick the right `ARCH=` value for the Makefiles in this project
#   - confirm the GPUs are actually NVLink-connected (required -- see the
#     top-level README's "Requirements" section)
#   - write you a PBS submission script that matches this cluster's actual
#     queue names, GPU resource syntax, and available modules
#
# Usage:
#   bash check_node.sh > node_info.txt
# then share node_info.txt (or just paste the terminal output).
#
# Nothing here modifies anything -- every command is read-only.

echo "===== Hostname / OS ====="
hostname
uname -a
echo

echo "===== GPUs (nvidia-smi) ====="
if command -v nvidia-smi >/dev/null 2>&1; then
    nvidia-smi
else
    echo "nvidia-smi not found on PATH."
fi
echo

echo "===== GPU interconnect topology (NVLink vs PCIe-only) ====="
if command -v nvidia-smi >/dev/null 2>&1; then
    nvidia-smi topo -m
    echo
    echo "(Look for 'NV#' entries between GPU pairs -- that means NVLink."
    echo " 'PIX'/'PXB'/'SYS' means PCIe/QPI only, which this project's code"
    echo " does NOT support -- see the top-level README's Requirements.)"
else
    echo "nvidia-smi not found on PATH."
fi
echo

echo "===== CUDA compiler (nvcc) ====="
if command -v nvcc >/dev/null 2>&1; then
    nvcc --version
    which nvcc
else
    echo "nvcc not found on PATH directly -- check 'module avail' below,"
    echo "most clusters require 'module load cuda' (or similar) first."
fi
echo

echo "===== Environment modules ====="
if command -v module >/dev/null 2>&1; then
    echo "--- module avail (cuda/nvhpc/gcc related) ---"
    module avail 2>&1 | grep -iE "cuda|nvhpc|nvcc|gcc"
    echo "--- module list (currently loaded) ---"
    module list 2>&1
else
    echo "'module' command not found (this cluster may not use environment modules)."
fi
echo

echo "===== InfiniBand / RDMA hardware (if present) ====="
if command -v ibv_devinfo >/dev/null 2>&1; then
    ibv_devinfo
elif command -v ibstat >/dev/null 2>&1; then
    ibstat
else
    echo "No ibv_devinfo/ibstat found (may not be relevant for this GPU-NVLink task)."
fi
echo

echo "===== CPU / memory ====="
lscpu 2>&1 | head -20
echo
free -h
echo

echo "===== PBS scheduler ====="
if command -v qstat >/dev/null 2>&1; then
    echo "--- available queues (qstat -Q) ---"
    qstat -Q
    echo "--- node resources (pbsnodes -a, first 60 lines) ---"
    pbsnodes -a 2>&1 | head -60
    echo "--- qsub location ---"
    which qsub
else
    echo "qstat not found on PATH -- is this the login/head node with PBS client tools,"
    echo "or should scripts be submitted from a different node?"
fi
echo

echo "===== Done. Paste all of the above back. ====="
