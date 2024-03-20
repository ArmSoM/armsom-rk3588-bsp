/**
 * @file evdev.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "evdev.h"
#if USE_EVDEV != 0 || USE_BSD_EVDEV

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#if USE_BSD_EVDEV
#include <dev/evdev/input.h>
#else
#include <linux/input.h>
#endif
#include <dirent.h>

#include "main.h"
/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
int map(int x, int in_min, int in_max, int out_min, int out_max);

/**********************
 *  STATIC VARIABLES
 **********************/
int evdev_fd;
int evdev_root_x;
int evdev_root_y;
int evdev_button;

int evdev_key_val;

int evdev_rot;
/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

#define TP_NAME_LEN (32)
static char tp_event[TP_NAME_LEN] = EVDEV_NAME;
/**
 * Get touchscreen device event no
 */
void evdev_get_tp_event(void)
{
    int fd = 0, len = 0;
    int i = 0, input_dev_num = 0;
    DIR *pDir;
    struct dirent *ent = NULL;
    char file_name[TP_NAME_LEN];
    char tp_name[TP_NAME_LEN];
    char *path = "/sys/class/input";

    if ((pDir = opendir(path)) == NULL)
    {
        printf("%s: open %s filed\n", __func__, path);
        return;
    }

    while ((ent = readdir(pDir)) != NULL)
    {
        if (strstr(ent->d_name, "input"))
            input_dev_num++;
//            printf("%s: %s input deveices %d\n",
//                   __func__,
//                   ent->d_name,
//                   input_dev_num);
    }
    closedir(pDir);

    for (i = 0; i < input_dev_num; i++)
    {
        sprintf(file_name, "/sys/class/input/input%d/name", i);
        fd = open(file_name, O_RDONLY);
        if (fd == -1)
        {
            printf("%s: open %s failed\n", __func__, file_name);
            return;
        }

        len = read(fd, tp_name, TP_NAME_LEN);
        if (len <= 0)
        {
            printf("%s: read %s failed\n", __func__, file_name);
            close(fd);
            return;
        }

        if (len >= TP_NAME_LEN)
            len = TP_NAME_LEN - 1;

        tp_name[len] = '\0';

        if (strstr(tp_name, "ts"))
        {
            sprintf(tp_event, "/dev/input/event%d", i);
            printf("%s: %s = %s%s\n", __func__, file_name, tp_name, tp_event);
        }

        close(fd);
    }
}

/**
 * Initialize the evdev interface
 */
