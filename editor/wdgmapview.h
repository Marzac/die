/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    3D map view widget
*/

#ifndef WDG_MAPVIEW_H
#define WDG_MAPVIEW_H

#include <QWidget>

class WdgMapView : public QWidget
{
    Q_OBJECT

public:
    explicit WdgMapView(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent * event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    int dragX;
    bool clickRegistered;

    void drawViewLabel(QPainter & painter);
};

#endif // WDG_MAPVIEW_H
