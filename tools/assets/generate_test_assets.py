#!/usr/bin/env python3
"""Generates KopSkoot test assets (all original artwork).

Outputs (relative to the repository root):
  data/templates/sample-kill-icon.png        template used by sample-killfeed.json
  data/test-assets/*.png                     overlays for the manual OBS test
  tests/data/entries/*.png                   kill-feed entry strips used by unit tests

Optional:
  --sequence DIR   also writes a full 1920x1080 frame sequence (+ .ocr.txt
                   sidecars) that `killsense-replay` can consume.

Requires Pillow (pip install pillow). Deterministic output.
"""
import argparse
import os
import random

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
W, H = 1920, 1080
PANEL = (18, 20, 28, 235)
ICON = 40
ENTRY_W, ENTRY_H = 520, 44
FEED_X, FEED_Y = 1380, 80  # top-left of the first kill-feed row


def font(size):
    for name in ("DejaVuSans-Bold.ttf", "arialbd.ttf", "Arial Bold.ttf"):
        for base in ("/usr/share/fonts/truetype/dejavu", "C:/Windows/Fonts", "/Library/Fonts", ""):
            path = os.path.join(base, name) if base else name
            try:
                return ImageFont.truetype(path, size)
            except OSError:
                continue
    return ImageFont.load_default()


def kill_icon():
    """Red ring with a white cross - generic 'kill' marker."""
    im = Image.new("RGBA", (ICON, ICON), PANEL)
    d = ImageDraw.Draw(im)
    d.ellipse((2, 2, ICON - 3, ICON - 3), outline=(230, 30, 40, 255), width=5)
    d.line((11, 11, ICON - 12, ICON - 12), fill=(250, 250, 250, 255), width=5)
    d.line((11, ICON - 12, ICON - 12, 11), fill=(250, 250, 250, 255), width=5)
    return im


def death_icon():
    """Grey ring with a horizontal bar - used for 'you were eliminated'."""
    im = Image.new("RGBA", (ICON, ICON), PANEL)
    d = ImageDraw.Draw(im)
    d.ellipse((2, 2, ICON - 3, ICON - 3), outline=(150, 150, 150, 255), width=5)
    d.rectangle((10, ICON // 2 - 3, ICON - 11, ICON // 2 + 3), fill=(200, 200, 200, 255))
    return im


def entry(text, icon, text_color=(245, 245, 245, 255)):
    im = Image.new("RGBA", (ENTRY_W, ENTRY_H), PANEL)
    im.paste(icon, (6, (ENTRY_H - ICON) // 2))
    d = ImageDraw.Draw(im)
    d.text((6 + ICON + 12, 9), text, font=font(22), fill=text_color)
    return im


ENTRIES = {
    "kill-raider": ("YOU ELIMINATED Raider", kill_icon),
    "kill-bandit": ("YOU ELIMINATED Bandit", kill_icon),
    "kill-warden": ("YOU ELIMINATED Warden", kill_icon),
    "kill-scout": ("YOU ELIMINATED Scout", kill_icon),
    "death-raider": ("YOU WERE ELIMINATED BY Raider", death_icon),
}


def background(seed):
    rnd = random.Random(seed)
    im = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    d = ImageDraw.Draw(im)
    for y in range(0, H, 8):
        c = int(30 + 40 * y / H)
        d.rectangle((0, y, W, y + 8), fill=(c // 2, c, c + 20, 255))
    for _ in range(60):  # blue/green "scenery" blocks (no red, so the colour cue stays clean)
        x, y = rnd.randrange(W), rnd.randrange(H)
        s = rnd.randrange(20, 160)
        g = rnd.randrange(60, 160)
        d.rectangle((x, y, x + s, y + s // 2), fill=(10, g, g + 40, 255))
    return im


def overlay(entries):
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for row, key in enumerate(entries):
        text, icon = ENTRIES[key]
        im.paste(entry(text, icon()), (FEED_X, FEED_Y + row * (ENTRY_H + 4)))
    return im


def write_sequence(out_dir):
    """Scenario used by docs/TESTING.md (expected: 4 kills, 1 death, best streak 3)."""
    os.makedirs(out_dir, exist_ok=True)
    timeline = []
    timeline += [[]] * 12
    timeline += [["kill-raider"]] * 8 + [[]] + [["kill-raider"]] * 9  # 1-frame flicker
    timeline += [[]] * 12
    timeline += [["kill-bandit"]] * 3 + [["kill-warden", "kill-bandit"]] * 15  # rapid double
    timeline += [[]] * 10
    timeline += [["death-raider"]] * 15
    timeline += [[]] * 10
    timeline += [["kill-scout"]] * 15
    timeline += [[]] * 6
    for i, rows in enumerate(timeline):
        frame = background(1000 + (i % 3)).copy()  # slight background motion
        frame.alpha_composite(overlay(rows))
        name = os.path.join(out_dir, "frame_%04d" % i)
        frame.convert("RGB").save(name + ".png", optimize=True)
        with open(name + ".ocr.txt", "w", encoding="utf-8") as f:
            f.write("\n".join(ENTRIES[k][0] for k in rows))
    print("wrote %d frames to %s" % (len(timeline), out_dir))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sequence", help="also write a replay frame sequence to this directory")
    args = ap.parse_args()

    tdir = os.path.join(ROOT, "data", "templates")
    adir = os.path.join(ROOT, "data", "test-assets")
    edir = os.path.join(ROOT, "tests", "data", "entries")
    for d in (tdir, adir, edir):
        os.makedirs(d, exist_ok=True)

    kill_icon().convert("RGB").save(os.path.join(tdir, "sample-kill-icon.png"))
    for key, (text, icon) in ENTRIES.items():
        entry(text, icon()).convert("RGB").save(os.path.join(edir, key + ".png"))

    background(1000).convert("RGB").save(os.path.join(adir, "killsense-test-background.png"), optimize=True)
    overlay(["kill-raider"]).save(os.path.join(adir, "killsense-test-kill-row1.png"), optimize=True)
    # Second overlay puts a new entry on top and pushes the old one down (rapid kill).
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    img.paste(entry(*[ENTRIES["kill-warden"][0], kill_icon()]), (FEED_X, FEED_Y))
    img.paste(entry(*[ENTRIES["kill-raider"][0], kill_icon()]), (FEED_X, FEED_Y + ENTRY_H + 4))
    img.save(os.path.join(adir, "killsense-test-kill-row2.png"), optimize=True)
    overlay(["death-raider"]).save(os.path.join(adir, "killsense-test-death.png"), optimize=True)
    print("assets written")

    if args.sequence:
        write_sequence(args.sequence)


if __name__ == "__main__":
    main()
