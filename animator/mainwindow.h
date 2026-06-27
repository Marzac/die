#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QShortcut>

#include <cstdint>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /// \brief Refresh the joint list and the selected joint's properties
    void updateJointProperties();

    /// \brief Refresh the selected bone's properties
    void updateBoneProperties();

    /// \brief Refresh the viewer controls from the rig view
    void updateViewerProperties();

    /// \brief Assign the selected texture to the current bone arc (image)
    void setTexture(uint16_t texId);

    /// \brief Repaint the rig canvas (e.g. after the current frame changes)
    void updateRigCanvas();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void resizeUI();
    void createShortcuts();
    void setSpinValueSilently(QAbstractSpinBox * box, double value);
    void setCheckboxStateSilently(QCheckBox * box, bool checked);

    QShortcut * shortcutJoints;
    QShortcut * shortcutBones;
    QShortcut * shortcutDeselect;

    Ui::MainWindow *ui;

private slots:
    void on_setJointMode();
    void on_setBoneMode();
    void on_deselect();

    void on_tabRiggerModes_currentChanged(int index);

    void on_spinJointX_valueChanged(double arg1);
    void on_spinJointY_valueChanged(double arg1);
    void on_spinJointZ_valueChanged(double arg1);
    void on_listJoints_itemSelectionChanged();
    void on_pushJointDelete_clicked();

    void on_spinBoneWidth_valueChanged(double arg1);
    void on_spinBoneLength_valueChanged(double arg1);
    void on_spinBoneOffset_valueChanged(double arg1);
    void on_spinBoneMinimumWidth_valueChanged(double arg1);
    void on_checkBoneInvisible_toggled(bool checked);
    void on_checkBoneMirrored_toggled(bool checked);
    void on_checkBoneRotate_toggled(bool checked);
    void on_comboBonePicture_currentIndexChanged(int index);
    void on_spinBoneTexture_valueChanged(int arg1);
    void on_pushBoneDelete_clicked();
    void on_pushBoneSwap_clicked();

    void on_pushTextureBrowse_clicked();
    void on_scrollTextures_valueChanged(int value);

    void on_pushFrameAdd_clicked();
    void on_pushFrameDelete_clicked();
    void on_scrollFrames_valueChanged(int value);
    void on_frameMoveRight_clicked();
    void on_pushFrameMoveLeft_clicked();

    void on_checkDisplayJoints_toggled(bool checked);
    void on_checkDisplayBones_toggled(bool checked);
    void on_checkDisplayFlesh_toggled(bool checked);
    void on_pushAllFrames_toggled(bool checked);

    void on_spinViewerPan_valueChanged(double arg1);
    void on_spinViewerY_valueChanged(double arg1);
    void on_spinViewerZ_valueChanged(double arg1);

    void on_actionNew_triggered();
    void on_actionLoad_triggered();
    void on_actionSave_triggered();
    void on_actionQuit_triggered();
    void on_actionAbout_triggered();
    void on_actionAboutQt_triggered();
};

extern MainWindow * mainWindow;

#endif // MAINWINDOW_H
