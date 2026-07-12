/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    QT Widgets utility functions
*/

#include "wdgutilities.h"

#include <QWidget>

#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>

void setSpinValueSilently(QDoubleSpinBox* box, double value)
{
    box->blockSignals(true);
    box->setValue(value);
    box->blockSignals(false);
}

void setSpinValueSilently(QSpinBox* box, double value)
{
    box->blockSignals(true);
    box->setValue(static_cast<int>(value));
    box->blockSignals(false);
}

void setCheckboxStateSilently(QCheckBox* box, bool checked)
{
    box->blockSignals(true);
    box->setChecked(checked);
    box->blockSignals(false);
}
