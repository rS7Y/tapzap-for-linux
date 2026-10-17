// SPDX-License-Identifier: GPL-3.0-or-later
// VM test client for COSMIC's accessibility protocol, bound at version 2.
#include <wayland-client.h>
#include "cosmic-a11y-client-protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct cosmic_a11y_manager_v1 *manager;
static uint32_t inverted, filter;
static void magnifier(void *data, struct cosmic_a11y_manager_v1 *object, uint32_t active) {
    (void)data;
    (void)object;
    (void)active;
}
static void screen_filter(void *data, struct cosmic_a11y_manager_v1 *object, uint32_t i,
                          uint32_t f) {
    (void)data;
    (void)object;
    inverted = i;
    filter = f;
}
static const struct cosmic_a11y_manager_v1_listener listener = {
    .magnifier = magnifier,
    .screen_filter = screen_filter,
};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                   uint32_t version) {
    (void)data;
    if (!strcmp(interface, "cosmic_a11y_manager_v1") && version >= 2) {
        manager = wl_registry_bind(registry, name, &cosmic_a11y_manager_v1_interface, 2);
        cosmic_a11y_manager_v1_add_listener(manager, &listener, NULL);
    }
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}
static const struct wl_registry_listener registry_listener = {.global = global,
                                                              .global_remove = removed};
int main(int argc, char **argv) {
    if (argc != 3 || (strcmp(argv[1], "0") && strcmp(argv[1], "1")) || strcmp(argv[2], "0"))
        return 2;
    alarm(5);
    struct wl_display *display = wl_display_connect(NULL);
    if (!display)
        return 1;
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    if (wl_display_roundtrip(display) < 0 || !manager || wl_display_roundtrip(display) < 0)
        return 1;
    uint32_t requested = (uint32_t)atoi(argv[1]);
    cosmic_a11y_manager_v1_set_screen_filter(manager, requested, 0);
    if (wl_display_roundtrip(display) < 0)
        return 1;
    printf("inverted=%u filter=%u\n", inverted, filter);
    wl_display_disconnect(display);
    return inverted == requested && filter == 0 ? 0 : 1;
}
