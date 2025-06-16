#ifndef __RESOURCE_POOL
#define __RESOURCE_POOL

#include "../libs/lvgl/lvgl.h"

namespace ResourcePool
{

void Init();
lv_font_t* GetFont(const char* name);
const void* GetImage(const char* name);

}

#endif
