#include "View.h"
#include "Model.h"
#include "../utils/log/log.h"
#include "../../utils/FileOperations/FileOperations.h"
#include "../../utils/Animations/Animations.h"

extern "C"
{
#include "../../libs/lvgl/src/extra/libs/png/lodepng.h"
}

using namespace Page;

// LV_IMG_DECLARE(img_src_bootlogo);

void View::create(void)
{
    // 画布的创建
    lv_obj_t *cont = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xa18cd1), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(cont, lv_color_hex(0xfbc2eb), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(cont, LV_GRAD_DIR_HOR, LV_PART_MAIN); // 水平渐变
    lv_obj_set_style_bg_main_stop(cont, 0, LV_PART_MAIN);              // 渐变起点
    lv_obj_set_style_bg_grad_stop(cont, 192, LV_PART_MAIN);            // 渐变终点
    // lv_obj_set_style_bg_img_src(cont, "S:./picture/cover/main1.bin", 0);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    ui.cont = cont;

    lv_obj_t *img = lv_obj_create(cont);
    lv_obj_remove_style_all(img);
    // lv_obj_set_style_bg_img_src(img, &img_src_bootlogo, 0);
    lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_img_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_img_src(img, ResourcePool::GetImage("lawyer_close"), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(img, ResourcePool::GetImage("lawyer_open"), LV_STATE_PRESSED);
    lv_obj_align_to(img, cont, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    ui.image = img;

    // 按钮画布的创建
    lv_obj_t *btnCont = lv_obj_create(cont);
    lv_obj_remove_style_all(btnCont);
    lv_obj_set_size(btnCont, 400, LV_VER_RES / 2);
    // lv_obj_clear_flag(btnCont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(btnCont, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btnCont, lv_color_hex(0xa1c4fd), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(btnCont, lv_color_hex(0x84fab0), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(btnCont, LV_GRAD_DIR_HOR, LV_PART_MAIN); // 水平渐变
    lv_obj_set_style_bg_main_stop(btnCont, 0, LV_PART_MAIN);              // 渐变起点
    lv_obj_set_style_bg_grad_stop(btnCont, 192, LV_PART_MAIN);            // 渐变终点
    lv_obj_align(btnCont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(btnCont, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_column(btnCont, 30, LV_PART_MAIN);
    lv_obj_set_flex_flow(btnCont, LV_FLEX_FLOW_ROW); // 设置弹性布局，item横着排，自动换行
    lv_obj_set_flex_align(btnCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(btnCont, LV_DIR_HOR);               // 设置画布滚动方向：横向滚动
    lv_obj_set_scroll_snap_x(btnCont, LV_SCROLL_SNAP_CENTER); // 设置在垂直滚动结束时捕捉子元素的位置：人话：打开菜单第一个item的位置，现在是居中
    ui.btnCont.cont = btnCont;

    // topContCreate
    topContCreate(ui.cont);

    // 动画的创建
    ui.anim_timeline = lv_anim_timeline_create();

#define ANIM_DEF(start_time, obj, attr, start, end) \
    {start_time, obj, LV_ANIM_EXEC(attr), start, end, 500, lv_anim_path_ease_out, true}

#define ANIM_OPA_DEF(start_time, obj) \
    ANIM_DEF(start_time, obj, opa_scale, LV_OPA_COVER, LV_OPA_TRANSP)

    lv_anim_timeline_wrapper_t wrapper[] =
        {
            ANIM_DEF(0, ui.btnCont.cont, height, 20, 240),
            ANIM_DEF(0, ui.btnCont.cont, width, 20, 384),

            LV_ANIM_TIMELINE_WRAPPER_END // 这个标志着结构体成员结束，不能省略，在下面函数lv_anim_timeline_add_wrapper的轮询中做判断条件
        };
    lv_anim_timeline_add_wrapper(ui.anim_timeline, wrapper);

    appearAnimStart();
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

void View::topContCreate(lv_obj_t *obj)
{
    lv_obj_t *cont = lv_obj_create(obj);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(90), lv_pct(8));
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, LV_OPA_90, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xeeeeee), 0);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(cont, 5, LV_PART_MAIN);
    ui.topCont.cont = cont;

    lv_obj_t *screenshotBtn = btnCreate(cont, nullptr, 0, 0, 50, 30);
    lv_obj_align(screenshotBtn, LV_ALIGN_TOP_LEFT, 5, 4);
    lv_obj_set_style_bg_color(screenshotBtn, lv_color_hex(0xffd76d), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(screenshotBtn, lv_color_hex(0xdc9c00), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(screenshotBtn, lv_color_hex(0xffd76d), LV_STATE_FOCUSED);
    lv_obj_set_ext_click_area(screenshotBtn, 10);
    ui.topCont.screenshotBtn = screenshotBtn;
    lv_obj_add_event_cb(screenshotBtn, topContEventHandler, LV_EVENT_SHORT_CLICKED, this);

    lv_obj_t *screenshotBtnLabel = lv_label_create(ui.topCont.screenshotBtn);
    lv_obj_remove_style_all(screenshotBtnLabel);
    lv_obj_set_style_text_font(screenshotBtnLabel, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(screenshotBtnLabel, lv_color_hex(0x222222), LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(screenshotBtnLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(screenshotBtnLabel, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(screenshotBtnLabel, "Shot");

    lv_obj_t *titleLabel = lv_label_create(cont);
    lv_obj_remove_style_all(titleLabel);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_22, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(titleLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(titleLabel, lv_color_black(), 0);
    lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(titleLabel, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(titleLabel, "Main");
    // lv_obj_set_size(titleLabel, lv_pct(60), LV_SIZE_CONTENT);
    ui.topCont.titleLabel = titleLabel;

    lv_obj_t *udiskBtn = btnCreate(cont, ResourcePool::GetImage("udisk_on"), 0, 0, 40, 30);
    lv_obj_align(udiskBtn, LV_ALIGN_TOP_RIGHT, -5, 4);
    lv_obj_clear_flag(udiskBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(udiskBtn, lv_color_hex(0xd6def2), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(udiskBtn, lv_color_hex(0xd6def2), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(udiskBtn, lv_color_hex(0xd6def2), LV_STATE_FOCUSED);
    ui.topCont.udiskBtn = udiskBtn;
}

lv_obj_t *View::btnCreate(lv_obj_t *par, const void *img_src, lv_coord_t x_ofs, lv_coord_t y_ofs, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *obj = lv_obj_create(par);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_align(obj, LV_ALIGN_LEFT_MID, x_ofs, y_ofs);
    lv_obj_set_style_bg_img_src(obj, img_src, 0);

    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_width(obj, w / 1.1f, LV_STATE_PRESSED);                   // 设置button按下时的宽
    lv_obj_set_style_height(obj, h / 1.1f, LV_STATE_PRESSED);                  // 设置button按下时的长
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x356b8c), 0);                 // 设置按钮默认的颜色
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x242947), LV_STATE_PRESSED);  // 设置按钮在被按下时的颜色
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xf2daaa), LV_STATE_FOCUSED);  // 设置按钮在被聚焦时的颜色
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xa99991), LV_STATE_DISABLED); // 设置按钮失能时的颜色
    lv_obj_set_style_radius(obj, 9, 0);                                        // 按钮画圆角

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

lv_obj_t *View::btnCreate(lv_obj_t *par, void *img_src, const char *name)
{
    lv_obj_t *obj = lv_obj_create(par);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, LV_HOR_RES / 4, LV_VER_RES / 4);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);
    // lv_obj_set_style_clip_corner(obj, true, LV_PART_MAIN);
    // lv_obj_set_style_bg_img_src(obj, img_src, 0);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // 设置图片
    lv_obj_t *img = lv_img_create(obj);
    lv_obj_remove_style_all(img);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, 0);
    lv_img_set_src(img, img_src);
    lv_obj_center(img);

    // 设置名称
    lv_obj_t *label = lv_label_create(obj);
    lv_obj_remove_style_all(label);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text_fmt(label, "%s", name);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);

    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_width(obj, LV_HOR_RES / 5, LV_STATE_PRESSED); // 设置button按下时的长宽
    lv_obj_set_style_height(obj, LV_VER_RES / 5, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xfccb90), LV_STATE_DEFAULT);  // 设置按钮默认的颜色
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

void View::sideTipsPopupCreate(lv_obj_t *obj, const char *tips)
{
    lv_obj_t *sidePop = lv_obj_create(obj);
    lv_obj_remove_style_all(sidePop);
    lv_obj_set_size(sidePop, 90 + 90, 40);
    lv_obj_set_style_bg_opa(sidePop, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(sidePop, lv_color_hex(0xaaaaaa), LV_PART_MAIN);
    lv_obj_align(sidePop, LV_ALIGN_BOTTOM_RIGHT, 60, -10);
    lv_obj_set_style_radius(sidePop, 10, LV_PART_MAIN);
    lv_obj_set_user_data(sidePop, (void *)"sidePop");
    lv_obj_clear_state(sidePop, LV_STATE_CHECKED);

    lv_obj_t *label = lv_label_create(sidePop);
    lv_obj_remove_style_all(label);
    lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_22, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_label_set_text(label, tips);
    lv_anim_move(sidePop, 10, -90, 700, 0);

    // 创建一次性定时器，2秒后执行淡出动画
    lv_timer_t *timer = lv_timer_create([](lv_timer_t *timer)
                                        {
            lv_obj_t *sidePop = (lv_obj_t *)timer->user_data;
            lv_anim_drop_out(sidePop); }, 1500, sidePop);
    lv_timer_set_repeat_count(timer, 1);
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

    lv_obj_t *btn = btnCreate(ui.btnCont.cont, icon, name);
    lv_obj_set_user_data(btn, execFile);
    lv_obj_add_event_cb(btn, applicationEventHandler, LV_EVENT_ALL, this);
    // ui.btnCont.btn = btn;
}

/**
 * @brief 设置U盘图标是否出现
 * @param isAppear 是否出现
 */
void View::setUdisk(bool isAppear)
{
    if(isAppear)
        lv_obj_clear_flag(ui.topCont.udiskBtn, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(ui.topCont.udiskBtn, LV_OBJ_FLAG_HIDDEN);
}

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
        printf("[View] clickEventCb, exec: %s\n", exec);

        if (exec != nullptr)
        {
            if (instance->_opts.runAppCb != nullptr)
            {
                instance->_opts.runAppCb(exec, nullptr); // 运行应用程序，应用程序退出前阻塞在此
                lv_obj_invalidate(lv_scr_act());         // 重绘屏幕
            }
        }
    }
}

void View::convertRGB2BGR(lv_img_dsc_t *snapshot)
{
    uint8_t tmp_data = 0;
    uint32_t count = 0;
    for (int w = 0; w < snapshot->header.w; w++)
    {
        for (int h = 0; h < snapshot->header.h; h++)
        {
            tmp_data = *(snapshot->data + count);
            *(uint8_t *)(snapshot->data + count) = *(snapshot->data + count + 2);
            *(uint8_t *)(snapshot->data + count + 2) = tmp_data;
            count += 4;
        }
    }
}

void View::screenshot(lv_obj_t *obj)
{
    char fileNameBuffer[128];
    time_t timep;
    struct tm *p;
    char timeBuffer[64];

    time(&timep);
    p = gmtime(&timep);
    strftime(timeBuffer, sizeof(timeBuffer), "picture/screenshot/screenshot-%Y%m%d-%H%M%S", p);

    lv_snprintf(fileNameBuffer, sizeof(fileNameBuffer), "%s%s.%s", Model::getExeDirectory().c_str(), timeBuffer, "png");

    lv_img_dsc_t *snapshot = lv_snapshot_take(obj, LV_IMG_CF_TRUE_COLOR_ALPHA);

    unsigned int error = 0;
    std::error_code ec;

    log_debug("screenshot path: %s", fileNameBuffer);

    if (!FileOperations::exists(fileNameBuffer, ec))
    {
        // 文件不存在，则创建
        FileOperations::createAny(fileNameBuffer, false, true, ec);
    }

    // PNG的期望buffer为BGR，lv_snapshot得到的buffer是RGB，这里需要转换
    convertRGB2BGR(snapshot);

    error = lodepng_encode32_file(fileNameBuffer, snapshot->data, snapshot->header.w, snapshot->header.h);

    log_debug("lodepng_error_text: %s -> %d", lodepng_error_text(error), error);

    if (snapshot)
    {
        log_debug("snapshot has data, and kill it!");
        lv_snapshot_free(snapshot);
    }
}

void View::topContEventHandler(lv_event_t *event)
{
    View *instance = (View *)lv_event_get_user_data(event);
    LV_ASSERT_NULL(instance);

    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_SHORT_CLICKED)
    {
        if (obj == instance->ui.topCont.screenshotBtn)
        {
            log_debug("[View] screenshotBtn is short clicked");
            screenshot(lv_scr_act());

            instance->sideTipsPopupCreate(lv_layer_top(), "snapshot Get!");
        }
    }
}