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

#include <utility>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resizeUI();
    rigger.mode = (RIG_MODES) ui->tabRiggerModes->currentIndex();

    setCheckboxStateSilently(ui->checkDisplayJoints, rigger.flags & FLAG_DISPLAY_JOINTS);
    setCheckboxStateSilently(ui->checkDisplayBones,  rigger.flags & FLAG_DISPLAY_BONES);
    setCheckboxStateSilently(ui->checkDisplayFlesh,  rigger.flags & FLAG_DISPLAY_FLESH);
    ui->pushAllFrames->blockSignals(true);
    ui->pushAllFrames->setChecked(rigger.editAllFrames);
    ui->pushAllFrames->blockSignals(false);

    updateJointProperties();
    updateBoneProperties();
    updateViewerProperties();

    createShortcuts();
}

/*****************************************************************************/
void MainWindow::createShortcuts()
{
    shortcutJoints = new QShortcut(QKeySequence(Qt::Key_J), this);
    connect(shortcutJoints, SIGNAL(activated()), this, SLOT(on_setJointMode()));

    shortcutBones = new QShortcut(QKeySequence(Qt::Key_B), this);
    connect(shortcutBones, SIGNAL(activated()), this, SLOT(on_setBoneMode()));

    shortcutDeselect = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(shortcutDeselect, SIGNAL(activated()), this, SLOT(on_deselect()));
}

void MainWindow::on_setJointMode() { ui->tabRiggerModes->setCurrentIndex(RIG_MODE_JOINTS); }
void MainWindow::on_setBoneMode()  { ui->tabRiggerModes->setCurrentIndex(RIG_MODE_BONES); }