void evdev_init(int rot)
{
    evdev_rot = rot;
    evdev_get_tp_event();
#if USE_BSD_EVDEV
    evdev_fd = open(tp_event, O_RDWR | O_NOCTTY);
#else
    evdev_fd = open(tp_event, O_RDWR | O_NOCTTY | O_NDELAY);
#endif
    if (evdev_fd == -1)
    {
        perror("unable open evdev interface:");
        return;
    }

#if USE_BSD_EVDEV
    fcntl(evdev_fd, F_SETFL, O_NONBLOCK);
#else
    fcntl(evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
#endif

    evdev_root_x = 0;
    evdev_root_y = 0;
    evdev_key_val = 0;
    evdev_button = LV_INDEV_STATE_REL;
}
/**
 * reconfigure the device file for evdev
 * @param dev_name set the evdev device filename
 * @return true: the device file set complete
 *         false: the device file doesn't exist current system
 */
bool evdev_set_file(char *dev_name)
{
    if (evdev_fd != -1)
    {
        close(evdev_fd);
    }
#if USE_BSD_EVDEV
    evdev_fd = open(dev_name, O_RDWR | O_NOCTTY);
#else
    evdev_fd = open(dev_name, O_RDWR | O_NOCTTY | O_NDELAY);
#endif

    if (evdev_fd == -1)
    {
        perror("unable open evdev interface:");
        return false;
    }

#if USE_BSD_EVDEV
    fcntl(evdev_fd, F_SETFL, O_NONBLOCK);
#else
    fcntl(evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
#endif

    evdev_root_x = 0;
    evdev_root_y = 0;
    evdev_key_val = 0;
    evdev_button = LV_INDEV_STATE_REL;

    return true;
}
/**
 * Get the current position and state of the evdev
 * @param data store the evdev data here
 * @return false: because the points are not buffered, so no more data to be read
 */
void evdev_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    struct input_event in;
    int tmp;

    while (read(evdev_fd, &in, sizeof(struct input_event)) > 0)
    {
        if (in.type == EV_REL)
        {
            if (in.code == REL_X)
#if EVDEV_SWAP_AXES
                evdev_root_y += in.value;
#else
                evdev_root_x += in.value;
#endif
            else if (in.code == REL_Y)
#if EVDEV_SWAP_AXES
                evdev_root_x += in.value;
#else
                evdev_root_y += in.value;
#endif
        }
        else if (in.type == EV_ABS)
        {
            if (in.code == ABS_X)
#if EVDEV_SWAP_AXES
                evdev_root_y = in.value;
#else
                evdev_root_x = in.value;
#endif
            else if (in.code == ABS_Y)
#if EVDEV_SWAP_AXES
                evdev_root_x = in.value;
#else
                evdev_root_y = in.value;
#endif
            else if (in.code == ABS_MT_POSITION_X)
#if EVDEV_SWAP_AXES
                evdev_root_y = in.value;
#else
                evdev_root_x = in.value;
#endif
            else if (in.code == ABS_MT_POSITION_Y)
#if EVDEV_SWAP_AXES
                evdev_root_x = in.value;
#else
                evdev_root_y = in.value;
#endif
            else if (in.code == ABS_MT_TRACKING_ID)
                if (in.value == -1)
                    evdev_button = LV_INDEV_STATE_REL;
                else if (in.value == 0)
                    evdev_button = LV_INDEV_STATE_PR;
        }
        else if (in.type == EV_KEY)
        {
            if (in.code == BTN_MOUSE || in.code == BTN_TOUCH)
            {
                if (in.value == 0)
                    evdev_button = LV_INDEV_STATE_REL;
                else if (in.value == 1)
                    evdev_button = LV_INDEV_STATE_PR;
            }
            else if (drv->type == LV_INDEV_TYPE_KEYPAD)
            {
                data->state = (in.value) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
                switch (in.code)
                {
                case KEY_BACKSPACE:
                    data->key = LV_KEY_BACKSPACE;
                    break;
                case KEY_ENTER:
                    data->key = LV_KEY_ENTER;
                    break;
                case KEY_UP:
                    data->key = LV_KEY_UP;
                    break;
                case KEY_LEFT:
                    data->key = LV_KEY_PREV;
                    break;
                case KEY_RIGHT:
                    data->key = LV_KEY_NEXT;
                    break;
                case KEY_DOWN:
                    data->key = LV_KEY_DOWN;
                    break;
                default:
                    data->key = 0;
                    break;
                }
                evdev_key_val = data->key;
                evdev_button = data->state;
                return ;
            }
        }
    }

    if (drv->type == LV_INDEV_TYPE_KEYPAD)
    {
        /* No data retrieved */
        data->key = evdev_key_val;
        data->state = evdev_button;
        return ;
    }
    if (drv->type != LV_INDEV_TYPE_POINTER)
        return ;
    /*Store the collected data*/

#if EVDEV_CALIBRATE
    if (app_disp_rotation() == LV_DISP_ROT_90 ||
            app_disp_rotation() == LV_DISP_ROT_270)
    {
        data->point.x = map(evdev_root_x, EVDEV_HOR_MIN, EVDEV_HOR_MAX,
                            0, drv->disp->driver->ver_res);
        data->point.y = map(evdev_root_y, EVDEV_VER_MIN, EVDEV_VER_MAX,
                            0, drv->disp->driver->hor_res);
    }
    else
    {
        data->point.x = map(evdev_root_x, EVDEV_HOR_MIN, EVDEV_HOR_MAX,
                            0, drv->disp->driver->hor_res);
        data->point.y = map(evdev_root_y, EVDEV_VER_MIN, EVDEV_VER_MAX,
                            0, drv->disp->driver->ver_res);
    }
#else
    data->point.x = evdev_root_x;
    data->point.y = evdev_root_y;
#endif

    data->state = evdev_button;

    switch (evdev_rot)
    {
    case 0:
    default:
        break;
    case 90:
        tmp = data->point.x;
        data->point.x = data->point.y;
        data->point.y = drv->disp->driver->ver_res - tmp;
        break;
    case 180:
        tmp = data->point.x;
        data->point.x = drv->disp->driver->hor_res - data->point.y;
        data->point.y = drv->disp->driver->ver_res - tmp;
        break;
    case 270:
        tmp = data->point.x;
        data->point.x = drv->disp->driver->hor_res - data->point.y;
        data->point.y = tmp;
        break;
    }

    if (data->point.x < 0)
        data->point.x = 0;
    if (data->point.y < 0)
        data->point.y = 0;
    if (data->point.x >= drv->disp->driver->hor_res)
        data->point.x = drv->disp->driver->hor_res - 1;
    if (data->point.y >= drv->disp->driver->ver_res)
        data->point.y = drv->disp->driver->ver_res - 1;

    return ;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
int map(int x, int in_min, int in_max, int out_min, int out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#endif
