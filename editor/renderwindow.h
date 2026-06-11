/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    render output window
*/

#ifndef RENDERWINDOW_H
#define RENDERWINDOW_H

#include <QWidget>

namespace Ui {
class RenderWindow;
}

class RenderWindow : public QWidget
{
    Q_OBJECT

public:
    explicit RenderWindow(QWidget *parent = nullptr);
    ~RenderWindow();

protected:
    void paintEvent(QPaintEvent * event) override;

private:
    Ui::RenderWindow *ui;
};

#endif // RENDERWINDOW_H