void MainWindow::on_deselect()
{
    rigger.deselect();
    updateJointProperties();
    updateBoneProperties();
    ui->widgetRig->update();
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

// Frame buttons (3 columns x 2 rows), anchored to the bottom, left of the right column
    const int btnW = 71, btnH = 23, gap = 8, rowGap = 7, moveW = 51;
    int blockW  = 2 * btnW + 2 * gap + moveW;
    int bottomH = framesH + 4 + scrollH;
    int framesTop = ch - margin - bottomH;

    int blockX = rightX - margin - blockW;
    ui->pushFrameAdd->setGeometry(blockX, framesTop, btnW, btnH);
    ui->pushFrameDelete->setGeometry(blockX, framesTop + btnH + rowGap, btnW, btnH);
    ui->pushFrameCopy->setGeometry(blockX + btnW + gap, framesTop, btnW, btnH);
    ui->pushFramePaste->setGeometry(blockX + btnW + gap, framesTop + btnH + rowGap, btnW, btnH);
    ui->frameMoveRight->setGeometry(blockX + 2 * btnW + 2 * gap, framesTop, moveW, btnH);
    ui->pushFrameMoveLeft->setGeometry(blockX + 2 * btnW + 2 * gap, framesTop + btnH + rowGap, moveW, btnH);

// Frames timeline + scrollbar fill the rest of the bottom strip
    int framesX = leftW + margin;
    int framesW = blockX - margin - framesX;
    if (framesW < 1) framesW = 1;
    ui->widgetFrames->setGeometry(framesX, framesTop, framesW, framesH);
    ui->scrollFrames->setGeometry(framesX, framesTop + framesH + 4, framesW, scrollH);

// Texture strip selector + scrollbar, just above the frames block
    const int texSelH = 50, texScrollH = 16;
    int canvasX = leftW + margin;
    int canvasW = rightX - margin - canvasX;
    if (canvasW < 1) canvasW = 1;

    int texTop = framesTop - 6 - (texSelH + 4 + texScrollH);
    ui->widgetTextureSelector->setGeometry(canvasX, texTop, canvasW, texSelH);
    ui->scrollTextures->setGeometry(canvasX, texTop + texSelH + 4, canvasW, texScrollH);
    ui->pushTextureBrowse->setGeometry(canvasW - 31, (texSelH - 21) / 2, 21, 21);

// Central canvas fills the area between the panels and the texture block
    int canvasY = margin;
    int canvasH = texTop - margin - canvasY;
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

void MainWindow::setCheckboxStateSilently(QCheckBox * box, bool checked)
{
    box->blockSignals(true);
    box->setChecked(checked);
    box->blockSignals(false);
}

/*****************************************************************************/
void MainWindow::on_tabRiggerModes_currentChanged(int index)
{
    rigger.mode = (RIG_MODES) index;
    ui->widgetRig->update();
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
void MainWindow::updateBoneProperties()
{
    int count = rigger.rig.bones.count();

    if (rigger.selectedBone < 0 || rigger.selectedBone >= count) {
        ui->plainBoneID->setPlainText("None");
        setSpinValueSilently(ui->spinBoneWidth, 0.0);
        setSpinValueSilently(ui->spinBoneLength, 0.0);
        setSpinValueSilently(ui->spinBoneOffset, 0.0);
        setSpinValueSilently(ui->spinBoneMinimumWidth, 0.0);
        setCheckboxStateSilently(ui->checkBoneInvisible, false);
        setCheckboxStateSilently(ui->checkBoneMirrored, false);
        setCheckboxStateSilently(ui->checkBoneRotate, false);
        setSpinValueSilently(ui->spinBoneTexture, 0.0);
        ui->widgetBoneTexture->setID(0);
        return;
    }

    Bone & b = rigger.rig.bones[rigger.selectedBone];
    int arc = ui->comboBonePicture->currentIndex();
    if (arc < 0) arc = 0;

    ui->plainBoneID->setPlainText(QString::number(rigger.selectedBone));
    setSpinValueSilently(ui->spinBoneWidth, b.width);
    setSpinValueSilently(ui->spinBoneLength, b.length);
    setSpinValueSilently(ui->spinBoneOffset, b.offset);
    setSpinValueSilently(ui->spinBoneMinimumWidth, b.minWidth);
    setCheckboxStateSilently(ui->checkBoneInvisible, b.flags & BONE_FLAG_INVISIBLE);
    setCheckboxStateSilently(ui->checkBoneMirrored, b.flags & BONE_FLAG_MIRROR);
    setCheckboxStateSilently(ui->checkBoneRotate, b.flags & BONE_FLAG_ROTATE);
    setSpinValueSilently(ui->spinBoneTexture, b.images[arc]);
    ui->widgetBoneTexture->setID(b.images[arc]);
}

void MainWindow::on_spinBoneWidth_valueChanged(double arg1)
{
    if (rigger.selectedBone < 0) return;
    for (Bone & b : rigger.rig.bones) {
        if (!b.selected) continue;
        b.width = arg1;
    }
    ui->widgetRig->update();
}

void MainWindow::on_spinBoneLength_valueChanged(double arg1)
{
    if (rigger.selectedBone < 0) return;
    for (Bone & b : rigger.rig.bones) {
        if (!b.selected) continue;
        b.length = arg1;
    }
    ui->widgetRig->update();
}

void MainWindow::on_spinBoneOffset_valueChanged(double arg1)
{
    if (rigger.selectedBone < 0) return;
    for (Bone & b : rigger.rig.bones) {
        if (!b.selected) continue;
        b.offset = arg1;
    }
    ui->widgetRig->update();
}

void MainWindow::on_spinBoneMinimumWidth_valueChanged(double arg1)
{
    if (rigger.selectedBone < 0) return;
    for (Bone & b : rigger.rig.bones) {
        if (!b.selected) continue;
        b.minWidth = arg1;
    }
    ui->widgetRig->update();
}

void MainWindow::on_checkBoneInvisible_toggled(bool checked)
{
    if (rigger.selectedBone < 0) return;
    for (Bone & b : rigger.rig.bones) {
        if (!b.selected) continue;
        if (checked) b.flags |= BONE_FLAG_INVISIBLE;
        else         b.flags &= ~BONE_FLAG_INVISIBLE;
    }
    ui->widgetRig->update();
}

void MainWindow::on_checkBoneMirrored_toggled(bool checked)
{
    if (rigger.selectedBone < 0) return;
    for (Bone & b : rigger.rig.bones) {
        if (!b.selected) continue;
        if (checked) b.flags |= BONE_FLAG_MIRROR;
        else         b.flags &= ~BONE_FLAG_MIRROR;
    }
    ui->widgetRig->update();
}

void MainWindow::on_checkBoneRotate_toggled(bool checked)
{
    if (rigger.selectedBone < 0) return;
    for (Bone & b : rigger.rig.bones) {
        if (!b.selected) continue;
        if (checked) b.flags |= BONE_FLAG_ROTATE;
        else         b.flags &= ~BONE_FLAG_ROTATE;
    }
    ui->widgetRig->update();
}

void MainWindow::on_comboBonePicture_currentIndexChanged(int index)
{
// Show the image assigned to the newly selected arc
    if (rigger.selectedBone < 0) {
        setSpinValueSilently(ui->spinBoneTexture, 0.0);
        ui->widgetBoneTexture->setID(0);
        return;
    }
    int arc = index < 0 ? 0 : index;
    Bone & b = rigger.rig.bones[rigger.selectedBone];
    setSpinValueSilently(ui->spinBoneTexture, b.images[arc]);
    ui->widgetBoneTexture->setID(b.images[arc]);
    rigger.selectedTextureID = b.images[arc];
    ui->widgetTextureSelector->update();
}

void MainWindow::on_spinBoneTexture_valueChanged(int arg1)
{
    if (rigger.selectedBone < 0) return;
    int arc = ui->comboBonePicture->currentIndex();
    if (arc < 0) arc = 0;
    for (Bone & b : rigger.rig.bones) {
        if (!b.selected) continue;
        b.images[arc] = (uint16_t) arg1;
        if (b.imageCount < arc + 1) b.imageCount = (uint16_t)(arc + 1);
    }
    ui->widgetBoneTexture->setID((uint16_t) arg1);
    ui->widgetRig->update();
}

void MainWindow::on_pushBoneDelete_clicked()
{
    if (rigger.selectedBone < 0) return;
    rigger.boneDelete(rigger.selectedBone);
    updateBoneProperties();
    ui->widgetRig->update();
}

void MainWindow::on_pushBoneSwap_clicked()
{
    if (rigger.selectedBone < 0) return;
    for (Bone & b : rigger.rig.bones) {
        if (!b.selected) continue;
        std::swap(b.jointID1, b.jointID2);
    }
    ui->widgetRig->update();
}

/*****************************************************************************/
void MainWindow::setTexture(uint16_t texId)
{
    rigger.selectedTextureID = texId;
    if (rigger.selectedBone < 0) return;

    int arc = ui->comboBonePicture->currentIndex();
    if (arc < 0) arc = 0;

    for (Bone & b : rigger.rig.bones) {
        if (!b.selected) continue;
        b.images[arc] = texId;
        if (b.imageCount < arc + 1) b.imageCount = (uint16_t)(arc + 1);
    }
    setSpinValueSilently(ui->spinBoneTexture, texId);
    ui->widgetBoneTexture->setID(texId);
    ui->widgetRig->update();
}

void MainWindow::on_pushTextureBrowse_clicked()
{
    QString path = QDir::currentPath();
    QString file = QFileDialog::getOpenFileName(this, "Open texture strip", path, "Image File (*.bmp *.png *.jpg)");
    if (file.isEmpty()) return;

    rigger.rig.textures.load(file);
    rigger.selectedTextureID = 0;
    ui->widgetTextureSelector->setScroll(0);
    ui->widgetTextureSelector->update();
}

void MainWindow::on_scrollTextures_valueChanged(int value)
{
    ui->widgetTextureSelector->setScroll(value);
}

/*****************************************************************************/
void MainWindow::updateRigCanvas()
{
    ui->widgetRig->update();
}

void MainWindow::on_pushFrameAdd_clicked()
{
    rigger.rig.frameInsert(rigger.rig.currentFrame + 1);
    updateJointProperties();
    ui->widgetFrames->update();
    ui->widgetRig->update();
}

void MainWindow::on_pushFrameDelete_clicked()
{
    rigger.rig.frameDelete(rigger.rig.currentFrame);
    updateJointProperties();
    ui->widgetFrames->update();
    ui->widgetRig->update();
}

void MainWindow::on_scrollFrames_valueChanged(int value)
{
    ui->widgetFrames->setScroll(value);
}

void MainWindow::on_frameMoveRight_clicked()
{
    Animation * a = rigger.rig.currentAnimationPtr();
    if (!a) return;
    int cur = rigger.rig.currentFrame;
    if (cur < 0 || cur >= a->frames.count() - 1) return;

    a->frames.swapItemsAt(cur, cur + 1);
    rigger.rig.frameSelect(cur + 1);
    updateJointProperties();
    ui->widgetFrames->update();
    ui->widgetRig->update();
}

void MainWindow::on_pushFrameMoveLeft_clicked()
{
    Animation * a = rigger.rig.currentAnimationPtr();
    if (!a) return;
    int cur = rigger.rig.currentFrame;
    if (cur <= 0 || cur >= a->frames.count()) return;

    a->frames.swapItemsAt(cur, cur - 1);
    rigger.rig.frameSelect(cur - 1);
    updateJointProperties();
    ui->widgetFrames->update();
    ui->widgetRig->update();
}

/*****************************************************************************/
void MainWindow::on_checkDisplayJoints_toggled(bool checked)
{
    if (checked) rigger.flags |= FLAG_DISPLAY_JOINTS;
    else         rigger.flags &= ~FLAG_DISPLAY_JOINTS;
    ui->widgetRig->update();
}

void MainWindow::on_checkDisplayBones_toggled(bool checked)
{
    if (checked) rigger.flags |= FLAG_DISPLAY_BONES;
    else         rigger.flags &= ~FLAG_DISPLAY_BONES;
    ui->widgetRig->update();
}

void MainWindow::on_checkDisplayFlesh_toggled(bool checked)
{
    if (checked) rigger.flags |= FLAG_DISPLAY_FLESH;
    else         rigger.flags &= ~FLAG_DISPLAY_FLESH;
    ui->widgetRig->update();
}

void MainWindow::on_pushAllFrames_toggled(bool checked)
{
    rigger.editAllFrames = checked;
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
    rigger.mode = (RIG_MODES) ui->tabRiggerModes->currentIndex();
    updateJointProperties();
    updateBoneProperties();
    updateViewerProperties();
    ui->widgetRig->update();
    ui->widgetTextureSelector->update();
    ui->widgetFrames->update();
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
    updateBoneProperties();
    ui->widgetRig->update();
    ui->widgetTextureSelector->update();
    ui->widgetFrames->update();
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
