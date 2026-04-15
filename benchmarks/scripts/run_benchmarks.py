#!/usr/bin/env python3
import subprocess
from pathlib import Path
import sys
import json

# Directories
SCRIPT_DIR = Path(__file__).resolve().parent
BENCHMARKS_DIR = SCRIPT_DIR.parent
PROJECT_ROOT = BENCHMARKS_DIR.parent
BTREE_BIN_DIR = BENCHMARKS_DIR / "bin" / "btree"
SBT_BIN_DIR = BENCHMARKS_DIR / "bin" / "sbt"
RESULTS_DIR = BENCHMARKS_DIR / "results"

def is_valid_json_file(file_path: Path) -> bool:
    """Check if a file exists and contains valid JSON."""
    if not file_path.exists():
        return False
    
    try:
        with open(file_path, 'r') as f:
            json.load(f)
        return True
    except (json.JSONDecodeError, IOError):
        return False

def needs_to_run(binary_path: Path, output_dir: Path) -> bool:
    """Check if a benchmark needs to run (no valid JSON result exists)."""
    output_file = (output_dir / f"{binary_path.name}.json").resolve()
    return not is_valid_json_file(output_file)

def run_benchmark(binary_path: Path, output_dir: Path, progress: str = "") -> bool:
    """Run a single benchmark binary and save JSON output.
    
    Returns:
        True if successful, False if failed
    """
    output_file = (output_dir / f"{binary_path.name}.json").resolve()
    
    progress_str = f"[{progress}] " if progress else ""
    print(f"Running {progress_str}{binary_path.name}...", end=" ", flush=True)
    
    try:
        result = subprocess.run(
            [
                str(binary_path),
                "--benchmark_format=json",
                f"--benchmark_out={output_file}"
            ],
            cwd=PROJECT_ROOT,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        print("✓")
        return True
    except subprocess.CalledProcessError as e:
        print("✗ FAILED")
        print(f"\nError running {binary_path.name}:")
        print(f"Exit code: {e.returncode}")
        
        # Print stderr if present
        if e.stderr:
            print("\nStderr output:")
            print(e.stderr)
        
        # Handle stdout - check if it's benchmark JSON (likely incomplete)
        if e.stdout:
            stdout_preview = e.stdout[:500]  # First 500 chars
            if '"benchmarks"' in stdout_preview or '"context"' in stdout_preview:
                print("\nBenchmark produced partial output (likely crashed mid-execution)")
                print("This usually indicates a segfault or other runtime error.")
                # Check if output file was created and is invalid
                if output_file.exists() and not is_valid_json_file(output_file):
                    print(f"Incomplete JSON file created: {output_file.name}")
            else:
                print("\nStdout output:")
                # Truncate very long outputs
                if len(e.stdout) > 1000:
                    print(e.stdout[:1000])
                    print(f"\n... (truncated {len(e.stdout) - 1000} characters)")
                else:
                    print(e.stdout)
        
        if not e.stderr and not e.stdout:
            print("No output captured - process may have been killed")
        
        print()  # Add blank line for readability
        return False

def main():
    # Create results directory
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    
    print("="*60)
    print("BENCHMARK EXECUTION")
    print("="*60)
    
    # Pre-filter all benchmarks
    btree_to_run = []
    btree_skipped = 0
    sbt_to_run = []
    sbt_skipped = 0
    
    if BTREE_BIN_DIR.exists():
        btree_binaries = sorted(BTREE_BIN_DIR.glob("bench_*"))
        btree_to_run = [b for b in btree_binaries if needs_to_run(b, RESULTS_DIR)]
        btree_skipped = len(btree_binaries) - len(btree_to_run)
    
    if SBT_BIN_DIR.exists():
        sbt_binaries = sorted(SBT_BIN_DIR.glob("bench_*"))
        sbt_to_run = [b for b in sbt_binaries if needs_to_run(b, RESULTS_DIR)]
        sbt_skipped = len(sbt_binaries) - len(sbt_to_run)
    
    # Display plan
    print(f"\nBTree benchmarks: {len(btree_to_run) + btree_skipped} found, {len(btree_to_run)} to run, {btree_skipped} skipped")
    print(f"SBT benchmarks: {len(sbt_to_run) + sbt_skipped} found, {len(sbt_to_run)} to run, {sbt_skipped} skipped")
    print("="*60)
    
    total_benchmarks = 0
    successful = 0
    failed = 0
    skipped = btree_skipped + sbt_skipped
    
    # Run BTree benchmarks
    if btree_to_run:
        print(f"\nRunning BTree benchmarks...")
        print("-"*60)
        
        for idx, binary in enumerate(btree_to_run, 1):
            total_benchmarks += 1
            progress = f"{idx}/{len(btree_to_run)}"
            if run_benchmark(binary, RESULTS_DIR, progress):
                successful += 1
            else:
                failed += 1
    elif BTREE_BIN_DIR.exists():
        print(f"\nNo BTree benchmarks to run (all skipped)")
    else:
        print(f"\n⚠ BTree binary directory not found: {BTREE_BIN_DIR}")
    
    # Run SBT benchmarks
    if sbt_to_run:
        print(f"\nRunning SBT benchmarks...")
        print("-"*60)
        
        for idx, binary in enumerate(sbt_to_run, 1):
            total_benchmarks += 1
            progress = f"{idx}/{len(sbt_to_run)}"
            if run_benchmark(binary, RESULTS_DIR, progress):
                successful += 1
            else:
                failed += 1
    elif SBT_BIN_DIR.exists():
        print(f"\nNo SBT benchmarks to run (all skipped)")
    else:
        print(f"\n⚠ SBT binary directory not found: {SBT_BIN_DIR}")
    
    # Summary
    print("\n" + "="*60)
    print("EXECUTION SUMMARY")
    print("="*60)
    print(f"Total run: {total_benchmarks}")
    print(f"Successful: {successful}")
    print(f"Failed: {failed}")
    print(f"Skipped (results exist): {skipped}")
    print(f"\nResults saved to: {RESULTS_DIR}")
    print("="*60)
    
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
