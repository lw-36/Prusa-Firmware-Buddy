#!/usr/bin/env python3
"""Compile a .po into a .mo, dropping msgids absent from the firmware binary."""

import argparse
import os
import subprocess
import tempfile
from pathlib import Path

import polib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path, help='input .po')
    parser.add_argument('--output',
                        required=True,
                        type=Path,
                        help='output .mo')
    parser.add_argument('--binary',
                        type=Path,
                        help='binary gating which strings are kept')
    args = parser.parse_args()

    pofile = polib.pofile(str(args.input))
    if args.binary:
        blob = args.binary.read_bytes()
        pofile[:] = [e for e in pofile if e.msgid.encode() in blob]

    # polib doesn't build a hash table so we need to call msgfmt here.
    # The temp file is closed (not just opened) before polib reopens it by
    # path, since Windows disallows opening a file that's already held open.
    tmp_fd, tmp_path = tempfile.mkstemp()
    os.close(tmp_fd)
    try:
        pofile.save(tmp_path)
        subprocess.run(['msgfmt', tmp_path, '-o',
                        str(args.output)],
                       check=True)
    finally:
        os.unlink(tmp_path)


if __name__ == '__main__':
    main()
