OUTPUT_ARCH( "riscv" )

SECTIONS
{
  . = 0x61000000;
  .text :
  {
    *(.text.entry)
    *(.text)
  }
  .data : { *(.data) }
}
