/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    editor entry point
*/

#include "mainwindow.h"

#include "editor.h"
#include "renderer.h"
#include "walker.h"
#include "audio.h"

#include <QApplication>
#include <QMessageBox>

#include <stdint.h>
#include <stdio.h>
#include <cpuid.h>

/*****************************************************************************/
inline bool hasSSE41()
{
    uint32_t eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);
    return (ecx & (1 << 19)) != 0; // SSE4.1 is bit 19 of ECX
}

/*****************************************************************************/
MainWindow * mainWindow = nullptr;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    if (!hasSSE41()) {
        QMessageBox::critical(nullptr, "Waller", "This computer does not support SSE4.1!");
        return -1;
    }

    QApplication::setWindowIcon(QIcon(":/editor-icon.png"));
    QApplication::setStyle("windows");

    renderer.init();
    audio.init();
    editor.init();
    walker.init();

    mainWindow = new MainWindow();
    mainWindow->showMaximized();

    int result = a.exec();

    delete mainWindow;
    mainWindow = nullptr;

    walker.terminate();
    editor.terminate();
    audio.terminate();
    renderer.terminate();

    return result;
}
