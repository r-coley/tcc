# c-testsuite SCC Runtime Cases

This directory contains a filtered import of the portable, freestanding
runtime cases from `c-testsuite` that originate in SCC (Simple C Compiler).

Import provenance:

- c-testsuite revision: `5c7275656d751de0e68b2d340a95b5681858ed07`
- c-testsuite repository: `https://github.com/c-testsuite/c-testsuite`
- SCC source revision recorded by each imported `.otags` file:
  `355356a9836e487939cf5e98b5332a63e5264e27`
- SCC repository: `git://git.simple-cc.org/scc`
- original paths: `tests/scc/execute/0047-anonexport.c` through
  `tests/scc/execute/0168-array.c`, as recorded in the source import.

The imported SCC test sources are covered by the ISC license below. The
wrapper suite itself is not imported; the project runs these files through
`tests/:runtests.sh` using `tests/external/c-testsuite-scc.manifest.txt`.

```text
ISC License

Copyright (c) 2012-2026 Roberto E. Vargas Caballero
and SCC contributors

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
```
