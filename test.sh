#!/bin/sh

rm -f err.log
trap "rm -rf err.log" EXIT
for i in bins/*.bin; do
  echo "$i"
  ./me7sum -r "$i".txt "$i" | grep -E '(ABORT|WARNING)' >> err.log
  grep ERROR "$i".txt >> err.log
done

cat err.log
[ ! -s err.log ]
