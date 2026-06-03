#pragma once

// LOG_DEBUG(fmt, ...) — prints via g_print in debug builds only.
#ifndef NDEBUG
#  include <glib.h>
#  define LOG_DEBUG(fmt, ...) g_print("[Debug] " fmt, ##__VA_ARGS__)
#else
#  define LOG_DEBUG(fmt, ...) ((void)0)
#endif

