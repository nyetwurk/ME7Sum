#!/bin/sh
# Temporary: fill bins/ with symlinks under the old bins/ names, for workflows
# that still use them. Remove once nothing reads bins/.
set -e
cd "$(dirname "$0")/.."

if [ ! -f corpus/corpus.tsv ]; then
  echo "bins: corpus/ is not checked out; run 'make corpus' first" >&2
  exit 1
fi

mkdir -p bins

# Never replace a regular file or directory in bins/.
link() {
  if [ -L "bins/$2" ] || [ ! -e "bins/$2" ]; then
    ln -sfn "$1" "bins/$2"
  fi
}

for l in corpus/*.bin; do
  if [ -L "$l" ]; then
    link "../corpus/$(readlink "$l")" "${l##*/}"
  fi
done
awk -F'\t' 'NR>1 {print $1}' corpus/corpus.tsv | while read -r name; do
  link "../corpus/images/$name.bin" "$name.bin"
done
awk -F'\t' 'NR>1 {print $1, $2}' scripts/bins-compat.tsv | while read -r old name; do
  link "../corpus/images/$name.bin" "$old.bin"
done
for f in testdata/modified/*; do
  link "../$f" "${f##*/}"
done
link ../testdata/broken broken

for l in bins/*; do
  if [ -L "$l" ] && [ ! -e "$l" ]; then
    rm "$l"
  fi
done
