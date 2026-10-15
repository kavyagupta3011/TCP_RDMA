#!/bin/bash
# check_node.sh
#
# Prints information about a cluster node (GPUs, NVLink, CUDA, PBS queues, modules) so
# that the ARCH value in the Makefiles and the PBS script can be set correctly.
#
# Usage:
#   bash check_node.sh > node_info.txt
#
# Read-only: nothing is modified.

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
