#include "../include/common_inc.h"
#include "Model.h"

pthread_mutex_t lv_mutex;
static pthread_t threadLvgl;
static Page::Model *model;

static void exitCallback(void);
void *threadLvglHandler(void *);

int main(int argc, char *argv[])
{
    printf("[Sys] eMP_mainPage begin!\n");

    // Init HAL
    HAL::Init();

    // model初始化
    model = new Page::Model(exitCallback, lv_mutex);

    lv_obj_fade_in(lv_scr_act(), 350, 0);

    /* Handle LitlevGL tasks (tickless mode) */
    pthread_create(&threadLvgl, NULL, threadLvglHandler, NULL);

    while (1)
    {
        // ...
    }

    return 0;
}

/**
 * @brief LVGL处理线程
 *
 * @return void*
 */
void *threadLvglHandler(void *)
{
    HAL::LVGL_Proc();
}

/**
 * @brief 退出回调函数
 */
static void exitCallback(void)
{

    delete model;

    exit(0);
}