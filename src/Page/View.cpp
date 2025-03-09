#include "View.h"

using namespace Page;

void View::create(void)
{
    // 画布的创建
    lv_obj_t *cont = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xcccccc), 0);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    ui.cont = cont;

    lv_obj_t *btnCont = lv_obj_create(cont);
    lv_obj_remove_style_all(btnCont);
    lv_obj_set_size(btnCont, LV_HOR_RES / 2, LV_VER_RES / 2);
    // lv_obj_clear_flag(btnCont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(btnCont, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btnCont, lv_color_hex(0x6a8d6d), 0);
    lv_obj_align(btnCont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(btnCont, 16, LV_PART_MAIN);

    lv_obj_set_style_pad_hor(btnCont, 30, LV_PART_MAIN); // 设置每一个item的宽度
    lv_obj_set_style_pad_row(btnCont, 30, LV_PART_MAIN); // 设置每一个item的间距
    lv_obj_set_flex_flow(btnCont, LV_FLEX_FLOW_ROW);     // 设置弹性布局，item横着排，自动换行
    lv_obj_set_flex_align(btnCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(btnCont, LV_DIR_HOR);               // 设置画布滚动方向：垂直滚动
    lv_obj_set_scroll_snap_y(btnCont, LV_SCROLL_SNAP_CENTER); // 设置在垂直滚动结束时捕捉子元素的位置：人话：打开菜单第一个item的位置，现在是居中
    ui.btnCont.cont = btnCont;

    // 动画的创建
    ui.anim_timeline = lv_anim_timeline_create();

#define ANIM_DEF(start_time, obj, attr, start, end) \
    {start_time, obj, LV_ANIM_EXEC(attr), start, end, 500, lv_anim_path_ease_out, true}

#define ANIM_OPA_DEF(start_time, obj) \
    ANIM_DEF(start_time, obj, opa_scale, LV_OPA_COVER, LV_OPA_TRANSP)

    lv_anim_timeline_wrapper_t wrapper[] =
        {
            ANIM_DEF(0, ui.btnCont.cont, height, 20, lv_obj_get_height(ui.btnCont.cont)),
            ANIM_DEF(0, ui.btnCont.cont, width, 20, lv_obj_get_width(ui.btnCont.cont)),

            LV_ANIM_TIMELINE_WRAPPER_END // 这个标志着结构体成员结束，不能省略，在下面函数lv_anim_timeline_add_wrapper的轮询中做判断条件
        };
    lv_anim_timeline_add_wrapper(ui.anim_timeline, wrapper);

    // appearAnimStart();
}

void View::release()
{
    if (ui.anim_timeline)
    {
        lv_anim_timeline_del(ui.anim_timeline);
        ui.anim_timeline = nullptr;
    }
    if (ui.anim_timelineClick)
    {
        lv_anim_timeline_del(ui.anim_timelineClick);
        ui.anim_timelineClick = nullptr;
    }

    // 释放用户数据
    lv_obj_t *btn;
    while ((btn = lv_obj_get_child(ui.btnCont.cont, -1)) != nullptr)
    {
        char *execFile = (char *)lv_obj_get_user_data(btn);

        delete[] execFile; // 释放保存的app数据
    }
}

void View::setOperations(Operations &opts)
{
    _opts = opts;
}

void View::appearAnimStart(bool reverse) // 开始开场动画
{
    lv_anim_timeline_set_reverse(ui.anim_timeline, reverse);
    lv_anim_timeline_start(ui.anim_timeline);
}

void View::appearAnimClick(bool reverse) // 按钮动画
{
    lv_anim_timeline_set_reverse(ui.anim_timelineClick, reverse);
    lv_anim_timeline_start(ui.anim_timelineClick);
}

lv_obj_t *View::btnCreate(lv_obj_t *par, const void *img_src, lv_coord_t y_ofs)
{
    lv_obj_t *obj = lv_obj_create(par);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, LV_HOR_RES / 4, LV_VER_RES / 4);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_align(obj, LV_ALIGN_CENTER, 0, y_ofs);
    lv_obj_set_style_bg_img_src(obj, img_src, 0);

    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_width(obj, LV_HOR_RES / 6, LV_STATE_PRESSED); // 设置button按下时的长宽
    lv_obj_set_style_height(obj, LV_VER_RES / 6, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x356b8c), 0);                 // 设置按钮默认的颜色
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x242947), LV_STATE_PRESSED);  // 设置按钮在被按下时的颜色
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xf2daaa), LV_STATE_FOCUSED);  // 设置按钮在被聚焦时的颜色
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xa99991), LV_STATE_DISABLED); // 设置按钮在被聚焦时的颜色
    lv_obj_set_style_radius(obj, 10, 0);                                       // 按钮画圆角

    static lv_style_transition_dsc_t tran;
    static const lv_style_prop_t prop[] = {LV_STYLE_WIDTH, LV_STYLE_HEIGHT, LV_STYLE_PROP_INV};
    lv_style_transition_dsc_init(
        &tran,
        prop,
        lv_anim_path_ease_out,
        150,
        0,
        NULL);
    lv_obj_set_style_transition(obj, &tran, LV_STATE_PRESSED);
    lv_obj_set_style_transition(obj, &tran, LV_STATE_FOCUSED);

    lv_obj_update_layout(obj);

    return obj;
}

