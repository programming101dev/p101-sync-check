#!/usr/bin/env bash
set -euo pipefail

tool=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/p101-sync-check-test.XXXXXX")
trap 'rm -rf "$work"' EXIT

run_expect() {
  expected=$1
  shift
  set +e
  "$tool" "$@" >"$work/stdout" 2>"$work/stderr"
  actual=$?
  set -e
  if [ "$actual" -ne "$expected" ]; then
    echo "expected $expected, got $actual: $*" >&2
    cat "$work/stdout" "$work/stderr" >&2
    exit 1
  fi
}

cat >"$work/clean.log" <<'LOG'
ordinary output
P101RESOURCE	4	1	1	1	100	200	ACQUIRE	pthread-mutex-held	A@thread=one	-	0	thread=one	10	lock	clean.c
P101RESOURCE	4	1	1	2	110	210	RELEASE	pthread-mutex-held	A@thread=one	-	0	thread=one	11	unlock	clean.c
P101COMPLETE	4	1	1	3	120	220	2	0	0
LOG

cat >"$work/cycle.log" <<'LOG'
P101RESOURCE	4	1	1	1	100	200	ACQUIRE	pthread-mutex-held	A@thread=one	-	0	thread=one	10	lock	cycle.c
P101RESOURCE	4	1	1	2	110	210	ACQUIRE	pthread-mutex-held	B@thread=one	-	0	thread=one	11	lock	cycle.c
P101RESOURCE	4	1	1	3	120	220	RELEASE	pthread-mutex-held	B@thread=one	-	0	thread=one	12	unlock	cycle.c
P101RESOURCE	4	1	1	4	130	230	RELEASE	pthread-mutex-held	A@thread=one	-	0	thread=one	13	unlock	cycle.c
P101RESOURCE	4	1	1	5	140	240	ACQUIRE	pthread-mutex-held	B@thread=two	-	0	thread=two	14	lock	cycle.c
P101RESOURCE	4	1	1	6	150	250	ACQUIRE	pthread-mutex-held	A@thread=two	-	0	thread=two	15	lock	cycle.c
P101COMPLETE	4	1	1	7	160	260	6	0	0
LOG

run_expect 0 -h
run_expect 0 --help
run_expect 2 -x
run_expect 2 "$work/clean.log" extra
run_expect 2 "$work/missing.log"
run_expect 0 "$work/clean.log"
grep -q '0 findings' "$work/stdout"
run_expect 0 -j "$work/clean.log"
grep -q '"findings":\[\]' "$work/stdout"
run_expect 0 -- "$work/clean.log"
run_expect 1 "$work/cycle.log"
grep -q 'P101-SYNC-001' "$work/stdout"
run_expect 1 -j "$work/cycle.log"
grep -q '"schema":"p101-sync-check-findings-v1"' "$work/stdout"

run_expect 0 <"$work/clean.log"

printf 'P101RESOURCE\t99\t1\t1\t1\t100\t200\tACQUIRE\tpthread-mutex-held\tA\t-\t0\tthread=one\t1\tlock\ta.c\n' >"$work/unsupported.log"
run_expect 2 "$work/unsupported.log"
printf 'P101RESOURCE\t4\tbad\n' >"$work/malformed.log"
run_expect 2 "$work/malformed.log"
printf 'P101RESOURCE\t4\t1\t1\t1\t100\t200\tACQUIRE\tpthread-mutex-held\tA\t-\t0\tthread=one\t1\tlock\ta.c\n' >"$work/incomplete.log"
run_expect 2 "$work/incomplete.log"

{
  printf 'P101RESOURCE\t4\t1\t1\t1\t100\t200\tACQUIRE\tpthread-mutex-held\t'
  printf '%05000d' 0
  printf '\t-\t0\tthread=one\t1\tlock\ta.c\n'
} >"$work/overlong.log"
run_expect 2 "$work/overlong.log"
