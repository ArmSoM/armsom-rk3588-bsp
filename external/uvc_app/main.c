#include <stdio.h>
#include <unistd.h>
#include "uvc_control.h"
#include "uvc_video.h"
#include "mpi_enc.h"
#include "drm.h"

int main(int argc, char* argv[])
{
    int fd;
    int ret;
    unsigned int handle;
    char *buffer;
    int handle_fd;
    size_t size;
    int i = 0;
    int width, height;
    int y, uv;
    int extra_cnt = 0;
    uint32_t flags = 0;

    if (argc != 3) {
        printf("Usage: uvc_app width height\n");
        printf("e.g. uvc_app 640 480\n");
        return -1;
    }
    width = atoi(argv[1]);
    height = atoi(argv[2]);
    if (width == 0 || height == 0) {
        printf("Usage: uvc_app width height\n");
        printf("e.g. uvc_app 640 480\n");
        return -1;
    }

    fd = drm_open();
    if (fd < 0)
        return -1;

    size = MPP_ALIGN(width, 16) * MPP_ALIGN(height, 16) * 3 / 2;
    ret = drm_alloc(fd, size, 16, &handle, 0);
    if (ret)
        return -1;

    ret = drm_handle_to_fd(fd, handle, &handle_fd, 0);
    if (ret)
        return -1;

    buffer = (char*)drm_map_buffer(fd, handle, size);
    if (!buffer) {
        printf("drm map buffer fail.\n");
        return -1;
    }
    y = width * height / 4;
    memset(buffer, 128, y);
    memset(buffer + y, 64, y);
    memset(buffer + y * 2, 128, y);
    memset(buffer + y * 3, 192, y);
    uv = width * height / 8;
    memset(buffer + y * 4, 0, uv);
    memset(buffer + y * 4 + uv, 64, uv);
    memset(buffer + y * 4 + uv * 2, 128, uv);
    memset(buffer + y * 4 + uv * 3, 192, uv);

    flags = UVC_CONTROL_LOOP_ONCE;
    uvc_control_run(flags);
    while(1) {
        extra_cnt++;
        uvc_read_camera_buffer(buffer, handle_fd, size, &extra_cnt, sizeof(extra_cnt));
        usleep(30000);
    }

    uvc_control_join(flags);

    drm_unmap_buffer(buffer, size);
    drm_free(fd, handle);
    drm_close(fd);
    return 0;
}
