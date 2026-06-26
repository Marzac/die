/**
    RIGGER
    Animation editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    rigger entry point
*/

#include "mainwindow.h"

#include "rigger.h"

#include <QApplication>
#include <QMessageBox>

#include <stdint.h>
#include <stdio.h>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif

/*****************************************************************************/
inline bool hasSSE41()
{
#ifdef _MSC_VER
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    return (cpuInfo[2] & (1 << 19)) != 0; // SSE4.1 is bit 19 of ECX
#else
    uint32_t eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);
    return (ecx & (1 << 19)) != 0; // SSE4.1 is bit 19 of ECX
#endif
}

/*****************************************************************************/
MainWindow * mainWindow = nullptr;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    if (!hasSSE41()) {
        QMessageBox::critical(nullptr, "Rigger", "This computer does not support SSE4.1!");
        return -1;
    }

    QApplication::setWindowIcon(QIcon(":/rigger-icon.png"));
    QApplication::setStyle("windows");

    rigger.init();

    mainWindow = new MainWindow();
    mainWindow->show();

    int result = a.exec();

    delete mainWindow;
    mainWindow = nullptr;

    rigger.terminate();

    return result;
}
