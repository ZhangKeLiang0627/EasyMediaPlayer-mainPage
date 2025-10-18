#pragma once

#include "common_inc.h"

namespace HAL
{
    void Init(void);
    void onUDiskEvent(const std::string &eventType, const std::string &devName, const std::string &mountPoint);
}
