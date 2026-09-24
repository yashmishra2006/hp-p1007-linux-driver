# Hardware / environment inventory

```text
Printer:          HP LaserJet P1007, serial <serial> (from the existing CUPS queue URI on the Linux host)
USB VID:          0x03F0 (HP). CONFIRMED by the INF/installer list
USB PID:          0x4817. CONFIRMED (lsusb)
USB interfaces:   1 interface, class 07/01/02 (printer, bidirectional). CONFIRMED
USB endpoints:    bulk OUT 0x01 and bulk IN 0x81, 512 bytes each. CONFIRMED
Device ID string: MFG:Hewlett-Packard;MDL:HP LaserJet P1007;CMD:HBS,PJL,ACL;CLS:PRINTER;DES:HP LaserJet P1007;FWVER:20080415;
Windows version:  UNKNOWN (the Windows PC hasn't been inspected yet)
Driver version:   1.0.2.2680 (INF DriverVer 04/15/2013). What is installed on Windows is UNKNOWN
Driver package:   ljP1000_P1500-HB-pnp-win64-en.exe (sha256 36f023ac…fa527)
Relevant files:   see docs/driver.md
```

## Linux host (this machine, 2026-09-24)
- Ubuntu 24.04.5, kernel 6.8.0-138, x86_64
- CUPS queue `HP_P1007`, URI `usb://HP/LaserJet%20P1007?serial=<serial>`, driver "Foomatic/foo2xqx (recommended)", state **paused**
- printer-driver-foo2zjs 20200505, hplip 3.23.12 installed; `xqxdecode`, `usb_printerid`, `arm2hpdl` available
- `/var/lib/foo2zjs/firmware/` is **empty** (see the firmware hypothesis in driver.md)
- `usbmon` not loaded, and no Wireshark installed
- Printer **not currently connected** (not in `lsusb`)

## Info needed from the physical P1007
1. `lsusb -v -d 03f0:` output (on Linux), or USBView / Device Manager hardware IDs (on Windows)
2. IEEE-1284 device ID: `usb_printerid /dev/usb/lp0`
3. Firmware state: does the orange/green LED behavior change on first print after power-on?
4. Paper size loaded (A4 or Letter)
5. A printer self-test/config page, if the printer can produce one (hold the button). It shows the firmware version and page count.
