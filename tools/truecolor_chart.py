#!/usr/bin/env python3
"""Emit a deterministic 48x16 truecolor chart for native GUI acceptance."""
import sys


def main():
    out = sys.stdout.buffer
    out.write(b"\x1b[0mAROS Term - RGB foreground and background\n")
    out.write(b"\x1b[1G48 columns x 16 rows, 24-bit SGR\n")
    for y in range(16):
        out.write(f"\x1b[1G{y:02d}  ".encode("ascii"))
        for x in range(48):
            red = (255 * x) // 47
            green = (255 * y) // 15
            blue = (255 * (x + y)) // 62
            out.write(f"\x1b[48;2;{red};{green};{blue}m ".encode("ascii"))
        out.write(b"\x1b[0m\n")
    out.write(b"\x1b[1GForeground: ")
    for red, green, blue in ((255, 20, 20), (20, 255, 20), (20, 20, 255),
                             (213, 77, 191), (42, 178, 199)):
        out.write(f"\x1b[38;2;{red};{green};{blue}mRGB ".encode("ascii"))
    out.write(b"\x1b[0m\n\x1b[1G")


if __name__ == "__main__":
    main()
