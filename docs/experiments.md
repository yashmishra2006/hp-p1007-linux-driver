# Research log

## EXP-000: Static inspection of the Windows driver package
```text
Experiment ID: EXP-000
Date: 2026-09-24
Input: ljP1000_P1500-HB-pnp-win64-en.exe (sha256 36f023ac…fa527)
Printer configuration: n/a (printer not connected)
Driver version: 1.0.2.2680 (2013-04-15)
Capture: none. Static extraction only (7z)
Observed behavior:
  - Monolithic Zenographics/Marvell ZX driver + language monitor, no GPD/Unidrv
  - P1007DP.PRN = PJL + ",XQX" TLV records + JBIG1 raster; decodes to a correct image
  - Firmware images P1005/P1006/P1505.img (no P1007.img)
  - Linux host: foo2xqx queue exists, firmware dir empty, queue paused
Changed bytes: n/a
Hypothesis:
  H1 P1007 wire format = PJL-wrapped XQX with JBIG1 raster (as foo2xqx implements)   [strong: HP-authored stream decodes]
  H2 P1007 needs firmware (P1005 image) after every power-on                         [REFUTED by EXP-006]
  H3 Past "always stuck" on Linux was caused by missing firmware                   [REFUTED by EXP-006; real cause UNKNOWN, see EXP-003 differences]
Confidence: see each hypothesis
Next experiment: EXP-001 (power-on + blank page capture on Windows)
```

## EXP-001: Windows capture (DEFERRED)
Not needed so far. The HP-authored demo stream plus the SDD config were enough to build a working driver. Keep this for any open unknowns.

## EXP-002: HP demo page sent raw from Linux (2026-09-24, job 38)
Input: P1007DP.PRN unmodified, `lp -o raw`. 108717 bytes sent, 58 bytes back-channel.
Observed: **printed perfectly** (confirmed by the user). Linux USB transport plus HP's stream works, with no firmware sent.

## EXP-003: foo2xqx queue, A4 geometry page (job 39)
Observed: printed, but the **content was shifted** (the user didn't measure it). Byte differences from HP: PJL USTATUS ON/INFO/TIMED, a NUL instead of \n after JOBATTR, RES item 600 instead of 400, MX 16 instead of 12, foo2zjs's own margins, no 64 KiB chunking.

## EXP-004: hp1007enc (HP-exact), same geometry page (job 40)
Stream layout is offset-identical to HP's. The PJL header differs only in the timestamp. Decoding the JBIG back gives a pixel-exact match.
Observed: **looks right, centred**. Compared with EXP-003, the shift is gone.

## EXP-005: 2-page stress job through the hp1007 filter (job 41)
1.2 MB, 16 + 4 JBIG chunks. Observed: **both pages perfect**; about 13 s total.

## EXP-006: cold start (job 42)
Printer powered off and on. The foo2zjs udev hook logged "Missing firmware" and sent nothing.
Observed: **printed fine**. H2/H3 refuted: the P1007 doesn't need a firmware download to print our stream.
