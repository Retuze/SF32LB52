#!/usr/bin/env python3
"""build_all.py — Build all firmware projects in one pass.

Usage:
    python scripts/build_all.py [--debug|--release]

By default builds all projects/* that contain a CMakeLists.txt.
Pass project names as positional args to build only those.
"""

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def get_projects() -> list[str]:
    """Return sorted list of project directory names."""
    proj_dir = ROOT / "projects"
    if not proj_dir.is_dir():
        return []
    return sorted(
        p.name for p in proj_dir.iterdir()
        if p.is_dir() and (p / "CMakeLists.txt").exists()
    )


def build(project: str, preset: str) -> bool:
    """Configure + build a single project. Returns True on success."""
    build_dir = ROOT / "build" / preset

    print(f"\n{'='*60}")
    print(f"  Building: {project} ({preset})")
    print(f"{'='*60}")

    # Configure
    ret = subprocess.run(
        ["cmake", "--preset", preset, "-S", str(ROOT)],
        cwd=str(ROOT),
    )
    if ret.returncode != 0:
        print(f"[FAIL] CMake configure failed for {project}")
        return False

    # Build
    ret = subprocess.run(
        ["cmake", "--build", str(build_dir), "--target", project],
        cwd=str(ROOT),
    )
    if ret.returncode != 0:
        print(f"[FAIL] Build failed for {project}")
        return False

    print(f"[OK] {project}.elf → {build_dir / project}.elf")
    return True


def main():
    parser = argparse.ArgumentParser(description="Build all watch firmware projects")
    parser.add_argument("projects", nargs="*", help="Project names (default: all)")
    parser.add_argument("--debug", action="store_true", default=True,
                        help="Debug build (default)")
    parser.add_argument("--release", action="store_true",
                        help="Release build")
    args = parser.parse_args()

    preset = "release" if args.release else "debug"
    targets = args.projects if args.projects else get_projects()

    if not targets:
        print("[FAIL] No projects found under projects/")
        sys.exit(1)

    print(f"Projects: {', '.join(targets)}")
    print(f"Preset:   {preset}")

    failed = []
    for proj in targets:
        if not build(proj, preset):
            failed.append(proj)

    print(f"\n{'='*60}")
    if failed:
        print(f"  FAILED: {', '.join(failed)}")
        sys.exit(1)
    else:
        print(f"  All {len(targets)} project(s) built successfully.")
        print(f"  Output: {ROOT / 'build' / preset}/")


if __name__ == "__main__":
    main()
