#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
./build.sh
check_dir="$(mktemp -d)"
trap 'rm -rf -- "$check_dir"' EXIT
"${CXX:-g++}" -std=c++17 -O2 -pthread -Wall -Wextra -Wpedantic tests/core.cpp -o "$check_dir/core-tests"
"$check_dir/core-tests"
python3 tests/test.py ./regex-shortener
