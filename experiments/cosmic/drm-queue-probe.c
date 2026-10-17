// SPDX-License-Identifier: GPL-3.0-or-later
#include <drm.h>
#include <drm_mode.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
int main(int argc, char **argv) {
    int fd = open(argc == 2 ? argv[1] : "/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    for (int i = 0; i < 256; i++) {
        struct drm_mode_create_dumb c = {.width = 64, .height = 64, .bpp = 32};
        if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &c)) {
            perror("create");
            return 2;
        }
        struct drm_mode_destroy_dumb d = {.handle = c.handle};
        if (ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d)) {
            perror("destroy");
            return 3;
        }
        printf("buffer %d complete\n", i + 1);
        fflush(stdout);
    }
    close(fd);
    return 0;
}
