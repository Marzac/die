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
    static constexpr float walkSensitivity = 0.125f;
    static constexpr float strafeSensitivity = 0.500f;
    static constexpr float panSensitivity = 1.0f;
    static constexpr float pitchSensitivity = 2.0f;

    static constexpr float pitchSpanMax = 80.0f;

    explicit WdgMapView(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent * event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    int dragX, dragY;
    bool clickRegistered;

    void drawViewLabel(QPainter & painter);
};

#endif // WDG_MAPVIEW_H
