# Bundled libraries

Each library here is an unmodified copy of a released source tree. To update
one, replace the directory with the new release's files, record the version
and the archive's SHA-256 below, and run the checks listed for it.

## HarfBuzz 14.5.0

- Files: `harfbuzz/src/` is the `src/` directory of `harfbuzz-14.5.0.tar.xz`
  from <https://github.com/harfbuzz/harfbuzz/releases/tag/14.5.0>, unchanged.
  `harfbuzz/COPYING` comes from the same archive.
- Archive SHA-256:
  `b7132e148358a45185c9feafd049dbaf243649d3c44414b3534d9c95d18592b9`
  (the digest GitHub lists for the asset).
- Licence: the "Old MIT" licence in `harfbuzz/COPYING`. The package ships it
  as `Licenses/HarfBuzz-COPYING.txt`.
- Previous version: 11.2.1 (archive SHA-256
  `093714c8548a285094685f0bdc999e202d666b59eeb3df2ff921ab68b8336a49`),
  in Limpet 0.1.0 and 0.2.0.

How Limpet builds it (see `HB_FLAGS` in the Makefile):

- one translation unit, `harfbuzz/src/harfbuzz.cc`, as C++11. HarfBuzz
  14.5.0 still requires only C++11. Its GPU, raster and vector libraries
  are separate sources and are not built;
- `HB_NO_MT`: only `src/cluster_font.c` calls HarfBuzz, from the task that
  draws, so no locking is needed;
- `HB_NO_MMAP`: font tables come from FreeType (`FT_Load_Sfnt_Table`),
  never from a mapped file, so HarfBuzz's file mapping is not needed;
- `HB_NO_SETLOCALE`: no dependency on the C locale;
- `HB_NO_GETENV`, for the ABIv11 worker only: HarfBuzz must not read
  `ENV:` from the handler process (the comment above `ABIV11_WORKER_FLAGS`
  in the Makefile says why).

Checks after an update:

- `make test-font test-font-memory test-shaping-corpus` on the host. The
  corpus test compares every cluster of `tests/shaping_corpus.h`, as the
  renderer draws it, with `tests/shaping-corpus.expected`. Refresh that file
  only after looking at every changed line;
- `make abiv11-frontend`, then `make abiv11-bench-shaping` and run
  `bench-shaping` from the Limpet drawer on AROS, alternating it with the
  previous build in one boot;
- the corpus shown in a Limpet window on AROS, before and after a resize,
  compared with the previous build.

## utf8proc 2.11.0

- Files: `utf8proc/`, from the utf8proc release (`utf8proc.h` says 2.11.0).
  Licence in `utf8proc/LICENSE.md`.
