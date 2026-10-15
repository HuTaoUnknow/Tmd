# MD4C

Unmodified `md4c.c` and `md4c.h` from Qt Base v6.11.1:
https://github.com/qt/qtbase/tree/v6.11.1/src/3rdparty/md4c

Upstream: https://github.com/mity/md4c
Copyright (c) 2016–2024 Martin Mitáš. MIT license; the full notice is retained at the start of both source files and in `licenses/md4c-MIT.txt`.

Pinned Git blobs:
- md4c.h: 8d6be1cb4631528e7c387ca2e49ce42d1ef87477
- md4c.c: 812bde535850dfff546c357ed0ed3e6b9a177cfb

Tmd's adapter lives in `core/markdown/MarkdownSyntax.cpp`. Use CommonMark plus GFM tables, task lists, strikethrough and autolinks. Disable the optional underscore-as-underline extension in both adapter and Qt rendering. Match Qt's UTF-8 parser input and translate byte offsets back to QString positions, including surrogate pairs.
