#!/bin/sh

DATA_DIR="/data"
LOCK_FILE="$DATA_DIR/.lock"

CONTAINER_ID=$(cat /proc/sys/kernel/random/uuid 2>/dev/null || \
               head -c 16 /dev/urandom | od -An -tx1 | tr -d ' \n')

seq_num=0

cleanup() {
    rc=$?
    trap - EXIT HUP INT QUIT PIPE TERM
    exit "$rc"
}
trap cleanup EXIT HUP INT QUIT PIPE TERM

mkdir -p "$DATA_DIR"

exec 9>"$LOCK_FILE"

while :; do
    target=""

    flock 9

    n=1
    while :; do
        name=$(printf "%03d" "$n")
        candidate="$DATA_DIR/$name"
        if ( set -C; : > "$candidate" ) 2>/dev/null; then
            target="$candidate"
            break
        fi
        n=$((n + 1))
    done
    
    seq_num=$((seq_num + 1))

    flock -u 9

    printf "%s %d\n" "$CONTAINER_ID" "$seq_num" > "$target"

    sleep 1
    rm -f "$target"
    sleep 1
done
