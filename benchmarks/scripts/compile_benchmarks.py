#!/usr/bin/env python3
import os
import subprocess
import shutil
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass

# Paths
SCRIPT_DIR = Path(__file__).parent
BENCHMARKS_DIR = SCRIPT_DIR.parent
PROJECT_ROOT = BENCHMARKS_DIR.parent
BUILD_DIR = BENCHMARKS_DIR / "build_bench_configs" / "shared"
BTREE_BIN_OUTPUT = BENCHMARKS_DIR / "bin" / "btree"
SBT_BIN_OUTPUT = BENCHMARKS_DIR / "bin" / "sbt"

@dataclass(frozen=True)
class BTreeConfig:
    """Configuration for a BTree benchmark build."""
    logical_page: int
    page_aware: bool
    degree: Optional[int]
    key_prefix: int
    
    def to_cmake_defines(self) -> Dict[str, str]:
        defines = {
            "FFDB_LOGICAL_PAGE_SIZE": str(self.logical_page),
            "FFDB_PAGE_BOUNDARY_AWARE": "true" if self.page_aware else "false",
            "FFDB_KEY_PREFIX_SIZE": str(self.key_prefix)
        }
        if self.degree is not None:
            defines["BENCH_DEGREE"] = str(self.degree)
        return defines

@dataclass(frozen=True)
class SBTConfig:
    """Configuration for an SBT benchmark build."""
    logical_page: int
    key_prefix: int
    
    def to_cmake_defines(self) -> Dict[str, str]:
        return {
            "FFDB_LOGICAL_PAGE_SIZE": str(self.logical_page),
            "FFDB_KEY_PREFIX_SIZE": str(self.key_prefix)
        }

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
        str(PROJECT_ROOT),
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
        str(PROJECT_ROOT),
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

def plan_btree_configs() -> Dict[BTreeConfig, List[str]]:
    """
    Plan all BTree configurations upfront.
    
    Returns:
        Dict mapping config to list of scenario names that use it
    """
    scenarios = {}
    
    # Category 1: Page boundary aware tests
    for log_size in [512, 1024, 2048]:
        scenario_name = f"btree_page_aware_lp{log_size}"
        config = BTreeConfig(
            logical_page=log_size,
            page_aware=True,
            degree=None,  # Uses find_t default
            key_prefix=8
        )
        scenarios[scenario_name] = config
    
    # Category 2: No page aware with various degrees
    for degree in [3, 4, 5, 8, 10, 16, 21, 32, 64]:
        scenario_name = f"btree_no_page_aware_deg{degree}"
        config = BTreeConfig(
            logical_page=512,
            page_aware=False,
            degree=degree,
            key_prefix=8
        )
        scenarios[scenario_name] = config
    
    # Category 3: Key prefix size tests
    for key_prefix in [0, 4, 8, 16, 32]:
        scenario_name = f"btree_key_prefix_kp{key_prefix}"
        config = BTreeConfig(
            logical_page=512,
            page_aware=False,
            degree=5,
            key_prefix=key_prefix
        )
        scenarios[scenario_name] = config
    
    # Group scenarios by their config
    config_to_scenarios = {}
    for scenario_name, config in scenarios.items():
        if config not in config_to_scenarios:
            config_to_scenarios[config] = []
        config_to_scenarios[config].append(scenario_name)
    
    return config_to_scenarios

def plan_sbt_configs() -> Dict[SBTConfig, List[str]]:
    """
    Plan all SBT configurations upfront.
    
    Returns:
        Dict mapping config to list of scenario names that use it
    """
    scenarios = {}
    
    # Category 1: Different logical page sizes
    for log_size in [512, 1024, 2048]:
        scenario_name = f"sbt_lp{log_size}"
        config = SBTConfig(
            logical_page=log_size,
            key_prefix=8
        )
        scenarios[scenario_name] = config
    
    # Category 2: Key prefix size tests
    for key_prefix in [0, 4, 8, 16, 32]:
        scenario_name = f"sbt_key_prefix_kp{key_prefix}"
        config = SBTConfig(
            logical_page=512,
            key_prefix=key_prefix
        )
        scenarios[scenario_name] = config
    
    # Group scenarios by their config
    config_to_scenarios = {}
    for scenario_name, config in scenarios.items():
        if config not in config_to_scenarios:
            config_to_scenarios[config] = []
        config_to_scenarios[config].append(scenario_name)
    
    return config_to_scenarios

