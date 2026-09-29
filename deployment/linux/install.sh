#!/bin/sh
set -eu
command -v python3 >/dev/null 2>&1 || { echo 'Ubuntu Python 3 is required. Install python3, then retry.' >&2; exit 1; }
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$root/installer.py" install "$@"
