// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include "ctm-client.h"
#include "color.h"

struct output {
    struct output *next;
    struct wl_output *proxy;
    uint32_t id;
    char name[128];
    bool ready;
    int scale;
};
static struct wl_display *display;
static struct wl_registry *registry;
static struct hyprland_ctm_control_manager_v1 *manager;
static struct output *outputs;
static uint32_t manager_id, manager_version;
static bool enabled, blocked, dirty, probe, io_failed, allow_extreme_dim;
static double intensity = 1, brightness = 0.7;
static const char *target;
static volatile sig_atomic_t stopping;

static void stop_signal(int sig) { stopping = 1; }
static bool selected(struct output *o) {
    return o->ready && (!target || strcmp(target, o->name) == 0);
}
static int selected_count(void) {
    int count = 0;
    for (struct output *o = outputs; o; o = o->next) count += selected(o);
    return count;
}
static void output_geometry(void *d, struct wl_output *o, int32_t x, int32_t y,
    int32_t w, int32_t h, int32_t subpixel, const char *make, const char *model, int32_t transform) {}
static void output_mode(void *d, struct wl_output *o, uint32_t flags, int32_t w, int32_t h, int32_t refresh) {}
static void output_done(void *d, struct wl_output *o) { ((struct output *)d)->ready = true; dirty = true; }
static void output_scale(void *d, struct wl_output *o, int32_t factor) { ((struct output *)d)->scale = factor; }
static void output_name(void *d, struct wl_output *o, const char *name) {
    snprintf(((struct output *)d)->name, sizeof(((struct output *)d)->name), "%s", name);
}
static void output_description(void *d, struct wl_output *o, const char *description) {}
static const struct wl_output_listener output_listener = {
    output_geometry, output_mode, output_done, output_scale, output_name, output_description
};
static void manager_blocked(void *d, struct hyprland_ctm_control_manager_v1 *m) { blocked = true; }
static const struct hyprland_ctm_control_manager_v1_listener manager_listener = { manager_blocked };

static void global(void *d, struct wl_registry *r, uint32_t id, const char *iface, uint32_t version) {
    if (probe) printf("GLOBAL %s %u\n", iface, version);
    if (!strcmp(iface, hyprland_ctm_control_manager_v1_interface.name)) {
        manager_id = id;
        manager_version = version;
    } else if (!strcmp(iface, wl_output_interface.name) && version >= 4) {
        struct output *o = calloc(1, sizeof(*o));
        if (!o) { stopping = 1; return; }
        o->id = id;
        o->proxy = wl_registry_bind(r, id, &wl_output_interface, 4);
        o->next = outputs;
        outputs = o;
        wl_output_add_listener(o->proxy, &output_listener, o);
    }
}
static void global_remove(void *d, struct wl_registry *r, uint32_t id) {
    if (id == manager_id) { manager_id = 0; stopping = 1; }
    for (struct output **p = &outputs; *p; p = &(*p)->next) {
        if ((*p)->id == id) {
            struct output *o = *p;
            *p = o->next;
            wl_output_release(o->proxy);
            free(o);
            dirty = true;
            break;
        }
    }
}
static const struct wl_registry_listener registry_listener = {global, global_remove};

/* Destroying the owning manager restores identity in the compositor, including
 * on SIGKILL/socket loss. It does not restore another app's private night state. */
