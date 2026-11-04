# codewatch

Question: what overwrites Limpet's code on AROS One (ABIv11)? In the Helix
preview 2 candidate 6 acceptance (2026-11-04, v11-1) Limpet 0.9.0 failed at the
first hx start: `hb_face_destroy` called `hb_aat_scratch_t`'s destructor, whose
first 16 bytes in memory were `10 00 00 00 14 00 00 00 b8 63 2d 4a 00 00 00 00`
instead of its prologue (`55 48 89 e5 ...`); the rest of the function was intact.

Build: `x86_64-aros-gcc --sysroot=<v11 SDK> -O2 -o codewatch codewatch.c`.
Usage is in the source header.

Found (2026-11-04, AROS One 1.3 on v11-1, Limpet 0.9.0, Helix candidate 6):

- Load addresses differ between boots, so a fixed address cannot be watched;
  `CLI N SYMBOL` finds the section in the running Limpet instead.
- In one of six boots the copy check found another of Limpet's code sections
  changed (HarfBuzz `OTL::match_class_cached2`, offset 0x15C): 32 bytes became
  `0x0178A95B, 0x01002950, 0, 0x4AA02EE0`. debug.library decodes 0x0178A95B as
  exec `Permit+0x2B`: the words are a stack frame. Some task ran with its stack
  pointer inside memory that holds Limpet's code.
- No task's stack range (`tc_SPLower`..`tc_SPUpper`) overlapped Limpet's hunks
  when checked, and no task's saved stack pointer lay outside its range (that
  field may not be kept current on this kernel). Neither Limpet nor rust-aros
  std switches stacks.
- The writing task is not identified yet. The fault is not in Limpet's font
  code: that code was the victim.
