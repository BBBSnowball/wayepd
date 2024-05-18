#include <stdio.h>
#include <stdarg.h>
extern "C" {
#include <neatvnc.h>
#define AML_UNSTABLE_API 1
#include <aml.h>

#include <pixman.h>
#include "fb.h"
#include <libdrm/drm_fourcc.h>
}

#pragma GCC diagnostic ignored "-Wunused-parameter"

extern const char nvnc_version[] = "0.1-dummy";

enum nvnc_log_level log_level = NVNC_LOG_INFO;

void nvnc_set_log_level(enum nvnc_log_level l) {
    log_level = l;
}

void nvnc__log(const struct nvnc_log_data* d, const char* fmt, ...) {
    if (d && d->level > log_level)
        return;

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

/*
struct nvnc_common { // copied from neatvnc, must match the one in fb.h
	void* userdata;
	nvnc_cleanup_fn cleanup_fn;

    nvnc_common() {
        userdata = NULL;
        cleanup_fn = NULL;
    }

    ~nvnc_common() {
        if (cleanup_fn)
            cleanup_fn(userdata);
    }
};
*/
struct nvnc_common2 : nvnc_common {
    nvnc_common2() {
        userdata = NULL;
        cleanup_fn = NULL;
    }

    ~nvnc_common2() {
        if (cleanup_fn)
            cleanup_fn(userdata);
    }
};

struct nvnc_client {
    struct nvnc_common2 common;
    struct nvnc* parent;
};

struct nvnc_display {
    struct nvnc_common2 common;
    uint16_t x_pos;
    uint16_t y_pos;
};

struct nvnc {
    struct nvnc_common2 common;

    nvnc_client client;
    nvnc_display display;
    nvnc_client_fn client_fn;
};

struct nvnc* nvnc_open_generic() {
    auto self = new nvnc();
    self->client.parent = self;
    return self;
}

struct nvnc* nvnc_open(const char* addr, uint16_t port) {
    (void)addr; (void)port;
    return nvnc_open_generic();
}

struct nvnc* nvnc_open_unix(const char *addr) {
    (void)addr;
    return nvnc_open_generic();
}

struct nvnc* nvnc_open_websocket(const char* addr, uint16_t port) {
    (void)addr; (void)port;
    return nvnc_open_generic();
}

void nvnc_close(struct nvnc* self) {
    delete self;
}

void nvnc_set_userdata(void* self, void* userdata, nvnc_cleanup_fn cleanup_fn) {
    auto self2 = (nvnc_common*)self;
    if (self2->cleanup_fn)
        self2->cleanup_fn(self2->userdata);
    self2->userdata = userdata;
    self2->cleanup_fn = cleanup_fn;
}

void* nvnc_get_userdata(const void* self) {
    return ((nvnc_common*)self)->userdata;
}

bool nvnc_has_auth(void) {
    return false;
}

int nvnc_enable_auth(struct nvnc* self, enum nvnc_auth_flags flags,
		nvnc_auth_fn, void* userdata) {
    return -1;
}

void nvnc_set_name(struct nvnc* self, const char* name) {}
void nvnc_set_key_fn(struct nvnc* self, nvnc_key_fn) {}
void nvnc_set_key_code_fn(struct nvnc* self, nvnc_key_fn) {}
void nvnc_set_pointer_fn(struct nvnc* self, nvnc_pointer_fn) {}
void nvnc_set_fb_req_fn(struct nvnc* self, nvnc_fb_req_fn) {}
void nvnc_set_client_cleanup_fn(struct nvnc_client* self, nvnc_client_fn fn) {}
void nvnc_set_cut_text_fn(struct nvnc*, nvnc_cut_text_fn fn) {}
void nvnc_set_desktop_layout_fn(struct nvnc* self, nvnc_desktop_layout_fn) {}
int nvnc_set_tls_creds(struct nvnc* self, const char* privkey_path,
                     const char* cert_path) { return 0; }
int nvnc_set_rsa_creds(struct nvnc* self, const char* private_key_path) { return 0; }

const char* nvnc_client_get_hostname(const struct nvnc_client* client) {
    return "dummyhost";
}
const char* nvnc_client_get_auth_username(const struct nvnc_client* client) {
    return "user";
}
void nvnc_send_cut_text(struct nvnc*, const char* text, uint32_t len) {}

struct nvnc_client* nvnc_client_first(struct nvnc* self) {
    return &self->client;
}
struct nvnc_client* nvnc_client_next(struct nvnc_client* client) {
    return NULL;
}
void nvnc_client_close(struct nvnc_client* client) {
}

struct nvnc* nvnc_client_get_server(const struct nvnc_client* client) {
    return const_cast<struct nvnc*>(client->parent);
}

static struct nvnc_display display_singleton;

struct nvnc_display* nvnc_display_new(uint16_t x_pos, uint16_t y_pos) {
    return &display_singleton;
}
void nvnc_display_ref(struct nvnc_display*) {
}
void nvnc_display_unref(struct nvnc_display*) {
}

void nvnc_add_display(struct nvnc*, struct nvnc_display*) {
}

void nvnc_remove_display(struct nvnc*, struct nvnc_display*) {
}

static void on_timer(void* obj) {
	struct nvnc* self = (struct nvnc*)aml_get_userdata(obj);
    printf("simulating connected client, self=%p\n", self);
    self->client_fn(&self->client);
}

/*
extern "C" void nvnc_ugly_hack__before_mainloop(struct nvnc* self);
void nvnc_ugly_hack__before_mainloop(struct nvnc* self) {
    on_timer(self);
}
*/

void nvnc_set_new_client_fn(struct nvnc* self, nvnc_client_fn fn) {
    //fn(&self->client);  // crashes because caller isn't ready
    self->client_fn = fn;

	auto x = aml_timer_new(1000, on_timer, self, NULL);
	aml_start(aml_get_default(), x);
    printf("set timer for new client, %p; self=%p\n", x, self);

    //auto x2 = aml_ticker_new(1000000, on_timer, self, NULL);
    //printf("set ticker for new client, %p\n", x);
}

void nvnc_display_feed_buffer(struct nvnc_display* display, struct nvnc_fb* fb,
			      struct pixman_region16* damage) {
    printf("nvnc_display_feed_buffer\r\n");
    printf("  damage:\n    extents: x1=%d, y1=%d, x2=%d, y2=%d\n",
        damage->extents.x1, damage->extents.y1, damage->extents.x2, damage->extents.y2);
    int num = 0;
    pixman_box16_t* rects = pixman_region_rectangles(damage, &num);
    for (int i=0; i<num; i++) {
        printf("    rect: x1=%d, y1=%d, x2=%d, y2=%d\n",
            rects[i].x1, rects[i].y1, rects[i].x2, rects[i].y2);
    }

    //if (fb->width != 280 || fb->height != 480 || fb->fourcc_format != DRM_FORMAT_XRGB8888) {
    if (fb->fourcc_format != DRM_FORMAT_XRGB8888 && fb->fourcc_format != DRM_FORMAT_XBGR8888) {
        printf("    unexpected framebuffer properties\n");
        return;
    }

    auto pixels = (uint8_t*)nvnc_fb_get_addr(fb);
    auto stride = nvnc_fb_get_stride(fb);
    auto ps = nvnc_fb_get_pixel_size(fb);
    if (!pixels) {
        printf("    no data\n");
        return;
    }
    auto f = fopen("current.ppm", "w");
    // copied from https://rosettacode.org/wiki/Bitmap/Write_a_PPM_file#C
    (void) fprintf(f, "P6\n%d %d\n255\n", fb->width, fb->height);
    for (int y=0; y<fb->height; y++) {
        for (int x=0; x<fb->width; x++) {
            auto pixel = pixels + y*stride*ps + x*ps;
            if (fb->fourcc_format == DRM_FORMAT_XBGR8888) {
                fwrite(pixel, 3, 1, f);
            } else {
                uint8_t rgb[3] = { pixel[2], pixel[1], pixel[0] };
                fwrite(rgb, 3, 1, f);
            }
        }
    }
    fclose(f);

    const int dimx = 800, dimy = 800;
    int i, j;
    FILE *fp = fopen("first.ppm", "wb"); /* b - binary mode */
    (void) fprintf(fp, "P6\n%d %d\n255\n", dimx, dimy);
    for (j = 0; j < dimy; ++j) {
        for (i = 0; i < dimx; ++i) {
            static unsigned char color[3];
            color[0] = i % 256;  /* red */
            color[1] = j % 256;  /* green */
            color[2] = (i * j) % 256;  /* blue */
            (void) fwrite(color, 1, 3, fp);
        }
    }
    (void) fclose(fp);
}
