/**
    RIGGER
    Animation editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    animation frame selector widget
*/

#ifndef WDG_FRAMESELECTOR_H
#define WDG_FRAMESELECTOR_H

#include <QWidget>

class WdgFrameSelector : public QWidget
{
    Q_OBJECT

public:
    explicit WdgFrameSelector(QWidget * parent = nullptr);

    void setScroll(int scroll);
    int getScroll() { return scroll; }

private:
    int scroll;

protected:
    void paintEvent(QPaintEvent * event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void wheelEvent(QWheelEvent * event) override;
};

#endif // WDG_FRAMESELECTOR_H
