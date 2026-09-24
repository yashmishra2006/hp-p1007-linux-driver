# HP LaserJet P1007 Linux driver

A small CUPS driver for the HP LaserJet P1007 that sends the printer the same PJL/XQX/JBIG stream as HP's Windows driver. It was built from HP's own reference data and tested on real hardware.

```sh
sudo apt install ghostscript libjbig-dev build-essential cups
sudo driver-linux/install.sh            # switches/creates the HP_P1007 queue
lp -d HP_P1007 document.pdf
```

No firmware upload is needed. The format is documented in [`protocol/packet-format.md`](protocol/packet-format.md) and the tests in [`docs/experiments.md`](docs/experiments.md).
HP's own driver files are **not** included (see `docs/driver.md`).

---

# P1007 research: investigation plan and environment inventory

**End goal:** a simple Linux/Raspberry Pi driver that prints as well as the Windows driver: correct geometry, density/economode, paper sizes, multi-page jobs, error and status reporting, and recovery from power cycles.
**Method:** capture what Windows sends → explain every byte → reproduce it → prove equivalence by comparing our output with Windows's output byte for byte.

Status docs: `docs/hardware.md`, `docs/driver.md`, `docs/experiments.md`.

## What we already know (EXP-000, static only)

| Finding | Status |
|---|---|
| The driver is a Zenographics/Marvell "ZX" driver (IMF renderer + `zlm_zx` language monitor), not Unidrv/GPD | CONFIRMED |
| HP's own `P1007DP.PRN` uses PJL + `,XQX` big-endian TLV records + standard JBIG1 raster | CONFIRMED (decoded to a correct image) |
| foo2xqx (already installed here) targets that same XQX format | CONFIRMED that its decoder parses HP's stream. Encoder equivalence UNKNOWN |
| P1007 = USB 03f0:4817 | STRONG HYPOTHESIS |
| P1007 needs P1005 firmware uploaded after every power-on | MEDIUM HYPOTHESIS |
| This Linux host has **no firmware** in `/var/lib/foo2zjs/firmware/`, which may explain current failures | MEDIUM HYPOTHESIS, cheapest thing to test |

## 1. Windows USB capture setup
- **Wireshark** (current) with **USBPcap** selected during install. Reboot after installing.
- Optional: **USBView** (Windows SDK/WDK) to dump descriptors.
- The HP driver installed from the same package (`ljP1000_P1500-HB-pnp-win64-en.exe`) so versions match.
- Copy `tests/windows/print-test.ps1` onto the Windows PC.
- Linux side (for later comparison): `sudo modprobe usbmon` + `sudo apt install tshark`.

## 2. Driver inspection tooling
- Linux (done / available): `7z`, `strings`, `sha256sum`, foo2zjs `xqxdecode`, Python 3 + Pillow.
- To add when needed: `pip install pefile`, and Ghidra (only if captures leave items unexplained).
- Windows: `Get-PrinterDriver -Name "HP LaserJet P1007" | fl *` and `pnputil /enum-drivers` to confirm what is actually installed.

