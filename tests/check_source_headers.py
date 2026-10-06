"""Check tracked first-party C++ headers. Run with Python 3; no Qt required."""

import argparse
from pathlib import Path
import subprocess
import sys
import unittest


HEADER = b"""/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */"""

# No currently tracked .h/.cpp falls here. Preserve externally owned notices
# if third-party source is added. New generated/vendored paths require review,
# not a filename heuristic. Untracked build/_deps and MCUT overlays are never
# enumerated by git ls-files.
EXCLUDED_PREFIXES = ("third_party/",)


def applicable(path):
    return path.endswith((".h", ".cpp")) and not path.startswith(EXCLUDED_PREFIXES)


def compliant(data):
    # Permit a UTF-8 BOM and either repository newline convention. Do not decode
    # the implementation: older files can contain non-UTF-8 bytes below headers.
    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]
    return data.replace(b"\r\n", b"\n").lstrip(b" \t\r\n").startswith(HEADER)


class ComplianceTest(unittest.TestCase):
    def test_content(self):
        for data in (HEADER, HEADER + b"\n\n#pragma once\n",
                     HEADER.replace(b"\n", b"\r\n"), b"\xef\xbb\xbf" + HEADER):
            self.assertTrue(compliant(data))
        for data in (b"", b"#pragma once\n" + HEADER,
                     HEADER.replace(b"2026", b"2025"),
                     HEADER.replace(b"BrickSuite", b"BrickVault"),
                     HEADER.replace(b"<https://www.gnu.org/licenses/>", b"other")):
            self.assertFalse(compliant(data))

    def test_scope(self):
        for path in ("src/New.cpp", "tests/New.h", "deployment/linux/Probe.cpp",
                     "new-first-party/New.h"):
            self.assertTrue(applicable(path))
        self.assertFalse(applicable("third_party/vendor/source.cpp"))
        self.assertFalse(applicable("tests/example.py"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(ComplianceTest))
        return 0 if result.wasSuccessful() else 1
    root = Path(__file__).resolve().parents[1]
    try:
        output = subprocess.check_output(
            ["git", "ls-files", "-z", "--", "*.h", "*.cpp"], cwd=root)
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"Cannot enumerate tracked C++ files: {error}", file=sys.stderr)
        return 1
    paths = [p.decode("utf-8") for p in output.split(b"\0") if p]
    failures = []
    selected = [p for p in paths if applicable(p)]
    if not selected:
        print("No applicable tracked C++ files found.", file=sys.stderr)
        return 1
    for path in selected:
        try:
            if not compliant((root / path).read_bytes()):
                failures.append(path)
        except OSError as error:
            failures.append(f"{path}: {error}")
    for failure in failures:
        print(f"Missing/incorrect BrickSuite source header: {failure}", file=sys.stderr)
    print(f"Inspected {len(paths)} tracked C++ files; {len(selected)} first-party; "
          f"{len(paths) - len(selected)} excluded; {len(failures)} failures.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
