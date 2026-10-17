// SPDX-License-Identifier: GPL-3.0-or-later
// A static native Wayland surface for measuring the compositor's final output.
#define _GNU_SOURCE
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static struct wl_display *display;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct xdg_wm_base *wm;
static struct wl_surface *surface;
static struct xdg_surface *xdg;
static int width = 900, height = 600;

static void ping(void *data, struct xdg_wm_base *base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(base, serial);
}
static const struct xdg_wm_base_listener wm_listener = {.ping = ping};

static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                   uint32_t version) {
    (void)data;
    (void)version;
    if (!strcmp(interface, "wl_compositor"))
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    else if (!strcmp(interface, "wl_shm"))
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    else if (!strcmp(interface, "xdg_wm_base")) {
        wm = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(wm, &wm_listener, NULL);
    }
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}
static const struct wl_registry_listener registry_listener = {.global = global,
                                                              .global_remove = removed};

static void configured(void *data, struct xdg_surface *configured_surface, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(configured_surface, serial);
    size_t length = (size_t)width * height * 4;
    int fd = memfd_create("tapzap-test-swatches", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, (off_t)length) != 0)
        exit(2);
    uint32_t *pixels = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED)
        exit(2);
    const uint32_t colours[] = {0xffffff, 0x808080, 0xff0000, 0x00ff00, 0x0000ff, 0x000000};
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            pixels[(size_t)y * width + x] = colours[(size_t)x * 6 / width];
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int)length);
    struct wl_buffer *buffer =
        wl_shm_pool_create_buffer(pool, 0, width, height, width * 4, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    munmap(pixels, length);
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, width, height);
    wl_surface_commit(surface);
    // The display server keeps the attached buffer alive after this destructor.
    wl_buffer_destroy(buffer);
}
static const struct xdg_surface_listener surface_listener = {.configure = configured};

static void resized(void *data, struct xdg_toplevel *top, int32_t w, int32_t h,
                    struct wl_array *states) {
    (void)data;
    (void)top;
    (void)states;
    if (w > 0 && h > 0 && w <= 4096 && h <= 4096) {
        width = w;
        height = h;
    }
}
static void closed(void *data, struct xdg_toplevel *top) {
    (void)data;
    (void)top;
    exit(0);
}
static const struct xdg_toplevel_listener top_listener = {.configure = resized, .close = closed};

int main(void) {
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Cannot connect to Wayland\n");
        return 1;
    }
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    if (wl_display_roundtrip(display) < 0 || !compositor || !shm || !wm)
        return 1;
    surface = wl_compositor_create_surface(compositor);
    xdg = xdg_wm_base_get_xdg_surface(wm, surface);
    xdg_surface_add_listener(xdg, &surface_listener, NULL);
    struct xdg_toplevel *top = xdg_surface_get_toplevel(xdg);
    xdg_toplevel_add_listener(top, &top_listener, NULL);
    xdg_toplevel_set_title(top, "TAP ZAP COSMIC rendered validation");
    xdg_toplevel_set_app_id(top, "app.tapzap.cosmic.swatches");
    xdg_toplevel_set_fullscreen(top, NULL);
    wl_surface_commit(surface);
    while (wl_display_dispatch(display) >= 0) {
    }
    return 0;
}
