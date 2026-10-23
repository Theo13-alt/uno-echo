/* SPDX-FileCopyrightText: 2026 Arachna
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "inputlayout.h"
#include <vector>
#include <windows.h>

bool InputLayout::requestForForegroundWindow(quintptr window, TextTools::LayoutLanguage destination)
{
    const auto target = reinterpret_cast<HWND>(window);
    if (destination == TextTools::LayoutLanguage::Unchanged || !IsWindow(target)
        || GetForegroundWindow() != target) return false;
    const DWORD thread = GetWindowThreadProcessId(target, nullptr);
    if (!thread) return false;
    const LANGID language = destination == TextTools::LayoutLanguage::Russian ? 0x0419 : 0x0409;
    const HKL current = GetKeyboardLayout(thread);
    if (LOWORD(reinterpret_cast<quintptr>(current)) == language) return true;
    const int count = GetKeyboardLayoutList(0, nullptr);
    if (count <= 0) return false;
    std::vector<HKL> installed(static_cast<size_t>(count));
    const int available = GetKeyboardLayoutList(count, installed.data());
    HKL selected = nullptr;
    for (int i = 0; i < available; ++i) {
        if (LOWORD(reinterpret_cast<quintptr>(installed[i])) == language) {
            selected = installed[i]; break;
        }
    }
    if (!selected) return false;
    GUITHREADINFO info{}; info.cbSize = sizeof(info);
    HWND focus = target;
    if (GetGUIThreadInfo(thread, &info) && info.hwndFocus
        && GetAncestor(info.hwndFocus, GA_ROOT) == target) focus = info.hwndFocus;
    if (GetForegroundWindow() != target) return false;
    // Ask the source editor's thread; ActivateKeyboardLayout would change echo's thread instead.
    return PostMessageW(focus, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(selected));
}
