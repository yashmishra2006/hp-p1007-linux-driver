/*
 * hp1007enc - encode 1-bit page bitmaps into the XQX stream used by the
 * HP LaserJet P1007, laid out exactly like HP's own driver output.
 *
 * Reference: P1007DP.PRN from HP's Windows package (driver 1.0.2.2680),
 * which prints correctly on our P1007 (EXP-002). Geometry from HP1006SD.SDD:
 * MarginMetric=47/300 in, PixAlignX=128, PixAlignY=4, ATMOVE HP2=-12.
 * See protocol/packet-format.md.
 *
 * Copyright (C) 2026 Yash Mishra
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Input:  one or more concatenated raw PBM (P4) pages at 1200x600 dpi, each
 *         covering the full sheet (e.g. Ghostscript -sDEVICE=pbmraw -r1200x600).
 * Output: PJL + XQX stream on stdout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <jbig.h>

#define XQX_START_DOC   1
#define XQX_END_DOC     2
#define XQX_START_PAGE  3
#define XQX_END_PAGE    4
#define XQX_START_PLANE 5
#define XQX_END_PLANE   6
#define XQX_JBIG        7

#define ITEM_SIZE       0x80000000u  /* byte size of the item list */
#define ITEM_END        0x80000001u  /* value 0xDEADBEEF */
#define END_MAGIC       0xDEADBEEFu

#define JBIG_CHUNK      65536        /* HP splits BID into 64 KiB records */
#define X_DPI           1200
#define Y_DPI           600
#define MARGIN_300      47           /* HP1006SD.SDD MarginMetric, 1/300 in */

static int  opt_paper = 9, opt_density = 3, opt_media = 1, opt_source = 7;
static int  opt_economode = 0, opt_resy = 400;
static int  opt_xoff = 0, opt_yoff = 0;  /* fine calibration, device pixels */
static const char *opt_ret = "MEDIUM";

static void die(const char *m) { fprintf(stderr, "ERROR: hp1007enc: %s\n", m); exit(1); }

static void be32(uint32_t v) {
    unsigned char b[4] = { v >> 24, v >> 16, v >> 8, v };
    fwrite(b, 1, 4, stdout);
}
static void rec(uint32_t type, uint32_t n) { be32(type); be32(n); }
static void item(uint32_t id, uint32_t v) { be32(id); be32(4); be32(v); }

/* ---- PBM reading ---------------------------------------------------- */
static int pbm_token(FILE *f) {
    int c, v = 0;
    do {
        c = getc(f);
        if (c == '#') while (c != '\n' && c != EOF) c = getc(f);
    } while (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    if (c < '0' || c > '9') return -1;
    while (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); c = getc(f); }
    return v;  /* the single whitespace after the token is consumed */
}

static unsigned char *pbm_read(FILE *f, int *w, int *h) {
    int c1 = getc(f), c2;
    while (c1 == '\n' || c1 == '\r' || c1 == ' ') c1 = getc(f);
    if (c1 == EOF) return NULL;
    c2 = getc(f);
    if (c1 != 'P' || c2 != '4') die("input is not raw PBM (P4)");
    *w = pbm_token(f); *h = pbm_token(f);
    if (*w <= 0 || *h <= 0) die("bad PBM header");
    size_t sz = (size_t)((*w + 7) / 8) * *h;
    unsigned char *p = malloc(sz + 1);   /* +1: copy_bits reads one byte ahead */
    if (!p || fread(p, 1, sz, f) != sz) die("short PBM page");
    return p;
}

/* Copy a w-bit run starting at bit sx of src into dst (starting at bit 0). */
static void copy_bits(unsigned char *dst, const unsigned char *src, int sx, int w) {
    int sb = sx >> 3, sh = sx & 7, nbytes = (w + 7) / 8;
    for (int i = 0; i < nbytes; i++) {
        unsigned v = src[sb + i] << sh;
        if (sh) v |= src[sb + i + 1] >> (8 - sh);
        dst[i] = v & 0xff;
    }
    if (w & 7) dst[nbytes - 1] &= 0xff << (8 - (w & 7));
}

