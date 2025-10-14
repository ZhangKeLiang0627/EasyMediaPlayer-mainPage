/**
 * @file lv_100ask_screenshot.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_100ask_screenshot.h"

#include "../libs/lvgl/src/extra/libs/png/lodepng.h"

#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

#include "save_as_bmp.h"
#include "save_as_png.h"

/*********************
 *      DEFINES
 *********************/
#define UpAlign4(n) (((n) + 3) & ~3)
#define UpAlign8(n) (((n) + 7) & ~7)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void data_pre_processing(lv_img_dsc_t * snapshot, uint16_t bpp, lv_100ask_screenshot_sv_t screenshot_sv);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
bool lv_100ask_screenshot_create(lv_obj_t * obj, lv_img_cf_t cf, lv_100ask_screenshot_sv_t screenshot_sv, const char * filename)
{
    lv_img_dsc_t* snapshot = lv_snapshot_take(obj, LV_IMG_CF_TRUE_COLOR_ALPHA);

    if(snapshot)
    {
        data_pre_processing(snapshot, LV_COLOR_DEPTH, screenshot_sv);

        if(screenshot_sv == LV_100ASK_SCREENSHOT_SV_PNG)
        {
            if(LV_COLOR_DEPTH == 16)
            {
                save_as_png_file(snapshot->data, snapshot->header.w, snapshot->header.h, 24, filename);
            }
            else if(LV_COLOR_DEPTH == 32)
            {
                save_as_png_file(snapshot->data, snapshot->header.w, snapshot->header.h, 32, filename);
            }
        }
        else if(screenshot_sv == LV_100ASK_SCREENSHOT_SV_BMP)
        {
            if(LV_COLOR_DEPTH == 16)
            {
                save_as_bmp_file(snapshot->data, snapshot->header.w, snapshot->header.h, 24, filename);
            }
            else if(LV_COLOR_DEPTH == 32)
            {
                save_as_bmp_file(snapshot->data, snapshot->header.w, snapshot->header.h, 32, filename);
            }

        }
        else if(screenshot_sv == LV_100ASK_SCREENSHOT_SV_JPEG)
        {
            if(LV_COLOR_DEPTH == 16)
            {
                tje_encode_to_file(filename, snapshot->header.w, snapshot->header.h, 3, snapshot->data);
            }
            else if(LV_COLOR_DEPTH == 32)
            {
                tje_encode_to_file(filename, snapshot->header.w, snapshot->header.h, 4, snapshot->data);
            }

        }

        lv_snapshot_free(snapshot);
        return true;
    }

    return false;
}


/*=====================
 * Other functions
 *====================*/

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 截图数据预处理：将 LVGL 像素格式转换为目标格式（PNG/JPEG/BMP）
 * @param snapshot [in/out] LVGL 截图数据结构体（data 字段会被修改）
 * @param bpp [in] 原像素位深（16 = RGB565，32 = RGBA）
 * @param screenshot_sv [in] 目标存储格式（PNG/JPEG/BMP）
 */
static void data_pre_processing(lv_img_dsc_t *snapshot, uint16_t bpp, lv_100ask_screenshot_sv_t screenshot_sv)
{
    // 安全检查：避免空指针访问
    if (snapshot == NULL || snapshot->data == NULL) {
        LV_LOG_WARN("data_pre_processing: invalid snapshot (NULL pointer)");
        return;
    }

    // 1. 处理 16位 RGB565 格式（转 RGB888）
    if (bpp == 16) {
        uint32_t count = 0;
        const uint16_t img_width = snapshot->header.w;
        const uint16_t img_height = snapshot->header.h;
        uint8_t *data_ptr = (uint8_t *)snapshot->data;  // 简化指针操作

        // 遍历所有像素（宽 x 高）
        for (uint16_t w = 0; w < img_width; w++) {
            for (uint16_t h = 0; h < img_height; h++) {
                // 步骤1：读取 RGB565 数据（注意字节序：高位在 count+1，低位在 count）
                uint16_t rgb565_data = (uint16_t)((data_ptr[count + 1] << 8) | data_ptr[count]);

                // 步骤2：RGB565 转 RGB888（扩展位宽，确保颜色精度）
                uint8_t r8 = (uint8_t)((rgb565_data >> 11) << 3);  // R5 → R8（左移3位补零）
                uint8_t g8 = (uint8_t)((rgb565_data >> 5) << 2);   // G6 → G8（左移2位补零）
                uint8_t b8 = (uint8_t)((rgb565_data >> 0) << 3);   // B5 → B8（左移3位补零）

                // 步骤3：按目标格式写入 RGB888（不同格式通道顺序不同）
                if (screenshot_sv == LV_100ASK_SCREENSHOT_SV_PNG || 
                    screenshot_sv == LV_100ASK_SCREENSHOT_SV_JPEG) {
                    // PNG/JPEG：RGB 顺序（count=R, count+1=G, count+2=B）
                    data_ptr[count]     = r8;
                    data_ptr[count + 1] = g8;
                    data_ptr[count + 2] = b8;
                } else if (screenshot_sv == LV_100ASK_SCREENSHOT_SV_BMP) {
                    // BMP：BGR 顺序（count=B, count+1=G, count+2=R）
                    data_ptr[count]     = b8;
                    data_ptr[count + 1] = g8;
                    data_ptr[count + 2] = r8;
                }

                count += 3;  // 每次处理 3字节（RGB888）
            }
        }
    }
    // 2. 处理 32位 RGBA 格式（交换 R/B 通道，适配 PNG/JPEG）
    else if (bpp == 32 && (screenshot_sv == LV_100ASK_SCREENSHOT_SV_PNG || 
                          screenshot_sv == LV_100ASK_SCREENSHOT_SV_JPEG)) {
        uint32_t count = 0;
        const uint16_t img_width = snapshot->header.w;
        const uint16_t img_height = snapshot->header.h;
        uint8_t *data_ptr = (uint8_t *)snapshot->data;  // 简化指针操作

        // 遍历所有像素（宽 x 高）
        for (uint16_t w = 0; w < img_width; w++) {
            for (uint16_t h = 0; h < img_height; h++) {
                // LVGL 32位格式通常是 RGBA（count=R, count+1=G, count+2=B, count+3=A）
                // PNG/JPEG 需 BGR 或 RGB，此处交换 R/B 通道（保持 A 通道不变）
                uint8_t tmp_r = data_ptr[count];          // 暂存 R 通道
                data_ptr[count]     = data_ptr[count + 2];  // R → B
                data_ptr[count + 2] = tmp_r;                // B → R

                count += 4;  // 每次处理 4字节（RGBA）
            }
        }
    }
    // 3. 不支持的格式（警告提示）
    else {
        LV_LOG_WARN("data_pre_processing: unsupported format (bpp=%d, sv=%d)", bpp, screenshot_sv);
    }
}

/*LV_USE_100ASK_SCREENSHOT*/
