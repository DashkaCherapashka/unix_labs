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

while :; do
    (
        flock 9 || exit 1

        n=1
        while :; do
            name=$(printf "%03d" "$n")
            target="$DATA_DIR/$name"
            if ( set -C; : > "$target" ) 2>/dev/null; then
                seq_num=$((seq_num + 1))
                printf "%s %d\n" "$CONTAINER_ID" "$seq_num" > "$target"
                echo "$target" > /tmp/.current_target
                break
            fi
            n=$((n + 1))
        done
    ) 9>"$LOCK_FILE"

    target=$(cat /tmp/.current_target)

    sleep 1
    rm -f "$target"
    sleep 1
done
