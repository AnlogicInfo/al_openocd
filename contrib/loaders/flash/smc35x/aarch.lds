OUTPUT_ARCH(aarch64)

SECTIONS
{
  . = 0x61000000;
  .text : {
    *(.text.entry)
    *(.text)
    }

  .data : { *(.data) }
}
