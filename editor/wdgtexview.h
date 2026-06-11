/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    texture preview widget
*/

#ifndef WDG_TEXVIEW_H
#define WDG_TEXVIEW_H

#include <QWidget>
#include <stdint.h>

class WdgTexView : public QWidget
{
    Q_OBJECT

public:
    explicit WdgTexView(QWidget *parent = nullptr);

    void setID(uint16_t id);

private:
    uint16_t id;

protected:
    void paintEvent(QPaintEvent * event) override;
};

#endif // WDG_TEXVIEW_H
