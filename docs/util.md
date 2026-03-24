---
layout: default
title: Utilities Module
---

# Utilities (`src/util.c`)

Functions:

- `kstrcmp(a,b)` — Lexicographic comparison of NUL-terminated strings.
- `kstrncmp(a,b,n)` — Compare up to `n` characters of two strings.
- `kstrcpy(dst,src)` — Unbounded copy; use only when destination is known to be large enough.
- `kstrncpy(dest,src,n)` — Copy at most `n-1` bytes and NUL-terminate; no-op when `n<=0`.
- `kstrlen(s)` — Return length of a NUL-terminated string.

Source: [src/util.c](../src/util.c)
