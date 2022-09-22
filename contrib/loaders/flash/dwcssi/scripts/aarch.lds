OUTPUT_ARCH(aarch64)

SECTIONS
{
  . = 0x12340000;
  .text : { *(.text.boot) }

  .data : { *(.data) }
}
