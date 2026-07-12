/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    QT Widgets utility functions
*/

#ifndef WDG_UTILITIES_H
#define WDG_UTILITIES_H

    #include <QWidget>

    #include <QSpinBox>
    #include <QDoubleSpinBox>
    #include <QCheckBox>

    void setSpinValueSilently(QDoubleSpinBox* box, double value);
    void setSpinValueSilently(QSpinBox* box, double value);

    void setCheckboxStateSilently(QCheckBox* box, bool checked);

#endif //WDG_UTILITIES_H