#!/usr/bin/env python
"""Bump the LoBBS build number in version.properties [LOBBS]"""

lines = None

with open("version.properties", "r", encoding="utf-8") as f:
    lines = f.readlines()

in_lobbs = False
with open("version.properties", "w", encoding="utf-8") as f:
    for line in lines:
        if line.strip() == "[LOBBS]":
            in_lobbs = True
            f.write(line)
            continue
        if in_lobbs and line.startswith("["):
            in_lobbs = False
        if in_lobbs and line.lstrip().startswith("build = "):
            words = line.split(" = ")
            ver = f"build = {int(words[1]) + 1}\n"
            f.write(ver)
        else:
            f.write(line)
