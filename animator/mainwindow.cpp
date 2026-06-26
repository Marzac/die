#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "rigger.h"

#include <QResizeEvent>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resizeUI();
    updateJointProperties();
    updateViewerProperties();
}

MainWindow::~MainWindow()
{
    delete ui;
}

/*****************************************************************************/
void MainWindow::resizeEvent(QResizeEvent *)
{
    resizeUI();
    update();
}

void MainWindow::resizeUI()
{
    int cw = ui->centralwidget->width();
    int ch = ui->centralwidget->height();

    const int margin  = 9;
    const int leftW   = 201;   // tools tab
    const int rightW  = 151;   // right-hand column
    const int framesH = 64;
    const int scrollH = 16;

// Left tools tab, full height
    ui->tabRiggerModes->setGeometry(0, 0, leftW, ch - 8);

// Right column, anchored to the right edge (kept stacked as in the .ui)
    int rightX = cw - rightW - 8;
    ui->groupViewer->setGeometry(rightX, 0, rightW, 121);
    ui->groupBox->setGeometry(rightX, 129, rightW, 81);
    ui->groupAnimate->setGeometry(rightX, 220, rightW, 151);
    ui->comboAnimationName->setGeometry(rightX, 380, rightW, 22);

// Frame buttons (2x2), anchored to the bottom, left of the right column
    const int btnW = 71, btnH = 23, gap = 8, rowGap = 7;
    int blockW  = 2 * btnW + gap;
    int bottomH = framesH + 4 + scrollH;
    int framesTop = ch - margin - bottomH;

    int blockX = rightX - margin - blockW;
    ui->pushFrameAdd->setGeometry(blockX, framesTop, btnW, btnH);
    ui->pushFrameDel->setGeometry(blockX, framesTop + btnH + rowGap, btnW, btnH);
    ui->pushFrameCopy->setGeometry(blockX + btnW + gap, framesTop, btnW, btnH);
    ui->pushFramePaste->setGeometry(blockX + btnW + gap, framesTop + btnH + rowGap, btnW, btnH);

// Frames timeline + scrollbar fill the rest of the bottom strip
    int framesX = leftW + margin;
    int framesW = blockX - margin - framesX;
    if (framesW < 1) framesW = 1;
    ui->widgetFrames->setGeometry(framesX, framesTop, framesW, framesH);
    ui->scrollFrames->setGeometry(framesX, framesTop + framesH + 4, framesW, scrollH);

// Central canvas fills the area between the panels
    int canvasX = leftW + margin;
    int canvasY = margin;
    int canvasW = rightX - margin - canvasX;
    int canvasH = framesTop - margin - canvasY;
    if (canvasW < 1) canvasW = 1;
    if (canvasH < 1) canvasH = 1;
    ui->widgetRig->setGeometry(canvasX, canvasY, canvasW, canvasH);
}

/*****************************************************************************/
void MainWindow::setSpinValueSilently(QAbstractSpinBox * box, double value)
{
    if (auto * doubleSpin = qobject_cast<QDoubleSpinBox *>(box)) {
        doubleSpin->blockSignals(true);
        doubleSpin->setValue(value);
        doubleSpin->blockSignals(false);

    } else if (auto * intSpin = qobject_cast<QSpinBox *>(box)) {
        intSpin->blockSignals(true);
        intSpin->setValue(static_cast<int>(value));
        intSpin->blockSignals(false);
    }
}

/*****************************************************************************/
void MainWindow::updateJointProperties()
{
    Frame * cur = rigger.rig.currentFramePtr();
    int count = cur ? cur->joints.count() : 0;

// Rebuild the list when the joint count changes
    if (ui->listJoints->count() != count) {
        ui->listJoints->blockSignals(true);
        ui->listJoints->clear();
        for (int i = 0; i < count; i++)
            ui->listJoints->addItem(QString::asprintf("Joint %04X", (unsigned int) i));
        ui->listJoints->blockSignals(false);
    }

// Mirror the per-joint selection into the list
    ui->listJoints->blockSignals(true);
    for (int i = 0; i < count; i++)
        ui->listJoints->item(i)->setSelected(cur->joints[i].selected);
    ui->listJoints->blockSignals(false);

    if (rigger.selectedJoint < 0 || rigger.selectedJoint >= count) {
        ui->plainJointID->setPlainText("None");
        setSpinValueSilently(ui->spinJointX, 0.0);
        setSpinValueSilently(ui->spinJointY, 0.0);
        setSpinValueSilently(ui->spinJointZ, 0.0);
        return;
    }

    Joint & j = cur->joints[rigger.selectedJoint];
    ui->plainJointID->setPlainText(QString::number(rigger.selectedJoint));
    setSpinValueSilently(ui->spinJointX, j.pos.x());
    setSpinValueSilently(ui->spinJointY, j.pos.y());
    setSpinValueSilently(ui->spinJointZ, j.pos.z());
}

/*****************************************************************************/
void MainWindow::on_spinJointX_valueChanged(double arg1)
{
    Frame * cur = rigger.rig.currentFramePtr();
    if (!cur || rigger.selectedJoint < 0) return;
    for (Joint & j : cur->joints) {
        if (!j.selected) continue;
        j.pos.setX(arg1);
        j.apos.setX(arg1);
    }
    ui->widgetRig->update();
}

