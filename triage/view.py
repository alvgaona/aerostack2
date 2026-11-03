"""Show one bug from BUGS.md at a time, redrawing when `current` or `triage.json` changes."""
import json, os, re, shutil, sys, textwrap, time

HERE = os.path.dirname(os.path.abspath(__file__))
BUGS = os.path.join(HERE, "BUGS.md")
CUR = os.path.join(HERE, "current")
LOG = os.path.join(HERE, "triage.json")

B, D, R = "\033[1m", "\033[2m", "\033[0m"
CYAN, YEL, GRN, RED, MAG = "\033[36m", "\033[33m", "\033[32m", "\033[31m", "\033[35m"


def parse():
    lines = open(BUGS).read().splitlines()
    sections, h2, cur = [], None, None
    for ln in lines:
        if ln.startswith("## "):
            h2 = ln[3:]
            cur = {"path": [h2], "body": []}
            sections.append(cur)
        elif ln.startswith("### "):
            cur = {"path": [h2, ln[4:]], "body": []}
            sections.append(cur)
        elif cur is not None:
            cur["body"].append(ln)
    parents = {s["path"][0] for s in sections if len(s["path"]) == 2}
    items = []
    for s in sections:
        if len(s["path"]) == 1 and s["path"][0] in parents and not any(l.startswith("- ") for l in s["body"]):
            continue
        if s["path"][0].startswith("Decisions pending"):
            continue
        body = s["body"]
        while body and not body[0].strip():
            body.pop(0)
        while body and not body[-1].strip():
            body.pop()
        if not body:
            continue
        # Split into intro / bullets / trailing prose.
        bullets, intro, trailing, i = [], [], [], 0
        while i < len(body) and not body[i].startswith("- "):
            intro.append(body[i]); i += 1
        while i < len(body):
            ln = body[i]
            if ln.startswith("- "):
                bullets.append([ln])
            elif ln.startswith("  ") and bullets:
                bullets[-1].append(ln)
            elif ln.strip():
                trailing.append(ln)
            i += 1
        if len(bullets) >= 2 and not trailing:
            for b in bullets:
                items.append({"path": s["path"], "intro": intro, "text": b})
        else:
            items.append({"path": s["path"], "intro": [], "text": body})
    return items


def style(t):
    t = re.sub(r"\*\*(.+?)\*\*", B + r"\1" + R, t)
    t = re.sub(r"`([^`]+)`", CYAN + r"\1" + R, t)
    return t


def render(items, n, log):
    w = min(shutil.get_terminal_size().columns, 110) - 2
    it = items[n - 1]
    out = ["\033[2J\033[H"]
    done = sum(1 for k in log if log[k].get("decision"))
    out.append(f"{D}Bug {R}{B}{n}{R}{D} of {len(items)}   ·   triaged {done}{R}")
    out.append(f"{MAG}{' › '.join(it['path'])}{R}")
    out.append(D + "─" * w + R)
    out.append("")
    blocks, cur = [], None
    for ln in it["text"]:
        m = re.match(r"^(- |\d+\. )", ln)
        if not ln.strip():
            blocks.append(("blank", "", "")); cur = None
        elif ln.startswith("|"):
            blocks.append(("row", "", ln)); cur = None
        elif m:
            cur = ["item", m.group(1), ln[len(m.group(1)):]]; blocks.append(cur)
        elif cur is not None:
            cur[2] += " " + ln.strip()
        else:
            cur = ["para", "", ln.strip()]; blocks.append(cur)
    for kind, mark, txt in blocks:
        if kind == "blank":
            out.append("")
        elif kind == "row":
            out.append(style(txt))
        else:
            pad = " " * len(mark)
            for k, wl in enumerate(textwrap.wrap(txt, w - len(pad))):
                out.append(style((mark if k == 0 else pad) + wl))
    entry = log.get(str(n))
    out.append("")
    out.append(D + "─" * w + R)
    if entry:
        dec = entry.get("decision", "")
        col = {"file": GRN, "drop": RED, "merge": YEL}.get(dec.split()[0] if dec else "", YEL)
        out.append(f"{B}Decision:{R} {col}{dec}{R}")
        for k in ("verified", "notes", "issue"):
            if entry.get(k):
                for k2, wl in enumerate(textwrap.wrap(f"{k.capitalize()}: {entry[k]}", w)):
                    out.append(style(wl))
    else:
        out.append(f"{D}Not triaged yet{R}")
    sys.stdout.write("\n".join(out) + "\n")
    sys.stdout.flush()


def mtime(p):
    try:
        return os.path.getmtime(p)
    except OSError:
        return 0


if __name__ == "__main__":
    items = parse()
    if len(sys.argv) > 1 and sys.argv[1] == "--list":
        for i, it in enumerate(items, 1):
            first = re.sub(r"\s+", " ", " ".join(it["text"]))[:110]
            print(f"{i:3} [{' > '.join(it['path'])}] {first}")
        sys.exit()
    last = None
    while True:
        key = (mtime(CUR), mtime(LOG), mtime(BUGS), shutil.get_terminal_size())
        if key != last:
            last = key
            items = parse()
            try:
                n = int(open(CUR).read().strip())
            except Exception:
                n = 1
            try:
                log = json.load(open(LOG))
            except Exception:
                log = {}
            render(items, max(1, min(n, len(items))), log)
        time.sleep(0.3)
