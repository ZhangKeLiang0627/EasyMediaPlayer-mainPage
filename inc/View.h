#pragma once

#include "../libs/lvgl/lvgl.h"
#include "../utils/lv_ext/lv_obj_ext_func.h"
#include "../utils/lv_ext/lv_anim_timeline_wrapper.h"
#include "ResourcePool.h"
#include <functional>

namespace Page
{
    using ExitCb = std::function<void(void)>;
    using RunApplicationCb = std::function<void(const char *, char *const *)>;

    struct Operations
    {
        ExitCb exitCb;
        RunApplicationCb runAppCb; // 运行应用程序回调函数，参数为执行文件名称
    };

    class View
    {
    private:
        Operations _opts; // View回调函数集

    public:
        struct
        {
            lv_obj_t *cont;
            lv_obj_t *image;

            struct
            {
                lv_obj_t *cont;
                lv_obj_t *btn;
            } btnCont;

            struct
            {
                lv_obj_t *cont;
                lv_obj_t *screenshotBtn;

                lv_obj_t *titleLabel;
            } topCont;

            lv_anim_timeline_t *anim_timeline;
            lv_anim_timeline_t *anim_timelineClick;
        } ui;

        void create(void);
        void release(void);
        void setOperations(Operations &opts);
        void appearAnimStart(bool reverse = false);
        void appearAnimClick(bool reverse = false);
        void addApplication(const char *name, const char *exec, char *const argv[], void *icon);

    private:
        void topContCreate(lv_obj_t *obj);

        static void onEvent(lv_event_t *event);

        static void applicationEventHandler(lv_event_t *event);
        static void topContEventHandler(lv_event_t *event);

        void sideTipsPopupCreate(lv_obj_t *obj, const char *tips);
        lv_obj_t *roundRectCreate(lv_obj_t *par, lv_coord_t x_ofs, lv_coord_t y_ofs);
        lv_obj_t *btnCreate(lv_obj_t *par, void *img_src, const char *name);
        lv_obj_t *btnCreate(lv_obj_t *par, const void *img_src, lv_coord_t x_ofs, lv_coord_t y_ofs, lv_coord_t w = 50, lv_coord_t h = 50);

        // lv_screenshot
        static void convertRGB2BGR(lv_img_dsc_t *snapshot);
        static void screenshot(lv_obj_t *obj);
    };

}