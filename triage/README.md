# Bug triage

Working notes for triaging `BUGS.md` into GitHub issues, one bug at a time.

- `BUGS.md`: the bugs found while rewriting the docs.
- `view.py`: shows one bug at a time and redraws when `current` or `triage.json` changes.
  Run `python3 triage/view.py`, or `python3 triage/view.py --list` for the numbered list.
- `triage.json`: what was checked and decided for each bug, keyed by its number in the viewer.
- `issues/`: issue drafts. The first two lines hold the title and labels.
- `show-drafts.sh`: renders drafts for `less -R drafts.ansi`.
