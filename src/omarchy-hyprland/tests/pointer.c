// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Short-lived native Wayland input driver for the focused dismissal check.
#include <wayland-client.h>
#include "pointer-protocol.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static struct zwlr_virtual_pointer_manager_v1 *manager;
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    if (!strcmp(interface, "zwlr_virtual_pointer_manager_v1"))
        manager = wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) {}
static uint32_t now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec*1000+ts.tv_nsec/1000000; }
int main(void) {
    struct wl_display *display = wl_display_connect(NULL); if (!display) return 2;
    struct wl_registry *registry = wl_display_get_registry(display);
    const struct wl_registry_listener listener = {global, removed};
    wl_registry_add_listener(registry, &listener, NULL); wl_display_roundtrip(display);
    if (!manager) return 3;
    struct zwlr_virtual_pointer_v1 *pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, NULL);
    char line[128], command[20]; int x, y;
    while (fgets(line, sizeof line, stdin)) {
        x = y = 0; if (sscanf(line, "%19s %d %d", command, &x, &y) < 1) continue;
        if (!strcmp(command, "move") || !strcmp(command, "click")) {
            zwlr_virtual_pointer_v1_motion_absolute(pointer, now(), x, y, 1920, 1080);
            zwlr_virtual_pointer_v1_frame(pointer); wl_display_roundtrip(display); usleep(120000);
        }
        if (!strcmp(command, "press") || !strcmp(command, "click")) {
            zwlr_virtual_pointer_v1_button(pointer, now(), 272, WL_POINTER_BUTTON_STATE_PRESSED);
            zwlr_virtual_pointer_v1_frame(pointer); wl_display_roundtrip(display); usleep(40000);
        }
        if (!strcmp(command, "release") || !strcmp(command, "click")) {
            zwlr_virtual_pointer_v1_button(pointer, now(), 272, WL_POINTER_BUTTON_STATE_RELEASED);
            zwlr_virtual_pointer_v1_frame(pointer); wl_display_roundtrip(display);
        }
        puts("ok"); fflush(stdout);
    }
    zwlr_virtual_pointer_v1_destroy(pointer);
    zwlr_virtual_pointer_manager_v1_destroy(manager);
    wl_registry_destroy(registry); wl_display_roundtrip(display); wl_display_disconnect(display);
    return 0;
}