lv_obj_t *View::roundRectCreate(lv_obj_t *par, lv_coord_t x_ofs, lv_coord_t y_ofs)
{
    /* Render octagon explode */
    lv_obj_t *roundRect = lv_obj_create(par);
    lv_obj_remove_style_all(roundRect);
    lv_obj_set_size(roundRect, 10, 10);
    lv_obj_set_style_radius(roundRect, 2, 0);

    lv_obj_set_style_shadow_width(roundRect, 10, 0);
    lv_obj_set_style_shadow_ofs_x(roundRect, 1, 0);
    lv_obj_set_style_shadow_ofs_y(roundRect, 1, 0);
    lv_obj_set_style_shadow_color(roundRect, lv_color_hex(0x5d8c3d), 0);
    lv_obj_set_style_shadow_spread(roundRect, 1, 0);
    lv_obj_set_style_shadow_opa(roundRect, LV_OPA_TRANSP, 0);

    lv_obj_set_style_bg_color(roundRect, lv_color_hex(0x88d35e), 0);
    lv_obj_set_style_bg_opa(roundRect, LV_OPA_TRANSP, 0);
    lv_obj_align(roundRect, LV_ALIGN_CENTER, x_ofs, y_ofs);

    return roundRect;
}

/**
 * @brief 在主界面添加一个app icon
 * @param name 应用程序名称
 * @param exec 应用程序文件路径
 * @param argv 应用程序参数
 * @param icon 应用程序图标(lv_img)
 */
void View::addApplication(const char *name, const char *exec, char *const argv[], void *icon)
{
    // 保存app执行文件名称
    int len = strlen(exec) + 1;
    char *execFile = new char[len];
    strcpy(execFile, exec);

    lv_obj_t *btn = btnCreate(ui.btnCont.cont, LV_SYMBOL_PLAY, 0);
    lv_obj_set_user_data(btn, execFile);
    lv_obj_add_event_cb(btn, applicationEventHandler, LV_EVENT_ALL, this);
    // ui.btnCont.btn = btn;
}

// /**
//  * @brief 应用程序 icon 点击事件回调函数
//  */
// void applicationEventHandler(lv_event_t *event)
// {
//     lv_event_code_t code = lv_event_get_code(event);
//     lv_obj_t *obj = lv_event_get_current_target(event);

//     const char *exec = (const char *)lv_obj_get_user_data(obj);
//     char *const *argv = (char *const *)lv_event_get_user_data(event);

//     // 打印跳转下一个程序的信息
//     printf("[Model] clickEventCb, exec:%s, argv:", exec);
//     for (int i = 0; argv[i] != nullptr; i++)
//     {
//         printf("%s ", argv[i]);
//     }
//     printf("\n");

//     if (exec != nullptr && argv != nullptr)
//     {
//         if (runApp != nullptr)
//         {
//             uiOpts.runApp(exec, argv);       // 运行应用程序,应用程序退出前阻塞在此
//             lv_obj_invalidate(lv_scr_act()); // 重绘屏幕
//         }
//     }

//     if (code == LV_EVENT_SHORT_CLICKED)
//     {
//         // instance->appearAnimClick();
//     }
// }

/**
 * @brief 应用程序 icon 点击事件回调函数
 */
void View::applicationEventHandler(lv_event_t *event)
{
    View *instance = (View *)lv_event_get_user_data(event);
    LV_ASSERT_NULL(instance);

    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_current_target(event);

    const char *exec = (const char *)lv_obj_get_user_data(obj);

    if (code == LV_EVENT_SHORT_CLICKED)
    {
        printf("clickEventCb, exec: %s\n", exec);

        if (exec != nullptr)
        {
            if (instance->_opts.runAppCb != nullptr)
            {
                instance->_opts.runAppCb(exec, nullptr); // 运行应用程序，应用程序退出前阻塞在此
                lv_obj_invalidate(lv_scr_act());         // 重绘屏幕
            }
        }
        // instance->appearAnimClick();
    }
}