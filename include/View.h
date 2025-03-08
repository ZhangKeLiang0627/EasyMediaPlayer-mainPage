#pragma once

#include "../libs/lvgl/lvgl.h"
#include "lv_obj_ext_func.h"
#include "lv_anim_timeline_wrapper.h"
#include <functional>

namespace Page
{
    using ExitCb = std::function<void(void)>;
    using RunApplicationCb = std::function<void(const char *, char *const *)>;

    struct Operations
    {
        ExitCb exitCb;
        RunApplicationCb runApp; // 运行应用程序回调函数，参数为执行文件名称
    };

    class View
    {
    private:
        Operations _opts; // View回调函数集

    public:
        struct
        {
            lv_obj_t *cont;

            struct
            {
                lv_obj_t *cont;
                lv_obj_t *btn;
            } btnCont;

            lv_anim_timeline_t *anim_timeline;
            lv_anim_timeline_t *anim_timelineClick;
        } ui;

        void create(Operations &opts);
        void release(void);
        void appearAnimStart(bool reverse = false);
        void appearAnimClick(bool reverse = false);

        void addApplication(const char *name, const char *exec, char *const argv[], void *icon);

    private:
        static void onEvent(lv_event_t *event);
        static void applicationEventHandler(lv_event_t *event);

        lv_obj_t *roundRectCreate(lv_obj_t *par, lv_coord_t x_ofs, lv_coord_t y_ofs);
        lv_obj_t *btnCreate(lv_obj_t *par, const void *img_src, lv_coord_t y_ofs);
    };

}