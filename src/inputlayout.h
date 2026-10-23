/* SPDX-FileCopyrightText: 2026 Arachna
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef INPUTLAYOUT_H
#define INPUTLAYOUT_H
#include "texttools.h"
#include <QtGlobal>
namespace InputLayout {
bool requestForForegroundWindow(quintptr window, TextTools::LayoutLanguage destination);
}
#endif
