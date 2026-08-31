#!/usr/bin/env python3
"""Render a README terminal image from the real Kyna CLI output."""

from __future__ import annotations

import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build-debug" / "bin" / "kyna"
OUTPUT = ROOT / "docs" / "assets" / "kyna-cli.png"
FONT_PATH = Path("/System/Library/Fonts/SFNSMono.ttf")


def run_cli(*arguments: str) -> list[str]:
    result = subprocess.run(
        [str(BINARY), *arguments, "--no-color"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.rstrip().splitlines()


def main() -> None:
    if not BINARY.exists():
        raise SystemExit("build the debug preset before rendering the CLI screenshot")
    if not FONT_PATH.exists():
        raise SystemExit(f"required font not found: {FONT_PATH}")

    run_output = run_cli("run", "examples/hello.kyna")
    bytecode_output = run_cli("bytecode", "examples/hello.kyna")[:14]
    transcript = [
        ("prompt", "$ ky run examples/hello.kyna"),
        *(("output", line) for line in run_output),
        ("blank", ""),
        ("prompt", "$ kyna bytecode examples/hello.kyna"),
        *(("output", line) for line in bytecode_output),
        ("dim", "   …"),
    ]

    width, height = 1440, 900
    image = Image.new("RGB", (width, height), "#090B12")
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((24, 24, width - 24, height - 24), radius=28, fill="#111522", outline="#32384A", width=2)
    draw.rounded_rectangle((24, 24, width - 24, 96), radius=28, fill="#181D2D")
    draw.rectangle((24, 68, width - 24, 96), fill="#181D2D")

    for x, color in ((62, "#FF5F57"), (100, "#FEBC2E"), (138, "#28C840")):
        draw.ellipse((x - 10, 50 - 10, x + 10, 50 + 10), fill=color)

    title_font = ImageFont.truetype(str(FONT_PATH), 24)
    body_font = ImageFont.truetype(str(FONT_PATH), 25)
    draw.text((width // 2, 50), "Kyna CLI", font=title_font, fill="#B9C0D4", anchor="mm")

    colors = {
        "prompt": "#A88BFF",
        "output": "#E8EBF4",
        "dim": "#727A91",
        "blank": "#E8EBF4",
    }
    y = 128
    for kind, line in transcript:
        draw.text((72, y), line, font=body_font, fill=colors[kind])
        y += 36

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT, optimize=True)
    print(OUTPUT.relative_to(ROOT))


if __name__ == "__main__":
    main()
