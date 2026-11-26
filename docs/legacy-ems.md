# EMS, UMB and extended-memory compatibility

EMM386 implements EMS using x86 paging and XMS backing storage. SeaBIOS
does not install a second EMS manager or claim INT 67h. NOEMS is a useful
diagnostic comparison, not a requirement for Windows 9x hardware discovery.

INT 15h/AH=88h reports contiguous usable E820 RAM above 1 MiB, capped at
63 MiB. E801 reports independent contiguous ranges starting at 1 MiB
and 16 MiB, with 1 KiB and 64 KiB units respectively. Reserved, ACPI,
NVS and missing ranges terminate the corresponding legacy count.
E820 itself remains authoritative and unchanged.

For Intel PAM, the final shadowing policy classifies each 16 KiB block
from C0000h through EFFFFh independently. Deployed option ROMs and
permanent firmware code remain read-only except where sharing a block
with live writable BIOS data makes that impossible. The existing dummy
option-ROM signature protects upper-memory runtime allocations.
WRITABLE_UPPERMEMORY still defaults to disabled; only wholly unused
blocks may be made writable when enabled. MALLOC_UPPERMEMORY controls
placement of firmware allocations, not whether EMS is enabled.

Do not force EMM386 I= or FRAME= ranges into firmware or device windows.
Compare otherwise identical cold boots, including one without EMM386,
and collect MEM /C /P, MEM /D, EMM386 and the BIOS memory report. Compare
the exact NDIS and Device Manager messages separately. No specific EMS
frame is promised; adapter ROMs and the selected firmware determine it.

References:
- Microsoft KB Q78557, EMM386 Troubleshooting and Technical Information:
  https://jeffpar.github.io/kbarchive/kb/078/Q78557/
- Ralf Brown Interrupt List, INT 15h AX=E801h:
  https://delorie.com/djgpp/doc/rbinter/id/48/17.html
