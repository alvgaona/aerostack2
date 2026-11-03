#!/bin/sh
# Render issue drafts with colors into drafts.ansi for `less -R`.
out="$(cd "$(dirname "$0")" && pwd)/drafts.ansi"
for f in "$@"; do
  printf '\033[1;35m══════ %s ══════\033[0m\n\n' "$(basename "$f")"
  sed -E $'s/^(Title: .*)$/\033[1m\\1\033[0m/; s/^(#+ .*)$/\033[1;36m\\1\033[0m/; s/^(https:.*)$/\033[34m\\1\033[0m/' "$f"
  echo
done > "$out"
