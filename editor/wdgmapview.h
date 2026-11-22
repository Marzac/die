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
#include <QMouseEvent>
#include <QWheelEvent>

/// Mouse navigation and wall picking, shared by the 3D view and the render window
class ViewMouse
{
public:
    static constexpr float walkSensitivity = 0.125f;
    static constexpr float strafeSensitivity = 0.500f;
    static constexpr float panSensitivity = 1.0f;
    static constexpr float pitchSensitivity = 2.0f;
    static constexpr float pitchSpanMax = 80.0f;

    void press(QMouseEvent * event);
    void move(QMouseEvent * event);
    void release(const QWidget * view, QMouseEvent * event);
    void wheel(QWheelEvent * event);

private:
    QPoint start;
    QPoint last;
    bool dragged = false;
};

class WdgMapView : public QWidget
{
    Q_OBJECT

public:
    explicit WdgMapView(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent * event) override;
    void mousePressEvent(QMouseEvent *event) override {mouse.press(event);}
    void mouseMoveEvent(QMouseEvent *event) override {mouse.move(event);}
    void mouseReleaseEvent(QMouseEvent *event) override {mouse.release(this, event);}
    void wheelEvent(QWheelEvent *event) override {mouse.wheel(event);}

private:
    ViewMouse mouse;
    void drawViewLabel(QPainter & painter);
};

#endif // WDG_MAPVIEW_H
