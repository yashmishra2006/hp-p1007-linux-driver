# hp1007: CUPS driver for the HP LaserJet P1007

Sends the printer the same stream HP's Windows driver does (see `../protocol/packet-format.md`):
PJL header → XQX records → JBIG1 raster at 1200×600 dpi, cropped to HP's 4 mm margins and split into 64 KiB chunks.

| File | Role |
|---|---|
| `hp1007enc.c` | Encoder: raw PBM pages (1200×600, full sheet) → P1007 stream. Needs `libjbig-dev`. |
| `hp1007` | CUPS filter: PDF → Ghostscript → `hp1007enc`. Reads PageSize, Density, EconoMode, InputSlot. |
| `hp1007.ppd` | Queue description: A4 default, Letter/Legal/Executive/A5/B5/A6, density 1–5, EconoMode, manual feed. |
| `install.sh` / `uninstall.sh` | Install the filter and PPD and switch the queue (default `HP_P1007`); the old PPD is saved for uninstall. |

Install: `sudo ./install.sh`. Then print from any app, or `lp -d HP_P1007 file.pdf`.

Standalone (no install): `PPD=hp1007.ppd ./hp1007 1 me t 1 "PageSize=A4" file.pdf > job.prn && lp -d HP_P1007 -o raw job.prn`

No firmware upload is needed (tested from cold power-on). On a Raspberry Pi: `apt install ghostscript libjbig-dev build-essential cups`, then the same install.
