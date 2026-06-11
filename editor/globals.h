/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    editor globals
*/

#ifndef GLOBALS_H
#define GLOBALS_H

class MainWindow;

static constexpr float FPS = 60.0f;
static constexpr float dt = 1.0f / FPS;

extern MainWindow * mainWindow;

#endif // GLOBALS_H
