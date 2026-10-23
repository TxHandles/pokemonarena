"""Applies a hand-written patch file to a made world file (used by tools/emerald_world.py).

"[patch Map Name]" blocks change that map:
    story Youngster Calvin = Route 103      a route trainer who is a story battle
    stage x y Story stage|before|after|sprite|sight   the person standing there fights that story battle
    at x y: line                            a line after the person standing there (if = ..., need = ...)
    drop npc|sign|door|item|spot|block x y  removes it
    set key = value                         replaces a map key
    tile x y C                              changes one grid square
    anything else                           is added to the map as it is
Lines before the first block go to the top of the file."""
import re


def apply_patches(text, patch):
    blocks = re.split(r"\n(?=\[map\])", text)
    cur, edits, top = None, {}, []
    for line in patch.split("\n"):
        s = line.strip()
        m = re.match(r"^\[patch (.+)\]$", s)
        if m:
            cur = m.group(1); edits.setdefault(cur, []); continue
        if cur is None:
            if s and not s.startswith("#"):
                top.append(s)
            continue
        if s and not s.startswith("#"):
            edits[cur].append(s)
    out = []
    for b in blocks:
        m = re.search(r"^name = (.+)$", b, re.M)
        if m and m.group(1) in edits:
            lines = b.split("\n")
            gi = lines.index("grid:")
            head, rest = lines[:gi], lines[gi:]
            for e in edits.pop(m.group(1)):
                sto = re.match(r"story (.+?) = (.+)$", e)
                stg = re.match(r"stage (\d+) (\d+) ([^|]+?)\s*(?:\|(.*))?$", e)
                at = re.match(r"at (\d+) (\d+): (.+)$", e)
                d = re.match(r"drop (npc|sign|door|item|spot|block) (\d+) (\d+)$", e)
                st = re.match(r"set (\w+) = (.*)$", e)
                tl = re.match(r"tile (\d+) (\d+) (\S)$", e)
                if sto:
                    for k, l in enumerate(head):
                        mm = re.match(r"(npc = \d+ \d+ \S+ \S+ \d+) fight\|" + re.escape(sto.group(1)) + r"\|[^|]*\|([^|]*)\|([^|]*)$", l)
                        if mm:
                            head[k] = "%s trainer|%s|%s|%s" % (mm.group(1), sto.group(2), mm.group(2) or "Let's battle!", mm.group(3) or "You're strong!")
                            if k + 1 < len(head) and head[k + 1].startswith("rematch = "):
                                del head[k + 1]
                            break
                    else:
                        print("patch: no", sto.group(1), "in", m.group(1))
                elif stg:
                    x, y, stage = stg.group(1), stg.group(2), stg.group(3).strip()
                    extra = [t.strip() for t in (stg.group(4) or "").split("|")]
                    for k, l in enumerate(head):
                        mm = re.match(r"npc = %s %s (\S+) (\S+) (\d+) (\w+)\|?(.*)$" % (x, y), l)
                        if not mm:
                            continue
                        sprite, face, sight, kind, args = mm.group(1), mm.group(2), mm.group(3), mm.group(4), mm.group(5).split("|")
                        before = args[2] if kind == "fight" and len(args) > 2 else "Let's battle!"
                        after = args[3] if kind == "fight" and len(args) > 3 else "You're strong!"
                        if len(extra) > 0 and extra[0]: before = extra[0]
                        if len(extra) > 1 and extra[1]: after = extra[1]
                        if len(extra) > 2 and extra[2]: sprite = extra[2]
                        if len(extra) > 3 and extra[3]: sight = extra[3]
                        head[k] = "npc = %s %s %s %s %s trainer|%s|%s|%s" % (x, y, sprite, face, sight, stage, before, after)
                        if k + 1 < len(head) and head[k + 1].startswith("rematch = "):
                            del head[k + 1]
                        break
                    else:
                        print("patch: no npc at", x, y, "in", m.group(1))
                elif tl:
                    tx, ty = int(tl.group(1)), int(tl.group(2))
                    row = rest[1 + ty]
                    rest[1 + ty] = row[:tx] + tl.group(3) + row[tx + 1:]
                elif at:
                    for k, l in enumerate(head):
                        if re.match(r"npc = %s %s " % (at.group(1), at.group(2)), l):
                            head.insert(k + 1, at.group(3)); break
                    else:
                        print("patch: nobody at", at.group(1), at.group(2), "in", m.group(1))
                elif d:
                    keep, skipif = [], False
                    for l in head:
                        if skipif and re.match(r"(if|unless|rematch) = ", l):
                            continue
                        skipif = False
                        if re.match(r"%s = %s %s " % (d.group(1), d.group(2), d.group(3)), l):
                            skipif = True; continue
                        keep.append(l)
                    head = keep
                elif st:
                    head = [l for l in head if not l.startswith(st.group(1) + " = ")] + ["%s = %s" % (st.group(1), st.group(2))]
                else:
                    head.append(e)
            b = "\n".join(head + rest)
        out.append(b)
    for k in edits:
        print("patch: no map named", k)
    if top:
        first = out[0].split("\n")
        wi = next((i for i, l in enumerate(first) if l.startswith("walked = ")), 0)
        first[wi + 1:wi + 1] = top
        out[0] = "\n".join(first)
    return "\n".join(out)
