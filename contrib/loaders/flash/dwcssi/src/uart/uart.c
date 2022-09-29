#define putchar(x) do { \
    asm volatile("mov x6, #0xf8400000"); \
    asm volatile("mov w7," #x : : : ); \
    asm volatile("str w7, [x6]": : :"memory"); \
} while(0)
