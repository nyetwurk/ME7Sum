#!/bin/sh
# Checks the corpus images ME7Sum verifies (corpus.tsv checksums=ok) and the
# modified images in testdata/modified. Reports go to testdata/reports/.

cd "$(dirname "$0")/.."
corpus=${XDFKIT_CORPUS:-corpus}
out=testdata/reports
log=$out/err.log

mkdir -p "$out"
trap 'rm -f "$log"' EXIT
: > "$log"

check() {
  echo "$1"
  if [ ! -f "$1" ]; then
    echo "MISSING $1" >> "$log"
    return
  fi
  ./me7sum -r "$out/$2.txt" "$1" | grep -E '(ABORT|WARNING)' >> "$log"
  grep ERROR "$out/$2.txt" >> "$log"
}

if [ -f "$corpus/corpus.tsv" ]; then
  for name in $(awk -F'\t' 'NR>1 && $8=="ok" {print $1}' "$corpus/corpus.tsv"); do
    check "$corpus/images/$name.bin" "$name"
  done
elif [ "$XDFKIT_REQUIRE_CORPUS" = 1 ]; then
  echo "no corpus at $corpus and XDFKIT_REQUIRE_CORPUS=1 (see BUILD.md)" >&2
  exit 1
else
  echo "no corpus at $corpus, skipping corpus images (see BUILD.md)" >&2
fi

for f in testdata/modified/*.bin; do
  name=${f##*/}
  check "$f" "${name%.bin}"
done

cat "$log"
[ ! -s "$log" ]
