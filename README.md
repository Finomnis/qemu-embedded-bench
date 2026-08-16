# qemu-embedded-bench

An arm qemu setup to benchmark algorithms

## Goal

The goal of this repository is to provide a reproducible setup to measure algorithm performance on an assembly level.

The algorithm has to be compiled for a known

## Reproducibility

For reproducibility, the entire benchmark setup is built into a Docker image.

This makes sure that qemu and plugin version are constant and reproducible.

## Warning

This is a synthetic benchmark that only counts instructions and memory accesses. It does not take into account the fact that instructions may take a variable amount of time, or that memory performance strongly depends on caching. So take the results with a grain of salt; they are primarily meant to analyze on an algorithm level, they are not suitable for microoptimizations like cache striding or similar.

## Usage

The firmware needs to be compiled with semihosting enabled and must report success/error via semihosting.
It must contain the asm labels `benchmark_begin` and `benchmark_end`; the measurement will happen
between those two labels.

Then, run the docker image with:
   - the firmware elf file mounted at `/algo.firmware`
   - the machine type as the first argument

Like so:

```
docker run \
  --rm \
  --mount type=bind,src=<my-firmware-elf-file>,dst=/algo.firmware \
  ghcr.io/finomnis/qemu-embedded-bench:v0.4.0 \
  <qemu-machine-name>
```

Example output:
```json
{
  "regions_started": 1,
  "regions_completed": 1,
  "instructions": 5,
  "reads": 2,
  "writes": 2
}
```

As a sanity check, make sure `regions_started` and `regions_completed` are always `1`; otherwise either the benchmark start/end markers
were not executed or they were executed multiple times.

For a complete usage example, see the `example` subdirectory, specifically `example/benchmark.sh`.

## Tracing

The benchmark also supports a full trace mode, which can be enabled using the `--trace` option, like:

```
docker run \
  --rm \
  --mount type=bind,src=<my-firmware-elf-file>,dst=/algo.firmware \
  ghcr.io/finomnis/qemu-embedded-bench:v0.4.0 \
  <qemu-machine-name> \
  --trace
```

This disables printing the json result and instead prints a newline separated list of instruction addresses the program went through,
interspersed with the strings `r` and `w`, representing memory read/write.