def get_binary_name(scenarios: List[str]) -> str:
    """
    Generate a binary name based on scenario names.
    If single scenario: use its name
    If multiple scenarios: create a combined name
    """
    if len(scenarios) == 1:
        return scenarios[0]
    
    # Multiple scenarios - create combined name
    # Sort to ensure consistent naming
    scenarios = sorted(scenarios)
    
    # Try to create a descriptive combined name
    # For btree: likely "no_page_aware_deg{X}" + "key_prefix_kp{Y}"
    if all("btree" in s for s in scenarios):
        if any("no_page_aware_deg" in s for s in scenarios) and any("key_prefix_kp" in s for s in scenarios):
            # Extract the key_prefix value
            kp_scenario = [s for s in scenarios if "key_prefix_kp" in s][0]
            kp_val = kp_scenario.split("kp")[-1]
            return f"btree_combined_no_page_aware_key_prefix_deg5_kp{kp_val}"
    
    # For sbt: likely "lp{X}" + "key_prefix_kp{Y}"  
    if all("sbt" in s for s in scenarios):
        if any("sbt_lp" in s and "key_prefix" not in s for s in scenarios) and any("key_prefix_kp" in s for s in scenarios):
            kp_scenario = [s for s in scenarios if "key_prefix_kp" in s][0]
            kp_val = kp_scenario.split("kp")[-1]
            return f"sbt_combined_lp512_kp{kp_val}"
    
    # Fallback: join with underscore
    return "_".join(scenarios)

def main():
    # Create output directories
    BTREE_BIN_OUTPUT.mkdir(parents=True, exist_ok=True)
    SBT_BIN_OUTPUT.mkdir(parents=True, exist_ok=True)
    
    # Initial CMake setup
    initial_cmake_setup()

    print("\n" + "="*60)
    print("PLANNING PHASE")
    print("="*60)
    
    # Plan all configurations upfront
    btree_configs = plan_btree_configs()
    sbt_configs = plan_sbt_configs()
    
    # Calculate statistics
    btree_total_scenarios = sum(len(scenarios) for scenarios in btree_configs.values())
    btree_unique_configs = len(btree_configs)
    btree_overlaps = btree_total_scenarios - btree_unique_configs
    
    sbt_total_scenarios = sum(len(scenarios) for scenarios in sbt_configs.values())
    sbt_unique_configs = len(sbt_configs)
    sbt_overlaps = sbt_total_scenarios - sbt_unique_configs
    
    print(f"\nBTree: {btree_total_scenarios} scenarios -> {btree_unique_configs} unique builds")
    if btree_overlaps > 0:
        print(f"  ✓ Avoided {btree_overlaps} redundant build(s)")
        for config, scenarios in btree_configs.items():
            if len(scenarios) > 1:
                binary_name = get_binary_name(scenarios)
                print(f"    • {binary_name}: {', '.join(scenarios)}")
    
    print(f"\nSBT: {sbt_total_scenarios} scenarios -> {sbt_unique_configs} unique builds")
    if sbt_overlaps > 0:
        print(f"  ✓ Avoided {sbt_overlaps} redundant build(s)")
        for config, scenarios in sbt_configs.items():
            if len(scenarios) > 1:
                binary_name = get_binary_name(scenarios)
                print(f"    • {binary_name}: {', '.join(scenarios)}")

    # ========================================================================
    # BTREE BUILD PHASE
    # ========================================================================
    
    print("\n" + "="*60)
    print("BTREE BUILD PHASE")
    print("="*60)
    
    for config, scenarios in btree_configs.items():
        binary_name = get_binary_name(scenarios)
        
        print(f"\nBuilding: {binary_name}")
        if len(scenarios) > 1:
            print(f"  Covers scenarios: {', '.join(scenarios)}")
        
        cmake_defines = config.to_cmake_defines()
        binaries = reconfigure_and_build(binary_name, cmake_defines, ["btree_bench"])
        
        output_name = f"bench_{binary_name}"
        shutil.copy2(binaries["btree_bench"], BTREE_BIN_OUTPUT / output_name)
        print(f"Binary saved: {output_name}")

    # ========================================================================
    # SBT BUILD PHASE
    # ========================================================================
    
    print("\n" + "="*60)
    print("SBT BUILD PHASE")
    print("="*60)
    
    for config, scenarios in sbt_configs.items():
        binary_name = get_binary_name(scenarios)
        
        print(f"\nBuilding: {binary_name}")
        if len(scenarios) > 1:
            print(f"  Covers scenarios: {', '.join(scenarios)}")
        
        cmake_defines = config.to_cmake_defines()
        binaries = reconfigure_and_build(binary_name, cmake_defines, ["sbt_bench"])
        
        output_name = f"bench_{binary_name}"
        shutil.copy2(binaries["sbt_bench"], SBT_BIN_OUTPUT / output_name)
        print(f"Binary saved: {output_name}")

    print("\n" + "="*60)
    print("BUILD SUMMARY")
    print("="*60)
    print(f"Total unique BTree configurations: {btree_unique_configs}")
    print(f"Total unique SBT configurations: {sbt_unique_configs}")
    print(f"\nBTree binaries: {BTREE_BIN_OUTPUT}")
    for binary in sorted(BTREE_BIN_OUTPUT.glob("bench_*")):
        print(f"  - {binary.name}")
    print(f"\nSBT binaries: {SBT_BIN_OUTPUT}")
    for binary in sorted(SBT_BIN_OUTPUT.glob("bench_*")):
        print(f"  - {binary.name}")
    print("="*60)

if __name__ == "__main__":
    main()
