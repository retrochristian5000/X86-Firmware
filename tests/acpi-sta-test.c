// Standalone regression for the production status decoder.
// Run: cc -std=c99 -Wall -Wextra -Werror tests/acpi-sta-test.c -o /tmp/acpi-sta-test
// SPDX-License-Identifier: LGPL-3.0-only
#include <assert.h>
#include "../src/fw/acpi-sta.h"

int main(void)
{
    int status = ACPI_STA_UNKNOWN;
    const unsigned char zero[] = { 0 };
    const unsigned char one[] = { 1 };
    const unsigned char ones[] = { 0xff };
    const unsigned char byte[] = { 0x0a, 8 };
    const unsigned char word[] = { 0x0b, 1, 0 };
    const unsigned char dword[] = { 0x0c, 15, 0, 0, 0 };
    const unsigned char qword[] = { 0x0e, 3, 0, 0, 0, 0, 0, 0, 0 };
    const unsigned char method[] = { 0, 0xa4, 0x0a, 8 };
    const unsigned char dynamic[] = { 0, 0xa4, 'A', 'B', 'C', 'D' };
    const unsigned char trailing[] = { 0, 0xa4, 1, 0 };
    const unsigned char args[] = { 1, 0xa4, 1 };

    assert(acpi_sta_present(ACPI_STA_DEFAULT) == 1);
    assert(acpi_sta_present(ACPI_STA_UNKNOWN) == -1);
    assert(acpi_sta_present(8) == 0);
    assert(acpi_sta_present(1) == 1);
    assert(acpi_sta_present(2) == 0);
    assert(acpi_sta_present(15) == 1);
    assert(acpi_sta_integer(zero, sizeof(zero), &status) == 1 && !status);
    assert(acpi_sta_integer(one, sizeof(one), &status) == 1 && status == 1);
    assert(acpi_sta_integer(ones, sizeof(ones), &status) == 1 && status == 255);
    assert(acpi_sta_integer(byte, sizeof(byte), &status) == 2 && status == 8);
    assert(acpi_sta_integer(word, sizeof(word), &status) == 3 && status == 1);
    assert(acpi_sta_integer(dword, sizeof(dword), &status) == 5 && status == 15);
    assert(acpi_sta_integer(qword, sizeof(qword), &status) == 9 && status == 3);
    assert(!acpi_sta_integer(qword, sizeof(qword) - 1, &status));
    assert(!acpi_sta_integer(zero, 0, &status));
    assert(acpi_sta_method(method, sizeof(method)) == 8);
    assert(acpi_sta_method(method, sizeof(method) - 1) == -1);
    assert(acpi_sta_method(dynamic, sizeof(dynamic)) == -1);
    assert(acpi_sta_method(trailing, sizeof(trailing)) == -1);
    assert(acpi_sta_method(args, sizeof(args)) == -1);
    return 0;
}
