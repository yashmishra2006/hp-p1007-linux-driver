# Windows driver inventory (static only, nothing executed)

Source: `driver/original/ljP1000_P1500-HB-pnp-win64-en.exe`
SHA-256 `36f023acfe5dc18abe5c7fa5341e8dcfc369df1a60ba9832c822d624fddfa527`
Unpacked with `7z` into `driver/extracted/installer/`, then `drv64.cab` into `driver/extracted/drv64/`.
Hashes of every file: `driver/metadata/SHA256SUMS`.

## Package identity — CONFIRMED (from INF / VER files)

| Item | Value |
|---|---|
| INF | `HPLJP1005.INF` (normalized copy: `driver/metadata/HPLJP1005.INF.txt`) |
| DriverVer | 04/15/2013, 1.0.2.2680 |
| Series | "HP LaserJet P1000_P1500 Series" (P1005/P1006/P1007/P1008/P1505/P1505n) |
| P1007 hardware ID | `USBPRINT\Hewlett-PackardHP_La7BBA` |
| Installer VID/PID list | 03F0: 3D17, 4817, 4917, 3E17, 3F17, 4017 (`properties.ini`) |

PID to model mapping: foo2zjs's `/lib/udev/hplj1000` says **P1007 = 03f0:4817**. That is a strong hypothesis, not yet confirmed: the INF doesn't pair models with PIDs, and we still have to read the printer's real USB descriptor.

## Architecture — CONFIRMED (INF directives + PE version resources)

This is **not Unidrv/GPD**. It is a Zenographics/Marvell "ZX" host-based driver:

| Role (INF) | File | PE OriginalFilename / description |
|---|---|---|
| PrintProcessor | `hp1006PP.dll` | `ZIMFPrnt.dll`, Intelligent MetaFile Print Processor 6.1.1.0 |
| DriverFile (renderer) | `hp1006SD.dll` | `zimfdrv`, IMF Driver (User Mode) 0.3.7003.1328 |
| ConfigFile (UI) | `hp1006SU.dll` | `zsu_zx.dll`, Marvell Printer Software Driver 2013.0415.2.2628 |
| LanguageMonitor | `hp1006LM.dll` | `zlm_zx.dll`, 2013.0415.2.2628 |
| Color mgmt | `hp1006GC.dll` | `zenocmm.dll` |
| Status monitor | `hp1006SM.exe` | tray/status app |
| Data file | `HP1006SD.SDD` | device description data (not yet parsed) |
| Firmware | `P1005.img`, `P1006.img`, `P1505.img` | "20100824" + big-endian ELF |
| Demo pages | `P100xDP.PRN`, `P1505*DP.PRN` | finished printer streams |

Implied data path (hypothesis, strong): app → EMF spool → ZIMF print processor → `hp1006SD` renders and encodes XQX → **`hp1006LM` language monitor** (bidirectional status; probably firmware download) → USB printer class port → printer.

## Demo page `P1007DP.PRN`: an HP-authored reference stream — CONFIRMED

Analysis: `analysis/reports/P1007DP.xqxdecode.txt`, image `analysis/reports/demo-decode/`.

- PJL job header (`@PJL JOB`, `SET DENSITY=3`, `ECONOMODE=OFF`, `RET=MEDIUM`, `USTATUS ... OFF`, `JOBATTR`)
- `ESC%-12345X` followed by `,XQX` magic
- Big-endian records: `u32 type, u32 item-count`, then items `u32 id, u32 len, value`; each record ends with item `0x80000001 = 0xDEADBEEF`
- START_DOC, START_PAGE (resolution, raster size, paper, source, copies, …), START_PLANE with a **JBIG BIH** (9856×6432, L0=128, options TPBON/DPON/LRLTWO), JBIG data chunks of up to 65536 bytes, END_PLANE, END_PAGE, END_DOC
- PJL `EOJ` trailer
- foo2zjs `xqxdecode` walks the whole file with no errors (108000 bytes of XQX), and its JBIG decoder produces the correct flyer image, 9856×6432 1-bit.

So the raster is standard JBIG1 (the format JBIG-KIT implements), wrapped in XQX framing. Proven by decoding, not guessed from byte patterns.

### Still UNKNOWN from the demo page
- Why `RESOLUTION_Y=400` with `RASTER_X=9856`, `VIDEO_BPP=2`, `VIDEO_X=4923`. The pixel geometry (true DPI per axis, whether it's 2× horizontal oversampling) will be resolved by the single-pixel and rectangle captures at known coordinates.
- Meaning of `0x10000000–0x10000005`, `0x20000007`, `0x40000003`. Resolved by differential captures (Phase 4).
- The demo page is a static file. It doesn't show what the *live driver* sends (status queries, firmware, per-job PJL). Only USB captures show that.

## Firmware — hypothesis (strong), to test
- foo2zjs's udev hook (`/lib/udev/hplj1000`) sends `sihpP1005.dl` to a P1007 at every hot-plug (`FWMODEL=P1005 # Alias`).
- The HP package ships `P1005.img` and no `P1007.img`, which fits the same aliasing.
- On this Linux host **`/var/lib/foo2zjs/firmware/` is empty**, so the P1007 never gets firmware here. If the P1007 needs firmware, that alone would explain unreliable Linux printing.
- Confirm with: the Windows capture from printer power-on (does a ~220 KB bulk-OUT transfer happen, and does it match `P1005.img`?).

## Next static targets (after captures, not before)
1. `HP1006SD.SDD`: look for tables of resolutions, paper sizes, and PJL strings.
2. `hp1006LM.dll`: strings/imports for PJL status queries and firmware-download logic.
3. `hp1006SD.dll`: only if captures leave XQX items unexplained. Ghidra, focused on the XQX record emitter.
