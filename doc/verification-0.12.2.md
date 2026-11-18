# Verification record: Limpet 0.12.2

Checks of 2026-11-18. All targets were built from the same source with
version 0.12.2 and date 18.11.2026, with the toolchains, SDKs and aros-crypto
kit 1 of 0.12.1. `make test` passes. All runtime checks used QEMU.

One thing changed in the terminal since 0.12.1: its window is a smart
refresh window with one layer. Before, it was a simple refresh window with
GimmeZeroZero, which has two layers. The SSH programs have the code of
0.12.1 and were rebuilt only for the version number.

## Why

A user reported that swinging the Limpet window around with the Opaque
commodity slowed the whole system (AROS One, VMware SVGA driver, 1920x1080).
A simple refresh window is asked to draw again whatever was covered or
outside the screen, and Limpet answered each request by drawing its rows.
With GimmeZeroZero the system also moves two layers for every step.

## Measurement

AROS One 1.3 (x86_64 ABIv11) on QEMU's VMware SVGA card, 1920x1080, Opaque
running, window 680x478 with a directory listing. A probe held the title bar
through `input.device` and moved the pointer 100 steps, one every two ticks.
A loop at priority -5 counted how much processor time was left; the table
gives its count per tick, two runs each. Higher is better.

| Window | Inside the screen | Up to 96 pixels outside the right edge |
|---|---|---|
| 0.12.1: simple refresh, GimmeZeroZero | 1971, 1967 | 2337, 2334 |
| Smart refresh, GimmeZeroZero | 2152, 2083 | 2788, 2693 |
| 0.12.2: smart refresh, one layer | 2608, 2530 | 2848, 2809 |

The released 0.12.2 archive, measured again the same way: 2455 and 2427
inside the screen, 2771 and 2762 at the edge.

## What each system ran

| Check | AROS One 1.3, 1920x1080 | mainline nightly 20261113 |
|---|---|---|
| Start from the archive, directory listing | passed | passed |
| Text selection with the mouse | passed (test build) | not run |
| Fast resize through the size gadget with Opaque | 800 sizes (test build), 600 (archive): text and frame intact | not run: the probe did not reach the size gadget |
| Covered by another window and uncovered | passed (test build) | not run |
| Close | passed | passed |

"Test build" is the same source change built before the version number was
set.

## Not checked

- The Raspberry Pi archive was built and not run: the test machine was in
  use.
- The translucent window, Preferences, the console handler and the SSH
  programs were not run again; their code is unchanged. The translucent
  window already used a smart refresh window with one layer.
- Real hardware.
