#!/usr/bin/env python3
"""Independent XQX record dumper (not derived from foo2zjs). Usage: xqxdump.py file.prn"""
import struct, sys
NAMES = {1:'START_DOC',2:'END_DOC',3:'START_PAGE',4:'END_PAGE',5:'START_PLANE',6:'END_PLANE',7:'JBIG'}
d = open(sys.argv[1],'rb').read()
m = d.find(b',XQX'); print(f'PJL header: {m-9} bytes (UEL at {m-9})'); p = m+4
while p+8 <= len(d):
    if d[p] == 0x1b: print(f'@{p}: trailer {d[p:]!r}'); break
    t, n = struct.unpack('>II', d[p:p+8])
    if t == 7:
        print(f'@{p}: JBIG {n} bytes, head {d[p+8:p+16].hex()} tail {d[p+8+n-6:p+8+n].hex()}'); p += 8+n; continue
    print(f'@{p}: {NAMES.get(t,hex(t))} items={n}'); p += 8
    for _ in range(n):
        i, l = struct.unpack('>II', d[p:p+8]); v = d[p+8:p+8+l]
        print(f'    {i:#010x} len={l} ' + (f'val={struct.unpack(">I",v)[0]}' if l==4 else f'hex={v.hex()}')); p += 8+l
