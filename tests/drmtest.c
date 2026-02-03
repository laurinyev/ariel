#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define __DAZZLE_IMPL__
#define __BT_IMPL__

#include <bt.h>


int init_drm() {
    int fd = -1;
    for (int i = 0; i < 10; i++) {
        char name[32];
        sprintf(name, "/dev/dri/card%d", i);
        fd = open(name, O_RDWR | O_CLOEXEC);
        if (fd != -1) {
            printf("Found and opened drm device \"%s\"\n", name);
            break;
        }
    }
    if (fd == -1) {
        printf("Failed to open drm device\n");
        return 1;
    }

    drmVersionPtr version = drmGetVersion(fd);
    if (version) {
        printf("Driver version: %d.%d.%d\n",version->version_major, version->version_minor, version->version_patchlevel);
        printf("Driver name: %s\n", version->name);
        printf("Driver date: %s\n", version->date);
        printf("Driver description: %s\n", version->desc);
        drmFreeVersion(version);
    }

    uint64_t cap;
    int ret = drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &cap);
    if (ret != 0 || cap == 0) {
        printf("Dumb buffer unsupported\n");
        return -1;
    }

    return fd;
}

typedef struct {
    int count;
    drmModeConnector **connectors;
} connected_connectors;

connected_connectors enumerate_resources(int fd) {
    drmModeRes *resources = drmModeGetResources(fd);

    if (resources->count_connectors == 0) {
        return (connected_connectors){0, NULL};
    }

    int count_connectors = 0;
    drmModeConnector **connectors = NULL;

    for (int i = 0; i < resources->count_connectors; ++i) {
        drmModeConnector *conn = drmModeGetConnector(fd, resources->connectors[i]);
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            count_connectors++;
            connectors = realloc(connectors, count_connectors * sizeof(drmModeConnector*));
            connectors[count_connectors - 1] = conn;
        } else {
            drmModeFreeConnector(conn);
        }
    }

    for(int i = 0; i < count_connectors; i++) {
        drmModeConnector *conn = connectors[i];
        printf("Connector %d modes:\n", i);
        for (int j = 0; j < conn->count_modes; ++j) {
            printf("    Mode %d: %dx%d %dHz\n", j, conn->modes[j].hdisplay, conn->modes[j].vdisplay, conn->modes[j].vrefresh);
        }
    }

    printf("Number of connected connectors: %d\n", count_connectors);

    return (connected_connectors){count_connectors, connectors}; // WIP
}

typedef struct {
    drmModeCrtc *crtc;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
    uint32_t fb_id;
    uint64_t offset;
    uint8_t *fb_ptr;
} framebuffer;

framebuffer setup_framebuffer(int fd, drmModeConnector *connector,int mode_num) {
    drmModeEncoder* encoder = drmModeGetEncoder(fd, connector->encoders[0]);

    if (encoder == NULL) {
        printf("Failed to get encoder\n");
        return (framebuffer){};
    }

    printf("Using encoder %d\n", encoder->encoder_id);

    drmModeCrtc* crtc = drmModeGetCrtc(fd, encoder->crtc_id);

    if (crtc == NULL) {
        printf("Failed to get crtc\n");
        return (framebuffer){};
    }

    printf("Using CRTC %d\n", crtc->crtc_id);

    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
    if(drmModeCreateDumbBuffer(fd, connector->modes[mode_num].hdisplay, connector->modes[mode_num].vdisplay, 32,0 , &handle, &pitch, &size)){
        printf("Failed to create dumb buffer\n");
        return (framebuffer){};
    }

    uint32_t fb;
    if (drmModeAddFB(fd, connector->modes[mode_num].hdisplay, connector->modes[mode_num].vdisplay, 24, 32, pitch, handle, &fb)){
        printf("Failed to add framebuffer\n");
        return (framebuffer){};
    }

    uint64_t offset;
    if (drmModeMapDumbBuffer(fd, handle, &offset)) {
        printf("Failed to prepare dumb buffer for use\n");
        return (framebuffer){};
    }

    uint8_t *fb_ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
    if (fb_ptr == MAP_FAILED) {
        printf("Failed to mmap framebuffer\n");
        return (framebuffer){};
    }

    return (framebuffer){crtc,handle, pitch, size, fb, offset, fb_ptr};
}


int main(int argc,char** argv) {

    int fd = init_drm();

    int mode_id = 0;

    if (argc > 1){
   	mode_id = atoi(argv[1]);
    }
    
    

    connected_connectors connectors = enumerate_resources(fd);

    if (connectors.count == 0) {
        printf("No connector found\n");
        return 1;
    }

    
    if (!drmIsMaster(fd)){
        printf("Process is not the DRM master\n");
        return 1;
    }
    
    for (int i = 0; i < connectors.count; i++) {
        drmModeConnector *connector = connectors.connectors[i];
        printf("Modesetting connector %d: %dx%d\n", connector->connector_id, connector->modes[mode_id].hdisplay, connector->modes[mode_id].vdisplay);

        framebuffer fb = setup_framebuffer(fd, connector,mode_id);

        dazzle_allocator_t alloc;
        alloc.malloc = malloc;
        alloc.free = free;
        
        dazzle_framebuffer_t daz_fb;
        daz_fb.address         = (uintptr_t)fb.fb_ptr;
        daz_fb.width           = connector->modes[mode_id].hdisplay;
        daz_fb.height          = connector->modes[mode_id].vdisplay;
        daz_fb.pitch           = fb.pitch;
        daz_fb.bpp             = 32;
        daz_fb.red_mask        = 0xFF; 
        daz_fb.green_mask      = 0xFF;
        daz_fb.blue_mask       = 0xFF;
        daz_fb.alpha_mask      = 0xFF;
        daz_fb.red_shift       = 16;
        daz_fb.green_shift     = 8;
        daz_fb.blue_shift      = 0;
        daz_fb.alpha_shift     = 24;
        
        dazzle_context_t* ctx = dazzle_init_fb(alloc, &daz_fb);
        
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
    
        printf("Framebuffer resolution: %dx%d\n",daz_fb.width,daz_fb.height);
        printf("Font type: %s\n", font.format == BT_FORMAT_PSF1 ? "PSF1" : 
                                  font.format == BT_FORMAT_PSF2 ? "PSF2" : 
                                  font.format == BT_FORMAT_TTF ? "TTF" : "Unknown");
        printf("Glyph count: %d\n", font.glyph_count);
        printf("Suggested width: %d\n", font.suggested_width);
        printf("Suggested height: %d\n", font.suggested_height);
        printf("bytes per glyph: %d\n", font.psfx_bytes_per_glyph);
        
        bt_terminal_t term;
        bt_terminal_init(&term, ctx, font, daz_fb.width, daz_fb.height);
        bt_terminal_set_colors(&term, 0x000000FF, 0x00000000);
        bt_terminal_clear(&term);

        for (int i = 0; i < font.glyph_count; i++) {
            bt_terminal_put_index(&term, i);
        }

        if(drmModeSetCrtc(fd, fb.crtc->crtc_id, fb.fb_id, 0, 0, &connector->connector_id, 1, &connector->modes[mode_id])){
            printf("Failed to modeset crtc\n");
            return 1;
        }
    }

    pause();

    return 0;

}
