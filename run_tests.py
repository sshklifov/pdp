#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys

def parse_args():
    parser = argparse.ArgumentParser(
        description="Run test executables and stop on first failure."
    )

    parser.add_argument(
        "tests",
        nargs="*",
        help="Test name substrings to match (default: run all tests)",
    )

    parser.add_argument(
        "-v", "--invert",
        action="store_true",
        help="Invert selection: exclude matching tests instead of including them",
    )

    return parser.parse_args()


def should_run(path: str, filters, invert: bool) -> bool:
    if not filters:
        return False if invert else True

    matches = any(f in path for f in filters)
    return not matches if invert else matches


def main():
    args = parse_args()
    test_dir = "Debug/test"

    skipped = []
    passed = []

    for name in os.listdir(test_dir):
        if not name.startswith("test_"):
            continue

        path = os.path.join(test_dir, name)

        if not os.path.isfile(path):
            continue
        if not os.access(path, os.X_OK):
            continue

        if not should_run(path, args.tests, args.invert):
            skipped.append(path)
            continue

        print(f"==> running {path}")
        result = subprocess.run([path])

        if result.returncode != 0:
            print(f"FAIL: {path} (exit code {result.returncode})")
            sys.exit(result.returncode)
        passed.append(path)

    if not passed:
        print("NO TESTS RUN")
        if skipped:
            print("\nSKIPPED TESTS:")
            for t in skipped:
                print(f"  {t}")
        sys.exit(1)

    print("ALL TESTS PASSED")

    if skipped:
        print("\nSKIPPED TESTS:")
        for t in skipped:
            print(f"  {t}")


if __name__ == "__main__":
    main()
