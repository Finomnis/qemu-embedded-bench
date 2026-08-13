#!/bin/bash

set -eu

MACHINE="$1"

BENCHMARK_BEGIN=$(
    llvm-nm "/algo.firmware" |
    awk '$3 == "benchmark_begin" { print "0x" $1 }'
)
BENCHMARK_END=$(
    llvm-nm "/algo.firmware" |
    awk '$3 == "benchmark_end" { print "0x" $1 }'
)

qemu-system-arm -M "$MACHINE" -nographic -semihosting -kernel /algo.firmware -d plugin -plugin /bench-plugin.so,start=$BENCHMARK_BEGIN,end=$BENCHMARK_END
