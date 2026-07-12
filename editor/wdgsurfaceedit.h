#ifndef WDGSURFACEEDIT_H
#define WDGSURFACEEDIT_H

#include "mapobjects.h"

#include <QWidget>

namespace Ui {class WdgSurfaceEdit;}

class WdgSurfaceEdit : public QWidget
{
    Q_OBJECT

public:
    explicit WdgSurfaceEdit(QWidget *parent = nullptr);
    ~WdgSurfaceEdit();

    void setSurface(const Surface * surface);
    void setTextureID(uint16_t id);
    void update();

signals:
    void surfaceChanged(const Surface & surface);

private slots:
    void on_spinID_valueChanged(int arg1);
    void on_spinScaleX_valueChanged(double arg1);
    void on_spinScaleY_valueChanged(double arg1);
    void on_spinShiftX_valueChanged(double arg1);
    void on_spinShiftY_valueChanged(double arg1);

    void on_checkAlpha_toggled(bool checked);
    void on_checkNoLight_toggled(bool checked);
    void on_checkNoGlow_toggled(bool checked);
    void on_checkNoFog_toggled(bool checked);

private:
    Ui::WdgSurfaceEdit *ui;
    Surface surface;
    bool hasSurface;
};

#endif // WDGSURFACEEDIT_H
