#!/usr/bin/env python3
import subprocess
from pathlib import Path
import sys

# Directories
SCRIPT_DIR = Path(__file__).resolve().parent
BENCHMARKS_DIR = SCRIPT_DIR.parent
PROJECT_ROOT = BENCHMARKS_DIR.parent
BTREE_BIN_DIR = BENCHMARKS_DIR / "bin" / "btree"
SBT_BIN_DIR = BENCHMARKS_DIR / "bin" / "sbt"
RESULTS_DIR = BENCHMARKS_DIR / "results"

def run_benchmark(binary_path: Path, output_dir: Path, progress: str = "") -> bool:
    """Run a single benchmark binary and save JSON output."""
    output_file = (output_dir / f"{binary_path.name}.json").resolve()
    
    progress_str = f"[{progress}] " if progress else ""
    print(f"Running {progress_str}{binary_path.name}...", end=" ", flush=True)
    
    try:
        subprocess.run(
            [
                str(binary_path),
                "--benchmark_format=json",
                f"--benchmark_out={output_file}"
            ],
            cwd=PROJECT_ROOT,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True
        )
        print("✓")
        return True
    except subprocess.CalledProcessError as e:
        print("✗ FAILED")
        print(f"\nError running {binary_path.name}:")
        print(e.stderr)
        return False

def main():
    # Create results directory
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    
    print("="*60)
    print("BENCHMARK EXECUTION")
    print("="*60)
    
    total_benchmarks = 0
    successful = 0
    failed = 0
    
    # Run BTree benchmarks
    if BTREE_BIN_DIR.exists():
        btree_binaries = sorted(BTREE_BIN_DIR.glob("bench_*"))
        print(f"\nBTree benchmarks ({len(btree_binaries)}):")
        print("-"*60)
        
        for idx, binary in enumerate(btree_binaries, 1):
            total_benchmarks += 1
            progress = f"{idx}/{len(btree_binaries)}"
            if run_benchmark(binary, RESULTS_DIR, progress):
                successful += 1
            else:
                failed += 1
    else:
        print(f"\n⚠ BTree binary directory not found: {BTREE_BIN_DIR}")
    
    # Run SBT benchmarks
    if SBT_BIN_DIR.exists():
        sbt_binaries = sorted(SBT_BIN_DIR.glob("bench_*"))
        print(f"\nSBT benchmarks ({len(sbt_binaries)}):")
        print("-"*60)
        
        for idx, binary in enumerate(sbt_binaries, 1):
            total_benchmarks += 1
            progress = f"{idx}/{len(sbt_binaries)}"
            if run_benchmark(binary, RESULTS_DIR, progress):
                successful += 1
            else:
                failed += 1
    else:
        print(f"\n⚠ SBT binary directory not found: {SBT_BIN_DIR}")
    
    # Summary
    print("\n" + "="*60)
    print("EXECUTION SUMMARY")
    print("="*60)
    print(f"Total benchmarks: {total_benchmarks}")
    print(f"Successful: {successful}")
    print(f"Failed: {failed}")
    print(f"\nResults saved to: {RESULTS_DIR}")
    print("="*60)
    
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
