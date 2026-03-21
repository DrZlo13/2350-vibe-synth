#pragma once
#include "SEGGER_RTT.h"

struct Log {
    template<typename... Args>
    static void info(const char* fmt, Args... args) {
        SEGGER_RTT_printf(0, fmt, args...);
    }
};
