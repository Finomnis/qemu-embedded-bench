#![no_std]
#![no_main]

use core::hint::black_box;
use cortex_m_rt::entry;
use cortex_m_semihosting::debug::{self, EXIT_SUCCESS};
use panic_semihosting as _;

#[entry]
fn main() -> ! {
    let input_a = black_box(1);
    let input_b = black_box(2);

    // Marks the start of the benchmark
    #[allow(named_asm_labels)]
    unsafe {
        core::arch::asm!(
            ".global benchmark_begin",
            "benchmark_begin:",
            options(nostack, preserves_flags),
        );
    }

    let result = run_benchmark(input_a, input_b);

    // Marks the end of the benchmark
    #[allow(named_asm_labels)]
    unsafe {
        core::arch::asm!(
            ".global benchmark_end",
            "benchmark_end:",
            options(nostack, preserves_flags),
        );
    }

    black_box(result);
    assert!(result == 3);

    debug::exit(EXIT_SUCCESS);
    panic!();
}

#[inline(never)]
fn run_benchmark(a: u32, b: u32) -> u32 {
    a + b
}