/* ---- JBIG ------------------------------------------------------------ */
struct buf { unsigned char *p; size_t n, cap; };
static void jbig_out(unsigned char *s, size_t len, void *arg) {
    struct buf *b = arg;
    if (b->n + len > b->cap) {
        b->cap = (b->n + len) * 2;
        if (!(b->p = realloc(b->p, b->cap))) die("out of memory");
    }
    memcpy(b->p + b->n, s, len); b->n += len;
}

/* ---- page ------------------------------------------------------------ */
static void emit_page(const unsigned char *pg, int W, int H) {
    int mx = MARGIN_300 * X_DPI / 300;       /* 188 bits  */
    int my = MARGIN_300 * Y_DPI / 300;       /*  94 lines */
    int vbits = W - 2 * mx;                  /* printable width in 1200-dpi bits */
    int video_x = vbits / 2;                 /* HP VIDEO_X is in 600-dpi pixels */
    vbits = video_x * 2;
    int video_y = (H - 2 * my) & ~3;         /* PixAlignY=4 */
    int raster_x = (vbits + 127) & ~127;     /* PixAlignX=128 */
    int raster_y = video_y;
    if (video_x <= 0 || video_y <= 0) die("page smaller than margins");

    int sbpl = (W + 7) / 8, dbpl = raster_x / 8;
    unsigned char *r = calloc((size_t)dbpl, raster_y);
    if (!r) die("out of memory");
    int sx = mx + opt_xoff, sy0 = my + opt_yoff;
    for (int y = 0; y < raster_y; y++) {
        int sy = sy0 + y;
        if (sy < 0 || sy >= H) continue;
        /* clamp horizontal window to the source line */
        int x0 = sx < 0 ? -sx : 0, w = vbits - x0;
        if (sx + x0 + w > W) w = W - (sx + x0);
        if (w <= 0) continue;
        unsigned char tmp[dbpl + 2];
        memset(tmp, 0, sizeof tmp);
        copy_bits(tmp, pg + (size_t)sy * sbpl, sx + x0, w);
        if (x0 == 0) memcpy(r + (size_t)y * dbpl, tmp, (w + 7) / 8);
        else for (int b = 0; b < w; b++)       /* rare: negative x offset */
            if (tmp[b >> 3] & (0x80 >> (b & 7)))
                r[(size_t)y * dbpl + ((b + x0) >> 3)] |= 0x80 >> ((b + x0) & 7);
    }

    struct buf jb = { 0 };
    struct jbg_enc_state s;
    unsigned char *planes[1] = { r };
    jbg_enc_init(&s, raster_x, raster_y, 1, planes, jbig_out, &jb);
    jbg_enc_layers(&s, 0);
    jbg_enc_options(&s, JBG_ILEAVE | JBG_SMID,
                    JBG_LRLTWO | JBG_TPDON | JBG_TPBON | JBG_DPON, 128, 12, 0);
    jbg_enc_out(&s);
    jbg_enc_free(&s);
    free(r);
    if (jb.n < 20) die("JBIG encoder produced no BIH");

    rec(XQX_START_PAGE, 15);
    item(ITEM_SIZE, 15 * 12);
    item(0x20000005, 1);                 /* copies */
    item(0x20000006, opt_source);        /* DMDEFAULTSOURCE, 7 = auto */
    item(0x20000000, opt_media);         /* DMMEDIATYPE, 1 = plain */
    item(0x20000007, 1);
    item(0x20000008, 600);               /* RESOLUTION_X */
    item(0x20000009, opt_resy);          /* HP writes 400 here */
    item(0x2000000d, raster_x);          /* RASTER_X (bits) */
    item(0x2000000e, raster_y);          /* RASTER_Y */
    item(0x2000000a, 2);                 /* VIDEO_BPP */
    item(0x2000000f, video_x);           /* VIDEO_X (600 dpi px) */
    item(0x20000010, video_y);           /* VIDEO_Y */
    item(0x20000011, opt_economode);
    item(0x20000001, opt_paper);         /* DMPAPER */
    item(ITEM_END, END_MAGIC);

    size_t off = 20, left = jb.n - 20;   /* BIH is the first 20 bytes */
    int first = 1;
    while (first || left) {
        size_t n = left > JBIG_CHUNK ? JBIG_CHUNK : left;
        rec(XQX_START_PLANE, 4);
        if (first) {
            item(ITEM_SIZE, 64);
            item(0x40000000, 0);
            be32(0x40000002); be32(20); fwrite(jb.p, 1, 20, stdout);
        } else {
            item(ITEM_SIZE, 48);
            item(0x40000000, 0);
            item(0x40000003, 1);         /* continuation of the same BIE */
        }
        item(ITEM_END, END_MAGIC);
        rec(XQX_JBIG, n);
        fwrite(jb.p + off, 1, n, stdout);
        rec(XQX_END_PLANE, 0);
        off += n; left -= n; first = 0;
    }
    rec(XQX_END_PAGE, 0);
    free(jb.p);
}

