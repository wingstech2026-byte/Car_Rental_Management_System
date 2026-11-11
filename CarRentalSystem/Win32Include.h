// =============================================================================
// Win32Include.h
// -----------------------------------------------------------------------------
// Single place where <windows.h> is included, with the two macros that every
// modern Win32 C++ program should define first:
//   WIN32_LEAN_AND_MEAN - skips rarely used headers (faster builds).
//   NOMINMAX            - stops <windows.h> from defining min/max macros that
//                         would break std::min, std::max and numeric_limits.
// =============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
