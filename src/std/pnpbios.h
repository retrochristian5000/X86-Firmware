#ifndef __PNPHEADER_H
#define __PNPHEADER_H

#define PNP_SIGNATURE 0x506e5024 // $PnP

struct pnpheader {
    u32 signature;
    u8 version;
    u8 length;
    u16 control;
    u8 checksum;
    u32 eventloc;
    u16 real_ip;
    u16 real_cs;
    u16 prot_ip;
    u32 prot_base;
    u32 oemid;
    u16 real_ds;
    u32 prot_database;
} PACKED;

#define FUNCTION_NOT_SUPPORTED 0x82
#define PNP_INVALID_HANDLE     0x83
#define PNP_BAD_PARAMETER      0x84
#define PNP_SET_FAILED         0x85
#define PNP_BUFFER_TOO_SMALL   0x8c
#define PNP_NODE_END           0xff

#endif // pnpheader.h
