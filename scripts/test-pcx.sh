#!/bin/sh
set -eu

out=${OUT:-out}
hostcc=${HOSTCC:-${CC:-cc}}

mkdir -p "$out"
"$hostcc" -Wall -Wextra -Werror \
    -D__MALLOC_H -D__UTIL_H \
    -include scripts/pcx-test-shim.h \
    src/pcx.c scripts/test-pcx.c \
    -o "$out/test-pcx"
"$out/test-pcx"
