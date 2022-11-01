OUTPUT_ARCH(aarch64)

SECTIONS
{
  . = 0x61000040;
  .text : {
    *(.text.entry)
    *(.text)
    }

  .data : { *(.data) }
}
