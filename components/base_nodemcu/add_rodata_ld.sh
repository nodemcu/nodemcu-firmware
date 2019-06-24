#!/bin/bash
# syntax: add_rodata_ld.sh /path/to/esp32.project.ld /path/to/snippet_file
set -eu
ldfile="$1"
partial="$2"
out="${ldfile}.new"
IFS='
'
msg="/* NodeMCU patched */"
if [[ $(head -n 1 "$ldfile") =~ "$msg" ]]
then
  echo "NodeMCU rodata already patched into $(basename $ldfile)"
  exit 0
else
  echo "Patching in NodeMCU rodata into $(basename $ldfile)"
  echo "$msg" > "$out"
fi

cat "$ldfile" | while read line
do
  if [[ $line =~ "drom0_0_seg" ]]
  then
    cat "$partial" >> "$out"
  fi
  echo $line >> "$out"
done
mv "$out" "$ldfile"
