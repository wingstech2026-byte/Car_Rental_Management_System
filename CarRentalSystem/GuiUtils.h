// =============================================================================
// GuiUtils.h
// -----------------------------------------------------------------------------
// Small Win32 helpers shared by all GUI classes, including an RAII wrapper for
// GDI fonts: FontHandle is a std::unique_ptr with a custom deleter, so the
// font is released with DeleteObject automatically - no manual cleanup, no leaks.
// =============================================================================
#pragma once

#include "Win32Include.h"

#include <memory>
#include <string>
#include <type_traits>

namespace Gui
{
    /// Deleter used by FontHandle: calls DeleteObject on the GDI object.
    struct GdiObjectDeleter
    {
        void operator()(HGDIOBJ object) const noexcept;
    };

    /// Owning RAII handle for an HFONT.
    using FontHandle = std::unique_ptr<std::remove_pointer_t<HFONT>, GdiObjectDeleter>;

    /// Creates a "Segoe UI" font of the given point size, scaled for the DPI.
    FontHandle createUiFont(int pointSize, bool bold, UINT dpi);

    /// Scales a size given at 96 DPI (100%) to the given DPI.
    int scaleForDpi(int value, UINT dpi) noexcept;

    /// Returns the text of a window/control.
    std::wstring getWindowText(HWND window);

    /// Converts a control ID to the HMENU parameter of CreateWindowEx.
    HMENU toMenuHandle(int controlId) noexcept;

    /// Creates a child control (position and size are set later by the layout code).
    HWND createChild(HWND parent, HINSTANCE instance, const wchar_t* className, const wchar_t* text,
                     DWORD style, DWORD extendedStyle, int controlId);

    /// Shows a message box.
    void showMessage(HWND owner, const std::wstring& text, const std::wstring& title, UINT icon);
}
