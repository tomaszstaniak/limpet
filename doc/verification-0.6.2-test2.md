# Verification: Limpet 0.6.2-test2 (2026-10-21)

Source: perf-down 3bff8a0 (v0.6.2-test1 9d7401d plus fd5f4f4 capabilities,
31941e4 local file links, 1db92e6 font packaging, 3bff8a0 documentation).
Built with GCC 10.5 and the ABIv11 2026.09 SDK, LIMPET_VERSION=0.6.2-test2,
LIMPET_DATE=21.10.2026. Archives in dist/0.6.2-test2 (SHA256SUMS there).

Host: `make test` (43 Python tests, fixture check), presentation, icon
contract, SDK and hyperlink tests pass.

Native, AROS One 1.3, pool v11-1, QEMU TCG, run from the final ZIP extracted
to RAM:, two boots:

- icons: `eza -l --icons=always` (eza candidate 11) shows file-type icons in
  one cell, columns aligned;
- local file link: hover shows `file:///RAM%20Disk:lk/notes.txt`, Ctrl+click
  opened the file in the default tool with its contents;
- HTTPS: an OSC 8 link to https://www.aros.org/ from `Type` of a file;
  hover shows the URL, Ctrl+click started the browser (OWB) through OpenURL
  with that address;
- user font: with `font=FONTS:TrueType/VeraMono.ttf` in ENV:Limpet/Limpet.conf
  a new window used Vera Mono; eza then showed no icons (not reported) and
  links still worked;
- Helix preview2-test1 opened and quit normally; the plain Shell (`Dir`,
  `Echo`) drew normally with the new default font;
- redirected eza output stayed plain (no escape bytes).

Not shown: mainline, other ABIv11 bases, real hardware, the experimental
handler archive (built, not run), CON: integration.
Evidence: AROS-dev/limpet-apps/aros-eza/docs/evidence/limpet-0.6.2-test2/.
