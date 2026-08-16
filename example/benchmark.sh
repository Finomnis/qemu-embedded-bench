#!/bin/bash

set -eu

SCRIPTPATH=$( cd "$(dirname "$(readlink -f "$0")")" || exit 1 ; pwd -P )
cd "$SCRIPTPATH"

cargo build --target thumbv6m-none-eabi --release
cargo build --target thumbv7em-none-eabi --release

echo "=== Cortex-M0 ==="
docker run \
  --rm \
  --mount type=bind,src=./target/thumbv6m-none-eabi/release/dummy,dst=/algo.firmware \
  ghcr.io/finomnis/qemu-embedded-bench:v0.4.0 \
  microbit

echo "=== Cortex-M4 ==="
docker run \
  --rm \
  --mount type=bind,src=./target/thumbv7em-none-eabi/release/dummy,dst=/algo.firmware \
  ghcr.io/finomnis/qemu-embedded-bench:v0.4.0 \
  mps2-an386