## 3. Information needed from the P1007
- USB descriptors: VID/PID, interface class/subclass/protocol, endpoint addresses and max packet sizes.
- IEEE-1284 device ID string (`usb_printerid`, or it's visible in the capture as a GET_DEVICE_ID control transfer).
- Paper size loaded, and the Windows paper-size and resolution settings used.
- Windows version (`winver`), plus installed driver version (see §2).
- Serial <serial> (already known from the CUPS URI).

## 4. Driver files to extract
Done (see `docs/driver.md`, hashes in `driver/metadata/SHA256SUMS`): INF, `hp1006SD.dll` (renderer), `hp1006LM.dll` (language monitor), `hp1006PP.dll`, `hp1006SU.dll`, `HP1006SD.SDD`, `P1005.img`, all `*DP.PRN`.
From the **Windows PC** also export the installed copies from `C:\Windows\System32\spool\drivers\x64\3\` (hp1006*) and `HKLM\SYSTEM\CurrentControlSet\Control\Print\Printers\HP LaserJet P1007` (registry export). Hash them, then compare with the package.

## 5. The first controlled experiment: EXP-001 "power-on + blank page"
Two questions in one capture: does Windows upload firmware, and what does a minimal job look like?

1. Printer **powered off**, USB connected. Close HP status monitor (`hp1006SM.exe`) if it's running.
2. Wireshark: start capture on the USBPcap interface for the printer's root hub. Include all devices on that hub. Filter later.
3. **Power the printer on.** Wait 60 s, doing nothing. (Captures enumeration, device ID, any firmware upload.)
4. In PowerShell: `.\print-test.ps1 -Test blank -Paper A4 -Dpi 600`
5. Wait until the page is ejected and the LED is steady, plus 10 s. Stop the capture.
6. Save as `captures/blank-page/exp001-poweron-blank.pcapng` with the `.json` sidecar the script writes, plus a phone photo of the output page.
7. Then **without power-cycling**, repeat step 4 in a new capture: `exp001b-warm-blank.pcapng`. This separates first-job-after-power-on behavior from per-job behavior.
8. Also produce `blank.prn` via `-ToFile` (driver output with no USB and no language monitor). Diffing it against the USB bulk-OUT payload shows exactly what the language monitor adds.

## 6. Capture format
- **pcapng** from Wireshark/USBPcap, raw and never edited. Put `sha256sum` alongside it on first import.
- Per capture: the script's JSON sidecar, a photo of the printed page, notes (LED behavior, anything odd).
- Derived artifacts (extracted bulk streams, decodes, diffs) go under `analysis/`, never in `captures/`.

## 7. How the first capture will be analyzed
1. `tshark` → list every transfer: time, endpoint, direction, type (control/bulk), length, status.
2. Control transfers: decode GET_DESCRIPTOR/GET_DEVICE_ID/GET_PORT_STATUS/SOFT_RESET → fill in `docs/hardware.md`.
3. Reassemble bulk-OUT payloads into one byte stream per job (and bulk-IN into the status stream).
4. Firmware test: look for a ~220 KB bulk-OUT burst after power-on. Compare it to `P1005.img` (raw, and after `arm2hpdl`-style wrapping). Identical or not → H2 confirmed or refuted.
5. Run the job stream through `xqxdecode` → record every PJL line and XQX item. Diff it against `P1007DP.PRN` and against foo2xqx's output for an equivalent blank page generated on Linux.
6. Bulk-IN: record PJL USTATUS/INFO replies, the language monitor's status polling cadence, and what "job done" looks like.
7. Write up in `protocol/packet-analysis.md` using the offset/length/direction/example/frequency/meaning/confidence/evidence table.

## 8. Evidence sufficient to start the Linux-side implementation
- The USB interface/endpoints are known, and the firmware question is resolved (sent or not, and exactly what bytes).
- For blank, single-pixel, and rectangle: every PJL line and every XQX item is either explained by a differential capture or explicitly marked constant-copied-from-Windows.
- A pixel at a known (X,Y) is located in the decoded JBIG bitmap. That fixes orientation, origin/margins, and true per-axis resolution (currently UNKNOWN: `RES_Y=400`, `VIDEO_BPP=2`).
- The JBIG parameters (L0, options, stripe layout, chunking) are known for each captured job.
- **Gate:** a Linux-generated stream for the blank/pixel/rectangle tests that is **byte-identical** to Windows after decode (JBIG may differ at the byte level; the decoded bitmaps and all non-raster items must match), and that the printer prints correctly.

Because foo2xqx already implements XQX, the first Linux test is probably not writing code. It's (a) supplying firmware from HP's `P1005.img` and (b) diffing foo2xqx's output against Windows's. We only write our own encoder where they disagree, or if foo2xqx proves unsuitable.

## Waiting on
EXP-001 capture files plus the §3 information. No protocol claims beyond the table above until then.
