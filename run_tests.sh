#!/usr/bin/env bash

for t in Debug/test/test_*; do
  if [[ -x "$t" && ! -d "$t" ]]; then
    echo "==> running $t"
    "$t"
    rc=$?
    if [ $rc -ne 0 ]; then
      echo "FAIL: $t (exit code $rc)"
      exit $rc
    fi
  fi
done

echo "ALL TESTS PASSED"