static void release_control(void) {
    enabled = false;
    if (manager) {
        hyprland_ctm_control_manager_v1_destroy(manager);
        manager = NULL;
        if (wl_display_roundtrip(display) < 0) { stopping = 1; io_failed = true; }
    }
}
static bool acquire_control(void) {
    if (manager) return true;
    if (!manager_id || manager_version < 2) {
        puts("ERR requires hyprland-ctm-control-v1 version 2 for conflict detection");
        return false;
    }
    blocked = false;
    manager = wl_registry_bind(registry, manager_id, &hyprland_ctm_control_manager_v1_interface, 2);
    hyprland_ctm_control_manager_v1_add_listener(manager, &manager_listener, NULL);
    if (wl_display_roundtrip(display) < 0) { stopping = 1; io_failed = true; return false; }
    if (blocked) {
        puts("ERR CTM already owned; turn off the other night-light app before enabling TAP ZAP");
        release_control();
        return false;
    }
    return true;
}
static bool apply(void) {
    dirty = false;
    if (!enabled || !manager) return true;
    double rgb[3];
    tapzap_rgb(intensity, brightness, rgb);
    for (struct output *o = outputs; o; o = o->next) {
        if (!o->ready) continue;
        bool use = selected(o);
        hyprland_ctm_control_manager_v1_set_ctm_for_output(manager, o->proxy,
            wl_fixed_from_double(use ? rgb[0] : 1), 0, 0,
            0, wl_fixed_from_double(use ? rgb[1] : 1), 0,
            0, 0, wl_fixed_from_double(use ? rgb[2] : 1));
    }
    hyprland_ctm_control_manager_v1_commit(manager);
    if (wl_display_roundtrip(display) < 0) { stopping = 1; io_failed = true; return false; }
    return true;
}
static void status(void) {
    int scale = 1;
    for (struct output *o = outputs; o; o = o->next) if (selected(o) && o->scale > scale) scale = o->scale;
    double rgb[3] = {1,1,1};
    if (enabled) tapzap_rgb(intensity, brightness, rgb);
    printf("OK enabled=%d intensity=%.6f brightness=%.6f outputs=%d scale=%d rgb=%.8f,%.8f,%.8f\n",
        enabled, intensity, brightness, selected_count(), scale,
        wl_fixed_to_double(wl_fixed_from_double(rgb[0])),
        wl_fixed_to_double(wl_fixed_from_double(rgb[1])),
        wl_fixed_to_double(wl_fixed_from_double(rgb[2])));
}
static void command(char *line) {
    double i, b;
    char extra;
    if (!strcmp(line, "quit")) { stopping = 1; return; }
    if (!strcmp(line, "status")) { status(); return; }
    if (!strcmp(line, "off") || !strcmp(line, "reset") || (!strcmp(line, "toggle") && enabled)) {
        release_control();
        if (!strcmp(line, "reset")) { intensity = 0; brightness = 1; }
        status();
        return;
    }
    if (sscanf(line, "set %lf %lf %c", &i, &b, &extra) == 2) {
        if (!isfinite(i) || !isfinite(b) || i < 0 || i > 1 || b < (allow_extreme_dim ? 0.0 : 0.1) || b > 1) {
            puts("ERR filter values outside allowed range");
            return;
        }
        intensity = i; brightness = b;
        if (apply()) status();
        return;
    }
    if (!strcmp(line, "on") || !strcmp(line, "toggle")) {
        if (!selected_count()) { puts("ERR requested output is not connected"); return; }
        if (!acquire_control()) return;
        enabled = true;
        if (apply()) status();
        return;
    }
    puts("ERR commands: set INTENSITY BRIGHTNESS | on | off | toggle | reset | status | quit");
}
int main(int argc, char **argv) {
    for (int a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "--probe")) probe = true;
        else if (!strcmp(argv[a], "--allow-extreme-dim")) allow_extreme_dim = true;
        else if (!strcmp(argv[a], "--output") && a+1 < argc) target = argv[++a];
        else { fprintf(stderr, "Usage: %s [--probe] [--output NAME]\n", argv[0]); return 2; }
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    struct sigaction sa = {.sa_handler = stop_signal};
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL); sigaction(SIGHUP, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    display = wl_display_connect(NULL);
    if (!display) { fputs("ERR cannot connect to Wayland session\n", stderr); return 1; }
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    if (wl_display_roundtrip(display) < 0 || wl_display_roundtrip(display) < 0) { stopping = 1; io_failed = true; }
    if (probe) {
        for (struct output *o = outputs; o; o = o->next) printf("OUTPUT %s\n", o->name);
        stopping = 1;
    } else if (!manager_id || manager_version < 2 || !selected_count()) {
        fputs("ERR needs CTM v2 and a matching wl_output v4 display\n", stderr);
        wl_display_disconnect(display);
        return 1;
    } else status();
    char line[256]; size_t used = 0; bool overflow = false;
    while (!stopping) {
        if (wl_display_dispatch_pending(display) < 0) { io_failed = true; break; }
        if (dirty && !apply()) break;
        if (wl_display_flush(display) < 0 && errno != EAGAIN) { io_failed = true; break; }
        struct pollfd fds[2] = {{wl_display_get_fd(display), POLLIN, 0}, {STDIN_FILENO, POLLIN, 0}};
        if (poll(fds, 2, -1) < 0) { if (errno == EINTR) continue; io_failed = true; break; }
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) { io_failed = true; break; }
        if (fds[1].revents & (POLLERR | POLLNVAL)) { io_failed = true; break; }
        if ((fds[0].revents & POLLIN) && wl_display_dispatch(display) < 0) { io_failed = true; break; }
        if (fds[1].revents & (POLLIN | POLLHUP)) {
            char buf[256]; ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) { if (n < 0 && errno == EINTR) continue; if (n < 0) io_failed = true; break; }
            for (ssize_t c = 0; c < n && !stopping; c++) {
                if (buf[c] == '\n') {
                    if (overflow) puts("ERR command too long");
                    else { line[used] = 0; command(line); }
                    used = 0; overflow = false;
                } else if (buf[c] != '\r') {
                    if (used+1 < sizeof(line)) line[used++] = buf[c];
                    else overflow = true;
                }
            }
        }
    }
    release_control();
    while (outputs) { struct output *o = outputs; outputs = o->next; wl_output_release(o->proxy); free(o); }
    wl_registry_destroy(registry);
    wl_display_flush(display);
    wl_display_disconnect(display);
    return io_failed ? 1 : 0;
}
