/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    texture strip selector widget
*/

#ifndef WDG_TEX_SELECTOR_H
#define WDG_TEX_SELECTOR_H

#include <QWidget>

class WdgTexSelector : public QWidget
{
    Q_OBJECT

public:
    explicit WdgTexSelector(QWidget *parent = nullptr);
    void setScroll(int scroll);
    int getScroll() {return scroll;}

    /// \brief Export every tile as a 3x3 mosaik PNG (for seamless painting)
    void slice(const QString & path);

    /// \brief Rebuild the strip from 3x3 mosaik PNGs (centre tile of each)
    void concat(const QString & path);

private:
    int scroll;

protected:
    void paintEvent(QPaintEvent * event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
};

#endif // WDG_TEX_SELECTOR_H