void MainWindow::on_spinJointY_valueChanged(double arg1)
{
    Frame * cur = rigger.rig.currentFramePtr();
    if (!cur || rigger.selectedJoint < 0) return;
    for (Joint & j : cur->joints) {
        if (!j.selected) continue;
        j.pos.setY(arg1);
        j.apos.setY(arg1);
    }
    ui->widgetRig->update();
}

void MainWindow::on_spinJointZ_valueChanged(double arg1)
{
    Frame * cur = rigger.rig.currentFramePtr();
    if (!cur || rigger.selectedJoint < 0) return;
    for (Joint & j : cur->joints) {
        if (!j.selected) continue;
        j.pos.setZ(arg1);
        j.apos.setZ(arg1);
    }
    ui->widgetRig->update();
}

void MainWindow::on_listJoints_itemSelectionChanged()
{
    Frame * cur = rigger.rig.currentFramePtr();
    if (!cur) return;

    rigger.jointDeselectAll();
    rigger.selectedJoint = RIG_UNSELECTED;

    for (int row = 0; row < ui->listJoints->count() && row < cur->joints.count(); row++) {
        if (!ui->listJoints->item(row)->isSelected()) continue;
        cur->joints[row].selected = true;
        rigger.selectedJoint = row;
    }

    int current = ui->listJoints->currentRow();
    if (current >= 0 && current < cur->joints.count() &&
        ui->listJoints->item(current)->isSelected())
        rigger.selectedJoint = current;

    updateJointProperties();
    ui->widgetRig->update();
}

void MainWindow::on_pushJointDelete_clicked()
{
    if (rigger.selectedJoint < 0) return;
    rigger.jointDelete(rigger.selectedJoint);
    updateJointProperties();
    ui->widgetRig->update();
}

/*****************************************************************************/
void MainWindow::updateViewerProperties()
{
    setSpinValueSilently(ui->spinViewerPan, rigger.rigView.pan);
    setSpinValueSilently(ui->spinViewerY, rigger.rigView.y);
    setSpinValueSilently(ui->spinViewerZ, rigger.rigView.diameter);
}

void MainWindow::on_spinViewerPan_valueChanged(double arg1)
{
    rigger.rigView.pan = arg1;
    ui->widgetRig->update();
}

void MainWindow::on_spinViewerY_valueChanged(double arg1)
{
    rigger.rigView.y = arg1;
    ui->widgetRig->update();
}

void MainWindow::on_spinViewerZ_valueChanged(double arg1)
{
    rigger.rigView.diameter = arg1;
    ui->widgetRig->update();
}

/*****************************************************************************/
void MainWindow::on_actionNew_triggered()
{
    rigger.init();
    updateJointProperties();
    updateViewerProperties();
    ui->widgetRig->update();
    setWindowTitle("Rigger");
}

void MainWindow::on_actionLoad_triggered()
{
    QString path = rigger.rig.path.isEmpty() ? QDir::currentPath() : rigger.rig.path;
    QString file = QFileDialog::getOpenFileName(this, "Open rig", path, "Rig File (*.rig)");
    if (file.isEmpty()) return;

    rigger.rig.load(file);
    rigger.deselect();
    updateJointProperties();
    ui->widgetRig->update();
    setWindowTitle("Rigger : " + QFileInfo(file).fileName());
}

void MainWindow::on_actionSave_triggered()
{
    QString path = rigger.rig.path.isEmpty() ? QDir::currentPath() : rigger.rig.path;
    QString file = QFileDialog::getSaveFileName(this, "Save rig", path, "Rig File (*.rig)");
    if (file.isEmpty()) return;
    if (!file.endsWith(".rig", Qt::CaseInsensitive)) file += ".rig";

    rigger.rig.save(file);
    setWindowTitle("Rigger : " + QFileInfo(file).fileName());
}

void MainWindow::on_actionQuit_triggered()
{
    close();
}

/*****************************************************************************/
void MainWindow::on_actionAbout_triggered()
{
    QString aboutText;
    aboutText += "<b>Rigger</b><br>";
    aboutText += "Animation editor for the <b>DIE Engine</b><br><br>";
    aboutText += "© 2024–2026 Frédéric Meslin<br>";
    aboutText += "<a href=\"https://fredslab.net\">Fred's Lab</a><br>";
    aboutText += "<a href=\"mailto:info@fredslab.net\">info@fredslab.net</a><br><br>";
    aboutText += "Open Source under the <b>MIT</b> license.<br>";
    aboutText += "If used commercially, contributions and donations are highly appreciated.<br><br>";
    aboutText += "Rigger is an open-source character animation tool developed by Fred's Lab,<br>"
                 "for building animated sprites for DIE (Depth Integration Engine).";

    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle("About Rigger");
    aboutBox.setTextFormat(Qt::RichText);
    aboutBox.setTextInteractionFlags(Qt::TextBrowserInteraction);
    aboutBox.setIcon(QMessageBox::Information);
    aboutBox.setText(aboutText);
    aboutBox.exec();
}

void MainWindow::on_actionAboutQt_triggered()
{
    QMessageBox::aboutQt(this, "About Qt");
}
