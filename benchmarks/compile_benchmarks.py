#!/usr/bin/env python3
import os
import subprocess
import shutil
from pathlib import Path
from typing import Dict, List, Tuple, Optional

# Project root (assumes script is in repo/benchmarks)
SCRIPT_DIR = Path(__file__).parent
ROOT_DIR = SCRIPT_DIR.parent
BUILD_DIR = ROOT_DIR / "build_bench_configs" / "shared"
BTREE_BIN_OUTPUT = SCRIPT_DIR / "bin" / "btree"
SBT_BIN_OUTPUT = SCRIPT_DIR / "bin" / "sbt"

def initial_cmake_setup():
    """Set up the build directory once with base configuration."""
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    print(f"\n{'='*60}")
    print("Initial CMake configuration...")
    print(f"{'='*60}")

    cmake_cmd = [
        "cmake",
        str(ROOT_DIR),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -march=native -mtune=native"
    ]

    subprocess.run(cmake_cmd, cwd=BUILD_DIR, check=True)
    print("CMake configuration complete")

def reconfigure_and_build(config_name: str, cmake_defines: Dict[str, str],
                          targets: List[str]) -> Dict[str, Path]:
    """Reconfigure CMake with new defines and build specified targets."""

    print(f"\n{'='*60}")
    print(f"Building: {config_name}")
    print(f"Config: {cmake_defines}")
    print(f"Targets: {', '.join(targets)}")
    print(f"{'='*60}")

    # Reconfigure with new defines
    cmake_cmd = [
        "cmake",
        str(ROOT_DIR),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -march=native -mtune=native"
    ]

    for key, value in cmake_defines.items():
        cmake_cmd.append(f"-D{key}={value}")

    subprocess.run(cmake_cmd, cwd=BUILD_DIR, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

    # Build all specified targets
    binaries = {}
    for target in targets:
        subprocess.run(
            ["cmake", "--build", ".", "--target", target, "--", f"-j{os.cpu_count()}"],
            cwd=BUILD_DIR,
            check=True
        )

        # Find the binary
        binary_path = BUILD_DIR / target
        if not binary_path.exists():
            # Try other common locations
            for candidate in [BUILD_DIR / "benchmarks" / target, BUILD_DIR / "bin" / target]:
                if candidate.exists():
                    binary_path = candidate
                    break

        if not binary_path.exists():
            raise FileNotFoundError(f"Could not find {target} binary in {BUILD_DIR}")

        binaries[target] = binary_path

    return binaries

def main():
    # Create output directories
    BTREE_BIN_OUTPUT.mkdir(parents=True, exist_ok=True)
    SBT_BIN_OUTPUT.mkdir(parents=True, exist_ok=True)
    
    # Initial CMake setup
    initial_cmake_setup()

    # Track configurations to detect duplicates for btree
    btree_configs: Dict[Tuple, List[str]] = {}
    sbt_configs: Dict[Tuple, List[str]] = {}

    # ========================================================================
    # BTREE BENCHMARKS
    # ========================================================================

    # 1. Page boundary aware tests with find_t degrees (BTREE only)
    print("\n" + "="*60)
    print("BTREE CATEGORY 1: Page Boundary Aware (with find_t)")
    print("="*60)

    for log_size in [512, 1024, 2048]:
        config_key = (log_size, True, None, 8)  # (logical_page, page_aware, degree, key_prefix)
        config_name = f"btree_page_aware_lp{log_size}"

        if config_key not in btree_configs:
            btree_configs[config_key] = []
        btree_configs[config_key].append(config_name)

        cmake_defines = {
            "FFDB_LOGICAL_PAGE_SIZE": str(log_size),
            "FFDB_PAGE_BOUNDARY_AWARE": "true",
            "FFDB_KEY_PREFIX_SIZE": "8"
        }

        binaries = reconfigure_and_build(config_name, cmake_defines, ["btree_bench"])
        output_name = f"bench_{config_name}"
        shutil.copy2(binaries["btree_bench"], BTREE_BIN_OUTPUT / output_name)
        print(f"Binary saved: {output_name}")

    # 2. No page boundary aware with various degrees (BTREE only)
    print("\n" + "="*60)
    print("BTREE CATEGORY 2: No Page Boundary Aware (degree sweep)")
    print("="*60)

    for degree in [3, 4, 5, 8, 16, 32, 64]:
        config_key = (512, False, degree, 8)  # default logical_page=512, key_prefix=8
        config_name = f"btree_no_page_aware_deg{degree}"

        if config_key not in btree_configs:
            btree_configs[config_key] = []
        btree_configs[config_key].append(config_name)

        cmake_defines = {
            "FFDB_LOGICAL_PAGE_SIZE": "512",
            "FFDB_PAGE_BOUNDARY_AWARE": "false",
            "BENCH_DEGREE": str(degree),
            "FFDB_KEY_PREFIX_SIZE": "8"
        }

        binaries = reconfigure_and_build(config_name, cmake_defines, ["btree_bench"])
        output_name = f"bench_{config_name}"
        shutil.copy2(binaries["btree_bench"], BTREE_BIN_OUTPUT / output_name)
        print(f"Binary saved: {output_name}")

    # 3. Key prefix size tests (no page aware, degree=5) (BTREE only)
    print("\n" + "="*60)
    print("BTREE CATEGORY 3: Key Prefix Size Tests (no page aware, deg=5)")
    print("="*60)

    for key_prefix in [0, 4, 8, 16, 32]:
        config_key = (512, False, 5, key_prefix)
        config_name = f"btree_key_prefix_kp{key_prefix}"

        # Check if this overlaps with category 2
        overlapping_names = []
        if config_key in btree_configs:
            overlapping_names = btree_configs[config_key]
            print(f"\n⚠ Config overlap detected for kp{key_prefix}!")
            print(f"  Already built as: {', '.join(overlapping_names)}")

            # Create combined name and rename the existing binary
            config_name = f"btree_combined_no_page_aware_key_prefix_deg5_kp{key_prefix}"
            old_binary = BTREE_BIN_OUTPUT / f"bench_{overlapping_names[0]}"
            new_binary = BTREE_BIN_OUTPUT / f"bench_{config_name}"

            if old_binary.exists():
                old_binary.rename(new_binary)
                print(f"Renamed: {old_binary.name} -> {new_binary.name}")

            # Update the config tracking to reflect the new name
            btree_configs[config_key] = [config_name]
        else:
            # No overlap, build normally
            btree_configs[config_key] = [config_name]

            cmake_defines = {
                "FFDB_LOGICAL_PAGE_SIZE": "512",
                "FFDB_PAGE_BOUNDARY_AWARE": "false",
                "BENCH_DEGREE": "5",
                "FFDB_KEY_PREFIX_SIZE": str(key_prefix)
            }

            binaries = reconfigure_and_build(config_name, cmake_defines, ["btree_bench"])
            output_name = f"bench_{config_name}"
            shutil.copy2(binaries["btree_bench"], BTREE_BIN_OUTPUT / output_name)
            print(f"Binary saved: {output_name}")

    # ========================================================================
    # SBT BENCHMARKS
    # ========================================================================

    # 1. SBT with different logical page sizes (default key_prefix=8)
    print("\n" + "="*60)
    print("SBT CATEGORY 1: Logical Page Size Tests")
    print("="*60)

    for log_size in [512, 1024, 2048]:
        config_key = (log_size, 8)  # (logical_page, key_prefix)
        config_name = f"sbt_lp{log_size}"

        if config_key not in sbt_configs:
            sbt_configs[config_key] = []
        sbt_configs[config_key].append(config_name)

        cmake_defines = {
            "FFDB_LOGICAL_PAGE_SIZE": str(log_size),
            "FFDB_KEY_PREFIX_SIZE": "8"
        }

        binaries = reconfigure_and_build(config_name, cmake_defines, ["sbt_bench"])
        output_name = f"bench_{config_name}"
        shutil.copy2(binaries["sbt_bench"], SBT_BIN_OUTPUT / output_name)
        print(f"Binary saved: {output_name}")

    # 2. SBT with different key prefix sizes (default logical_page=512)
    print("\n" + "="*60)
    print("SBT CATEGORY 2: Key Prefix Size Tests")
    print("="*60)

    for key_prefix in [0, 4, 8, 16, 32]:
        config_key = (512, key_prefix)
        config_name = f"sbt_key_prefix_kp{key_prefix}"

        # Check if this overlaps with category 1
        overlapping_names = []
        if config_key in sbt_configs:
            overlapping_names = sbt_configs[config_key]
            print(f"\n⚠ Config overlap detected for kp{key_prefix}!")
            print(f"  Already built as: {', '.join(overlapping_names)}")

            # Create combined name and rename the existing binary
            config_name = f"sbt_combined_lp512_kp{key_prefix}"
            old_binary = SBT_BIN_OUTPUT / f"bench_{overlapping_names[0]}"
            new_binary = SBT_BIN_OUTPUT / f"bench_{config_name}"

            if old_binary.exists():
                old_binary.rename(new_binary)
                print(f"Renamed: {old_binary.name} -> {new_binary.name}")

            # Update the config tracking to reflect the new name
            sbt_configs[config_key] = [config_name]
        else:
            # No overlap, build normally
            sbt_configs[config_key] = [config_name]

            cmake_defines = {
                "FFDB_LOGICAL_PAGE_SIZE": "512",
                "FFDB_KEY_PREFIX_SIZE": str(key_prefix)
            }

            binaries = reconfigure_and_build(config_name, cmake_defines, ["sbt_bench"])
            output_name = f"bench_{config_name}"
            shutil.copy2(binaries["sbt_bench"], SBT_BIN_OUTPUT / output_name)
            print(f"Binary saved: {output_name}")

    print("\n" + "="*60)
    print("BUILD SUMMARY")
    print("="*60)
    print(f"Total unique BTree configurations: {len(btree_configs)}")
    print(f"Total unique SBT configurations: {len(sbt_configs)}")
    print(f"\nBTree binaries: {BTREE_BIN_OUTPUT}")
    for binary in sorted(BTREE_BIN_OUTPUT.glob("bench_*")):
        print(f"  - {binary.name}")
    print(f"\nSBT binaries: {SBT_BIN_OUTPUT}")
    for binary in sorted(SBT_BIN_OUTPUT.glob("bench_*")):
        print(f"  - {binary.name}")
    print("="*60)

if __name__ == "__main__":
    main()
