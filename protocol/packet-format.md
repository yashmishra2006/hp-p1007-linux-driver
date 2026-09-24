# P1007 print stream format

Status legend: **C** = confirmed on hardware or by exact decode, **H** = hypothesis, **U** = unknown.

## Transport (C)
USB 2.0 high-speed, 03f0:4817, one interface, class 7/1/2 (printer, bidirectional).
Bulk OUT EP 0x01 (512 B), bulk IN EP 0x81 (512 B). The job is written to bulk OUT as a plain byte stream (CUPS `usb` backend, EXP-002..006).
No firmware download is needed: the printer prints a job immediately after a cold power-on (EXP-006). Device ID: `MFG:Hewlett-Packard;MDL:HP LaserJet P1007;CMD:HBS,PJL,ACL;CLS:PRINTER;DES:HP LaserJet P1007;FWVER:20080415;`

## Job layout (C: HP P1007DP.PRN, and our output prints identically)
```
ESC%-12345X@PJL JOB\n
@PJL SET JAMRECOVERY=OFF\n
@PJL SET DENSITY=<1-5>\n
@PJL SET ECONOMODE=<ON|OFF>\n
@PJL SET RET=MEDIUM\n
@PJL USTATUS DEVICE = OFF\n
@PJL USTATUS JOB = OFF\n
@PJL USTATUS PAGE = OFF\n
@PJL SET JOBATTR="JobAttr4=<YYYYMMDDhhmmss>"\n
ESC%-12345X,XQX
<records>
ESC%-12345X@PJL EOJ\nESC%-12345X
```

## Records (all big-endian u32)
`type, count` followed by `count` items of `id, len, value[len]`. The JBIG record (type 7) is `7, nbytes, data[nbytes]`.
Item `0x80000000` = byte size of the rest of the item list. Every list ends with item `0x80000001 = 0xDEADBEEF`.

| Record | type | Items (HP order) |
|---|---|---|
| START_DOC | 1 | 0x10000005=1, 0x10000001=0, 0x10000002=0 (duplex), 0x10000000=0, 0x10000003=1 |
| START_PAGE | 3 | 0x20000005 copies=1, 0x20000006 source (7 auto, 4 manual), 0x20000000 media=1, 0x20000007=1, 0x20000008=600, 0x20000009=400, 0x2000000d RASTER_X, 0x2000000e RASTER_Y, 0x2000000a=2, 0x2000000f VIDEO_X, 0x20000010 VIDEO_Y, 0x20000011 economode, 0x20000001 DMPAPER |
| START_PLANE (first) | 5 | 0x40000000=0, 0x40000002 = 20-byte JBIG BIH |
| START_PLANE (continuation) | 5 | 0x40000000=0, 0x40000003=1 |
| JBIG | 7 | up to 65536 bytes of BID per record |
| END_PLANE / END_PAGE / END_DOC | 6 / 4 / 2 | none |

A page is START_PAGE, then (START_PLANE, JBIG ≤64 KiB, END_PLANE) repeated until the BID is exhausted, then END_PAGE. Multiple pages share one START_DOC/END_DOC (C, EXP-005: 2 pages, 16+4 chunks).

## Raster (C)
- 1 bit per sample, 1 = black, 1200 dpi horizontal × 600 dpi vertical. VIDEO_X is in 600-dpi pixels, and 2 bits per pixel = VIDEO_BPP 2.
- The page is cropped by HP's hard margin of 47/300 in on every side (HP1006SD.SDD): 188 bits left/right, 94 lines top/bottom.
- RASTER_X = 2·VIDEO_X rounded up to a multiple of 128 (PixAlignX), and the padding is white. RASTER_Y = VIDEO_Y, a multiple of 4.
- JBIG1 (ITU T.82), single layer: DL=0, D=0, P=1, L0=128, MX=12, MY=0, order ILEAVE|SMID, options LRLTWO|TPDON|TPBON|DPON.
- The printer places the video area so that content is centred correctly on A4 (C, EXP-004).

## Unknown
- The meaning of 0x20000009=400 (named RESOLUTION_Y by foo2zjs, but 400 is not a resolution this printer uses). We copy HP's value. **U**
- 0x10000000/1/3/5, 0x20000007: constants copied from HP. **U**
- Back-channel (bulk IN) content: the printer sends 20–60 byte replies even with USTATUS OFF. Not needed for printing. **U**
- The media-type codes for non-plain paper. **U**
