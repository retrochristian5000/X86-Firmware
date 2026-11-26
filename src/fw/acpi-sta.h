// ACPI device status helpers for the limited firmware AML parser.
// This file may be distributed under the terms of the GNU LGPLv3 license.
#ifndef __ACPI_STA_H
#define __ACPI_STA_H

#define ACPI_STA_PRESENT 0x01
#define ACPI_STA_DEFAULT 0x0f
#define ACPI_STA_UNKNOWN (-1)

// Only literal AML integers are supported. Consume the entire encoding,
// retaining the low status byte; upper bits are not device-status flags.
static inline int
acpi_sta_integer(const unsigned char *ptr, int length, int *status)
{
    if (length < 1)
        return 0;
    int bytes;
    switch (ptr[0]) {
    case 0x00:
        *status = 0;
        return 1;
    case 0x01:
        *status = 1;
        return 1;
    case 0xff:
        *status = 0xff;
        return 1;
    case 0x0a: bytes = 1; break;
    case 0x0b: bytes = 2; break;
    case 0x0c: bytes = 4; break;
    case 0x0e: bytes = 8; break;
    default:
        return 0;
    }
    if (length <= bytes)
        return 0;
    *status = ptr[1];
    return bytes + 1;
}

// MethodFlags followed by exactly one Return of a literal integer.
// Do not evaluate AML, access hardware, or guess dynamic method results.
static inline int
acpi_sta_method(const unsigned char *ptr, int length)
{
    int status;
    if (length < 3 || (ptr[0] & 7) || ptr[1] != 0xa4)
        return ACPI_STA_UNKNOWN;
    int used = acpi_sta_integer(ptr + 2, length - 2, &status);
    return used && used == length - 2 ? status : ACPI_STA_UNKNOWN;
}

static inline int
acpi_sta_present(int status)
{
    return status < 0 ? ACPI_STA_UNKNOWN : !!(status & ACPI_STA_PRESENT);
}

#endif
