# qemu-embedded-bench

An arm qemu setup to benchmark algorithms

## Goal

The goal of this repository is to provide a reproducible setup to measure algorithm performance on an assembly level.

The algorithm has to be compiled for a known

## Reproducibility

For reproducibility, the entire benchmark setup is built into a Docker image.

This makes sure that qemu and plugin version are constant and reproducible.

## Usage

The firmware needs to be compiled with semihosting enabled and must report success/error via semihosting.
It must contain the asm labels `benchmark_begin` and `benchmark_end`; the measurement will happen
between those two labels.

Then, run the docker image with:
   - the firmware elf file mounted at `/tmp/algo.firmware`
   - the machine type as the first argument

Like so:

```
docker run --rm --mount type=bind,src=<my-firmware-elf-file>,dst=/tmp/algo.firmware qemu-embedded-bench <qemu-machine-name>
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
