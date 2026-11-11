// =============================================================================
// GuiUtils.cpp
// =============================================================================
#include "GuiUtils.h"

#include <algorithm>

namespace Gui
{
    void GdiObjectDeleter::operator()(HGDIOBJ object) const noexcept
    {
        if (object != nullptr)
        {
            DeleteObject(object);
        }
    }

    FontHandle createUiFont(int pointSize, bool bold, UINT dpi)
    {
        // Negative height = character height in pixels: points * dpi / 72.
        const int height = -MulDiv(pointSize, static_cast<int>(dpi), 72);
        return FontHandle(CreateFontW(height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"));
    }

    int scaleForDpi(int value, UINT dpi) noexcept
    {
        return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
    }

    std::wstring getWindowText(HWND window)
    {
        const int length = GetWindowTextLengthW(window);
        if (length <= 0)
        {
            return std::wstring();
        }
        std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
        const int copied = GetWindowTextW(window, text.data(), length + 1);
        text.resize(static_cast<std::size_t>(std::max(copied, 0)));
        return text;
    }

    HMENU toMenuHandle(int controlId) noexcept
    {
        return reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId));
    }

    HWND createChild(HWND parent, HINSTANCE instance, const wchar_t* className, const wchar_t* text,
                     DWORD style, DWORD extendedStyle, int controlId)
    {
        return CreateWindowExW(extendedStyle, className, text, style, 0, 0, 0, 0, parent,
                               toMenuHandle(controlId), instance, nullptr);
    }

    void showMessage(HWND owner, const std::wstring& text, const std::wstring& title, UINT icon)
    {
        MessageBoxW(owner, text.c_str(), title.c_str(), MB_OK | icon);
    }
}
