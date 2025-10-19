#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #if defined(WIQ_IOH_BUILD)
    #define WIQ_IOH_API __declspec(dllexport)
  #else
    #define WIQ_IOH_API __declspec(dllimport)
  #endif
#else
  #define WIQ_IOH_API __attribute__((visibility("default")))
#endif
