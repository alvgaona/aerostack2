#!/usr/bin/env python3
"""Decide which colcon packages CI must build for a change set.

Maps every file changed since the merge-base with the target branch to its
nearest colcon package, expands the set with the packages that depend on them
(`colcon list --packages-above`), and escalates to the full workspace when a
change touches something with workspace-wide reach (CI config, as2_core,
as2_msgs, the aerostack2 metapackage, or root build files).

Writes GitHub Actions outputs (and prints them):
  packages    space-separated package names to build and test
  skip        'true' when no package is affected (docs-only change)
  full_build  'true' when the whole workspace is selected

--all bypasses the diff and selects the full workspace (used on pushes to
main, which always build everything). It is also the single source of truth
for the workspace package list.
"""
from __future__ import annotations

import argparse
import os
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

# A change under any of these paths affects every package or CI itself.
ESCALATE_PREFIXES = (
    ".github/",
    "aerostack2/",
    "as2_core/",
    "as2_msgs/",
)
ESCALATE_FILES = (
    "codecov.yaml",
    "pixi.lock",
    "pixi.toml",
)


def run(*args: str) -> str:
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def all_packages() -> list[str]:
    return sorted(set(run("colcon", "list", "--names-only").split()))


def with_dependents(packages: set[str]) -> list[str]:
    out = run("colcon", "list", "--names-only", "--packages-above", *sorted(packages))
    return sorted(set(out.split()))


def nearest_package(path: Path) -> str | None:
    for parent in path.parents:
        manifest = parent / "package.xml"
        if manifest.is_file():
            name = ET.parse(manifest).getroot().findtext("name")
            return name.strip() if name else None
    return None


def escalates(path: str) -> bool:
    return path.startswith(ESCALATE_PREFIXES) or path in ESCALATE_FILES


def emit(packages: list[str], full_build: bool) -> None:
    outputs = {
        "packages": " ".join(packages),
        "skip": "true" if not packages else "false",
        "full_build": "true" if full_build else "false",
    }
    lines = [f"{key}={value}" for key, value in outputs.items()]
    if github_output := os.environ.get("GITHUB_OUTPUT"):
        with open(github_output, "a", encoding="utf-8") as fh:
            fh.write("\n".join(lines) + "\n")
    print("\n".join(lines))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="origin/main", help="ref to diff against")
    parser.add_argument("--all", action="store_true", help="select the full workspace")
    args = parser.parse_args()

    if args.all:
        emit(all_packages(), full_build=True)
        return

    merge_base = run("git", "merge-base", args.base, "HEAD").strip()
    changed = run("git", "diff", "--name-only", merge_base, "HEAD").splitlines()

    if any(escalates(path) for path in changed):
        emit(all_packages(), full_build=True)
        return

    packages = {pkg for path in changed if (pkg := nearest_package(Path(path)))}
    emit(with_dependents(packages) if packages else [], full_build=False)


if __name__ == "__main__":
    main()
