/*
 * Freestanding kernel entry for the ISO build.
 * Draws a VGA text banner; React UI is staged under /ui on the ISO for later.
 */

typedef unsigned short u16;
typedef unsigned int u32;

enum {
    VGA_WIDTH = 80,
    VGA_HEIGHT = 25,
    VGA_COLOR = 0x0A /* light green on black */
};

static volatile u16 *const vga = (volatile u16 *)0xB8000;

static void vga_clear(void)
{
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = (u16)((VGA_COLOR << 8) | ' ');
    }
}

static void vga_write(u32 row, u32 col, const char *s)
{
    u32 i = row * VGA_WIDTH + col;
    while (*s && i < VGA_WIDTH * VGA_HEIGHT) {
        vga[i++] = (u16)((VGA_COLOR << 8) | (unsigned char)*s++);
    }
}

void kmain(void)
{
    vga_clear();
    vga_write(1, 2, "Terminal OS");
    vga_write(3, 2, "ISO kernel online.");
    vga_write(5, 2, "Shared shell + React UI ship in /ui on this disc.");
    vga_write(7, 2, "Windows host build lives in winbuild/.");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
