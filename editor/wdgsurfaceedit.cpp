#include "wdgsurfaceedit.h"
#include "ui_wdgsurfaceedit.h"

#include "wdgutilities.h"

WdgSurfaceEdit::WdgSurfaceEdit(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WdgSurfaceEdit)
    , surface{}
    , hasSurface(false)
{
    ui->setupUi(this);
}

WdgSurfaceEdit::~WdgSurfaceEdit()
{
    delete ui;
}

/*****************************************************************************/
void WdgSurfaceEdit::setSurface(const Surface * s)
{
    hasSurface = s != nullptr;
    if (hasSurface) surface = *s;
    update();
}

void WdgSurfaceEdit::setTextureID(uint16_t id)
{
    ui->spinID->setValue(id);
}

void WdgSurfaceEdit::update()
{
    if (!hasSurface) {
        ui->widgetTexture->setID(0);
        setSpinValueSilently(ui->spinID, 0);
        setSpinValueSilently(ui->spinScaleX, 0.0);
        setSpinValueSilently(ui->spinScaleY, 0.0);
        setSpinValueSilently(ui->spinShiftX, 0.0);
        setSpinValueSilently(ui->spinShiftY, 0.0);
        setCheckboxStateSilently(ui->checkAlpha, false);
        setCheckboxStateSilently(ui->checkNoLight, false);
        setCheckboxStateSilently(ui->checkNoGlow, false);
        setCheckboxStateSilently(ui->checkNoFog, false);
        return;
    }

    setSpinValueSilently(ui->spinID, surface.id);
    ui->widgetTexture->setID(surface.id);
    setSpinValueSilently(ui->spinScaleX, surface.scaleX);
    setSpinValueSilently(ui->spinScaleY, surface.scaleY);
    setSpinValueSilently(ui->spinShiftX, surface.shiftX);
    setSpinValueSilently(ui->spinShiftY, surface.shiftY);
    setCheckboxStateSilently(ui->checkAlpha, surface.flags & SURFACE_FLAG_ALPHA);
    setCheckboxStateSilently(ui->checkNoLight, surface.flags & SURFACE_FLAG_NO_LIGHT);
    setCheckboxStateSilently(ui->checkNoGlow, surface.flags & SURFACE_FLAG_NO_GLOW);
    setCheckboxStateSilently(ui->checkNoFog, surface.flags & SURFACE_FLAG_NO_FOG);
}

/*****************************************************************************/
void WdgSurfaceEdit::on_spinID_valueChanged(int arg1)
{
    if (!hasSurface) return;
    surface.id = arg1;
    ui->widgetTexture->setID(arg1);
    emit surfaceChanged(surface);
}

void WdgSurfaceEdit::on_spinScaleX_valueChanged(double arg1)
{
    if (!hasSurface) return;
    surface.scaleX = arg1;
    emit surfaceChanged(surface);
}

void WdgSurfaceEdit::on_spinScaleY_valueChanged(double arg1)
{
    if (!hasSurface) return;
    surface.scaleY = arg1;
    emit surfaceChanged(surface);
}

void WdgSurfaceEdit::on_spinShiftX_valueChanged(double arg1)
{
    if (!hasSurface) return;
    surface.shiftX = arg1;
    emit surfaceChanged(surface);
}

void WdgSurfaceEdit::on_spinShiftY_valueChanged(double arg1)
{
    if (!hasSurface) return;
    surface.shiftY = arg1;
    emit surfaceChanged(surface);
}

void WdgSurfaceEdit::on_checkAlpha_toggled(bool checked)
{
    if (!hasSurface) return;
    surface.flags &= ~SURFACE_FLAG_ALPHA;
    if (checked) surface.flags |= SURFACE_FLAG_ALPHA;
    emit surfaceChanged(surface);
}

void WdgSurfaceEdit::on_checkNoLight_toggled(bool checked)
{
    if (!hasSurface) return;
    surface.flags &= ~SURFACE_FLAG_NO_LIGHT;
    if (checked) surface.flags |= SURFACE_FLAG_NO_LIGHT;
    emit surfaceChanged(surface);
}

void WdgSurfaceEdit::on_checkNoGlow_toggled(bool checked)
{
    if (!hasSurface) return;
    surface.flags &= ~SURFACE_FLAG_NO_GLOW;
    if (checked) surface.flags |= SURFACE_FLAG_NO_GLOW;
    emit surfaceChanged(surface);
}

void WdgSurfaceEdit::on_checkNoFog_toggled(bool checked)
{
    if (!hasSurface) return;
    surface.flags &= ~SURFACE_FLAG_NO_FOG;
    if (checked) surface.flags |= SURFACE_FLAG_NO_FOG;
    emit surfaceChanged(surface);
}