static void usage(void) {
    fprintf(stderr,
        "usage: hp1007enc [-p paper] [-d density 1-5] [-e] [-m media] [-s source]\n"
        "                 [-R ret] [-y resy] [-x xoff] [-Y yoff] < pages.pbm > job.prn\n"
        "  paper: 1=Letter 5=Legal 7=Executive 9=A4 11=A5 13=B5 70=A6 (DMPAPER)\n"
        "  input: raw PBM pages at 1200x600 dpi covering the full sheet\n");
    exit(2);
}

int main(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, "p:d:em:s:R:y:x:Y:h")) != -1) switch (c) {
        case 'p': opt_paper = atoi(optarg); break;
        case 'd': opt_density = atoi(optarg); break;
        case 'e': opt_economode = 1; break;
        case 'm': opt_media = atoi(optarg); break;
        case 's': opt_source = atoi(optarg); break;
        case 'R': opt_ret = optarg; break;
        case 'y': opt_resy = atoi(optarg); break;
        case 'x': opt_xoff = atoi(optarg); break;
        case 'Y': opt_yoff = atoi(optarg); break;
        default: usage();
    }
    if (opt_density < 1 || opt_density > 5) usage();

    char ts[16]; time_t t = time(NULL);
    strftime(ts, sizeof ts, "%Y%m%d%H%M%S", localtime(&t));
    printf("\033%%-12345X@PJL JOB\n"
           "@PJL SET JAMRECOVERY=OFF\n"
           "@PJL SET DENSITY=%d\n"
           "@PJL SET ECONOMODE=%s\n"
           "@PJL SET RET=%s\n"
           "@PJL USTATUS DEVICE = OFF\n"
           "@PJL USTATUS JOB = OFF\n"
           "@PJL USTATUS PAGE = OFF\n"
           "@PJL SET JOBATTR=\"JobAttr4=%s\"\n"
           "\033%%-12345X,XQX",
           opt_density, opt_economode ? "ON" : "OFF", opt_ret, ts);

    rec(XQX_START_DOC, 7);
    item(ITEM_SIZE, 7 * 12);
    item(0x10000005, 1);
    item(0x10000001, 0);
    item(0x10000002, 0);                 /* DMDUPLEX */
    item(0x10000000, 0);
    item(0x10000003, 1);
    item(ITEM_END, END_MAGIC);

    int w, h, pages = 0;
    unsigned char *pg;
    while ((pg = pbm_read(stdin, &w, &h))) {
        emit_page(pg, w, h);
        free(pg);
        fprintf(stderr, "PAGE: %d 1\n", ++pages);   /* CUPS page accounting */
    }
    if (!pages) die("no pages in input");

    rec(XQX_END_DOC, 0);
    printf("\033%%-12345X@PJL EOJ\n\033%%-12345X");
    return fflush(stdout) ? 1 : 0;
}
