#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#define __DAZZLE_IMPL__
#define __BT_IMPL__
#include <bt.h>

int main(int argc, char **argv) {

    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
	printf("Failed to open framebuffer!");
	return 1;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("Failed to read fixed screen info");
        close(fbfd);
        return 1;
    }

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("Failed to read variable screen info");
        close(fbfd);
        return 1;
    }

    size_t screensize = finfo.line_length * vinfo.yres;
    uint8_t* fbmem = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbmem == MAP_FAILED) {
        perror("Failed to mmap framebuffer");
       close(fbfd);
       return 1;
    }

	
    dazzle_allocator_t alloc;
    alloc.malloc = malloc;
    alloc.free = free;

    dazzle_framebuffer_t fb;
    fb.address         = (uintptr_t)fbmem;
    fb.width           = vinfo.xres;
    fb.height          = vinfo.yres;
    fb.pitch           = finfo.line_length;
    fb.bpp             = vinfo.bits_per_pixel;
    fb.red_mask        = 0xFF; 
    fb.green_mask      = 0xFF;
    fb.blue_mask       = 0xFF;
    fb.alpha_mask      = 0xFF;
    fb.red_shift       = vinfo.red.offset;
    fb.green_shift     = vinfo.green.offset;
    fb.blue_shift      = vinfo.blue.offset;
    fb.alpha_shift     = vinfo.transp.offset;

    dazzle_context_t* ctx = dazzle_init_fb(alloc, &fb);

    FILE* f = fopen("test.psf", "rb");
    if(f == NULL) {
        printf("Failed to open test.psf\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    int fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* psf = malloc(fsize);
    fread(psf, fsize, 1, f);
    fclose(f);

    font_t font = load_font(alloc, psf, fsize);

    printf("Framebuffer resolution: %dx%d\n",fb.width,fb.height);
    printf("Font type: %s\n", font.format == BT_FORMAT_PSF1 ? "PSF1" : 
                              font.format == BT_FORMAT_PSF2 ? "PSF2" : 
                              font.format == BT_FORMAT_TTF ? "TTF" : "Unknown");
    printf("Glyph count: %d\n", font.glyph_count);
    printf("Suggested width: %d\n", font.suggested_width);
    printf("Suggested height: %d\n", font.suggested_height);
    printf("bytes per glyph: %d\n", font.psfx_bytes_per_glyph);

    bt_terminal_t term;
    bt_terminal_init(&term, ctx, font, fb.width, fb.height);
    bt_terminal_set_colors(&term, 0x000000FF, 0x00000000);
    bt_terminal_clear(&term);

    for (int i = 0; i < font.glyph_count; i++) {
        bt_terminal_put_index(&term, i);
    }

    while (true){
	//nothing to do for now
    }
}
