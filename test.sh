#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
./build.sh
check_dir="$(mktemp -d)"
trap 'rm -rf -- "$check_dir"' EXIT
"${CXX:-g++}" -std=c++17 -O2 -pthread -Wall -Wextra -Wpedantic tests/core.cpp -o "$check_dir/core-tests"
"$check_dir/core-tests"
for test in finite automata_search generalization algebra_search semantic_synthesis closure_search profile_search unary_sets unary_evolution optimality context_loops; do
    "${CXX:-g++}" -std=c++17 -O2 -pthread -Wall -Wextra -Wpedantic "tests/$test.cpp" -o "$check_dir/$test-tests"
    "$check_dir/$test-tests"
done
python3 tests/test.py ./regex-shortener
python3 tests/optimality_oracle.py ./regex-shortener
