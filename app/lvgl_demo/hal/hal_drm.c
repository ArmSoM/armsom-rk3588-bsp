#if USE_DRM
#include "lvgl/lvgl.h"
#include "lvgl/lv_conf.h"
#include "lv_drivers/display/drm.h"

void hal_drm_init(lv_coord_t hor_res, lv_coord_t ver_res, int rotated)
{

    /*Create a display*/
    drm_disp_drv_init(rotated * 90 % 360);

}
#endif

