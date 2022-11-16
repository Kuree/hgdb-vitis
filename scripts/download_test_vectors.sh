#!/usr/bin/env bash

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
# the actual root folder
ROOT="$(dirname "${ROOT}")"
TEST_VECTOR="${ROOT}"/tests/vectors

mkdir -p "${TEST_VECTOR}"

for test in dct dct-pipelined ex1; do
  echo "${test}"
  if [ ! -d "${TEST_VECTOR}"/${test} ]; then
    wget https://github.com/Kuree/files/raw/master/vectors/hls/${test}.tar.gz
    tar xzf "${test}".tar.gz
    mv "${test}" "${TEST_VECTOR}"
    rm -f "${test}".tar.gz
  fi
done
