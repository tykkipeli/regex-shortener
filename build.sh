#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
"${CXX:-g++}" -std=c++17 -O3 -DNDEBUG -pthread -Wall -Wextra -Wpedantic src/main.cpp -o regex-shortener
printf 'Fertig. Beispiel: ./regex-shortener '\''(01|001|0001)*'\'' --seconds 30\n'
