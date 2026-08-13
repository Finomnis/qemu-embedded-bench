#!/bin/bash

set -eu

cargo build --target thumbv6m-none-eabi --release
cargo build --target thumbv7em-none-eabi --release

echo "=== Cortex-M0 ==="
docker run --rm --mount type=bind,src=./target/thumbv6m-none-eabi/release/dummy,dst=/tmp/algo.firmware qemu-embedded-bench microbit

echo "=== Cortex-M4 ==="
docker run --rm --mount type=bind,src=./target/thumbv7em-none-eabi/release/dummy,dst=/tmp/algo.firmware qemu-embedded-bench mps2-an386
