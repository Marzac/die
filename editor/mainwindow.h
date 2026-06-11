/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    editor main window
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "renderwindow.h"

#include "editor.h"
#include "map.h"

#include <QMainWindow>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QList>
#include <QShortcut>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    EDIT_MODES uiEditMode;
    bool viewEnableTop;
    bool viewEnableSide;
    bool viewEnableFront;
    bool viewEnable3D;

    bool once;
    QTimer * renderTimer;
    QTimer * uiTimer;
    RenderWindow * renderWindow;

    void setEditMode(EDIT_MODES mode);
    void setTexture(uint16_t texId);

    void updateNodeProperties();
    void updateWallProperties();
    void updateWallTexture();
    void updateSubmapProperties();
    void updateDoorProperties();
    void updateDoorTexture();
    void updateLiftProperties();
    void updateLiftTexture();
    void updateSpriteProperties();
    void updateSpriteTexture();
    void updateStaircaseProperties();
    void updateStaircaseTexture();
    void updateLightProperties();
    void updateSpeakerProperties();
    void updatePathProperties();
    void updateTagProperties();
    void updateSunProperties();
    void updateFogProperties();
    void updateEngineProperties();
    void updateEditorProperties();

    /// \brief Refresh every per-object properties panel
    void updateAllProperties();

    void refreshTagCombos();

    void pushUndoState();
    void scheduleUndoPush();
    void undo();
    void redo();

    void updateViewpointProperties();
    void updateViewNormals(bool normals);
    void updateViewMode();

    void saveMap(bool submap);

private:
    void resizeUI();
    void resizeEvent(QResizeEvent *event);
    void closeEvent(QCloseEvent *event);

    void setSpinValueSilently(QAbstractSpinBox* box, double value);
    void setCheckboxStateSilently(QCheckBox* box, bool checked);

    void createShortcuts();
    void createContextMenu();

    void createRenderWindow();
    void createNetworkWindow();

    QShortcut * shortcutEditNodes;
    QShortcut * shortcutEditWalls;
    QShortcut * shortcutEditSubmaps;
    QShortcut * shortcutEditDoors;
    QShortcut * shortcutEditLifts;
    QShortcut * shortcutEditSprites;
    QShortcut * shortcutEditStaircases;
    QShortcut * shortcutEditLights;
    QShortcut * shortcutEditSpeakers;
    QShortcut * shortcutEditDelete;

private slots:
    void timerRender_tick();
    void timerUI_tick();
    void contextMenu_show(const QPoint &pos);

    void on_setNodeMode();
    void on_setWallMode();
    void on_setSubmapMode();
    void on_setDoorMode();
    void on_setLiftMode();
    void on_setSpriteMode();
    void on_setStaircaseMode();
    void on_setLightMode();
    void on_setSpeakerMode();
    void on_deleteCurrent();

    void on_tabEditorModes_currentChanged(int index);

    void on_checkEditorSnap_toggled(bool checked);
    void on_comboEditorSnap_currentIndexChanged(int index);
    void on_scrollTextures_valueChanged(int value);

    void on_spinNodeX_valueChanged(double arg1);
    void on_spinNodeY_valueChanged(double arg1);
    void on_spinNodeZ_valueChanged(double arg1);
    void on_spinNodeMetaA_valueChanged(double arg1);
    void on_spinNodeMetaB_valueChanged(double arg1);
    void on_spinNodeMetaC_valueChanged(double arg1);
    void on_comboNodeTag_currentIndexChanged(int index);
    void on_listNodes_itemSelectionChanged();
    void on_pushNodeAddSubmap_clicked();
    void on_pushNodeAddDoor_clicked();
    void on_pushNodeAddStaircase_clicked();
    void on_pushNodeAddLight_clicked();
    void on_pushNodeAddSprite_clicked();
    void on_pushNodeAddSpeaker_clicked();
    void on_pushNodeDelete_clicked();

    void on_spinWallHeight_valueChanged(double arg1);
    void on_comboWallTexture_currentIndexChanged(int index);
    void on_spinWallTexture_valueChanged(int arg1);
    void on_spinWallScaleX_valueChanged(double arg1);
    void on_spinWallScaleY_valueChanged(double arg1);
    void on_spinWallShiftX_valueChanged(double arg1);
    void on_spinWallShiftY_valueChanged(double arg1);
    void on_checkWallInvisible_toggled(bool checked);
    void on_checkWallAlpha_toggled(bool checked);
    void on_checkWallBackculled_toggled(bool checked);
    void on_checkWallCeilingFront_toggled(bool checked);
    void on_checkWallCeilingBack_toggled(bool checked);
    void on_checkWallFloorFront_toggled(bool checked);
    void on_checkWallFloorBack_toggled(bool checked);
    void on_pushWallSwap_clicked();
    void on_pushWallDelete_clicked();

    void on_plainSubmapName_textChanged();
    void on_comboSubmapTag_currentIndexChanged(int index);
    void on_spinSubmapPan_valueChanged(double arg1);
    void on_spinSubmapScale_valueChanged(double arg1);
    void on_pushSubmapBrowse_clicked();
    void on_pushSubmapDelete_clicked();

    void on_plainDoorName_textChanged();
    void on_comboDoorTag_currentIndexChanged(int index);
    void on_spinDoorWidth_valueChanged(double arg1);
    void on_spinDoorHeight_valueChanged(double arg1);
    void on_spinDoorThick_valueChanged(double arg1);
    void on_comboDoorMode_currentIndexChanged(int index);
    void on_spinDoorAngle_valueChanged(double arg1);
    void on_spinDoorSwing_valueChanged(double arg1);
    void on_spinDoorTime_valueChanged(double arg1);
    void on_checkDoorAlpha_toggled(bool checked);
    void on_checkDoorLocked_toggled(bool checked);
    void on_comboDoorEasing_currentIndexChanged(int index);
    void on_comboDoorTexture_currentIndexChanged(int index);
    void on_spinDoorTexture_valueChanged(int arg1);
    void on_spinDoorScaleX_valueChanged(double arg1);
    void on_spinDoorScaleY_valueChanged(double arg1);
    void on_pushDoorOpen_clicked();
    void on_pushDoorClose_clicked();
    void on_pushDoorShake_clicked();
    void on_pushDoorDelete_clicked();

    void on_plainLiftName_textChanged();
    void on_comboLiftTag_currentIndexChanged(int index);
    void on_spinLiftWidth_valueChanged(double arg1);
    void on_spinLiftLength_valueChanged(double arg1);
    void on_spinLiftThick_valueChanged(double arg1);
    void on_spinLiftTravel_valueChanged(double arg1);
    void on_spinLiftTime_valueChanged(double arg1);
    void on_comboLiftMode_currentIndexChanged(int index);
    void on_checkLiftAlpha_toggled(bool checked);
    void on_checkLiftLocked_toggled(bool checked);
    void on_checkLiftHaltable_toggled(bool checked);
    void on_checkLiftContinuous_toggled(bool checked);
    void on_checkLiftReturn_toggled(bool checked);
    void on_comboLiftEasing_currentIndexChanged(int index);
    void on_comboLiftTexture_currentIndexChanged(int index);
    void on_spinLiftTexture_valueChanged(int arg1);
    void on_spinLiftScaleX_valueChanged(double arg1);
    void on_spinLiftScaleY_valueChanged(double arg1);
    void on_pushLiftStart_clicked();
    void on_pushLiftStop_clicked();
    void on_pushLiftDelete_clicked();
    void on_pushNodeAddLift_clicked();

    void on_plainSpriteName_textChanged();
    void on_comboSpriteTag_currentIndexChanged(int index);
    void on_spinSpriteWidth_valueChanged(double arg1);
    void on_spinSpriteHeight_valueChanged(double arg1);
    void on_spinSpritePan_valueChanged(double arg1);
    void on_checkSpriteInvisible_toggled(bool checked);
    void on_checkSpriteBackculled_toggled(bool checked);
    void on_checkSpriteShadows_toggled(bool checked);
    void on_checkSpriteAutopan_toggled(bool checked);
    void on_spinSpriteTexture_valueChanged(int arg1);
    void on_pushSpriteDelete_clicked();

    void on_spinStaircasePan_valueChanged(double arg1);
    void on_spinStaircaseHeight_valueChanged(double arg1);
    void on_spinStaircaseWidth_valueChanged(double arg1);
    void on_spinStaircaseLength_valueChanged(double arg1);
    void on_spinStaircaseSteps_valueChanged(int arg1);
    void on_comboStaircaseTexture_currentIndexChanged(int index);
    void on_spinStaircaseTexture_valueChanged(int arg1);
    void on_spinStaircaseScaleX_valueChanged(double arg1);
    void on_spinStaircaseScaleY_valueChanged(double arg1);
    void on_pushStaircaseDelete_clicked();

    void on_pushLightColorA_clicked();
    void on_pushLightColorB_clicked();
    void on_scrollLightStrength_valueChanged(int value);
    void on_comboLightAnimation_currentIndexChanged(int index);
    void on_scrollLightSpeed_valueChanged(int value);
    void on_checkLightEnable_toggled(bool checked);
    void on_comboLightTag_currentIndexChanged(int index);
    void on_pushLightDelete_clicked();

    void on_pushTagNew_clicked();
    void on_pushTagDelete_clicked();
    void on_comboTagsList_currentIndexChanged(int index);
    void on_comboTagsList_lineEdit_returnPressed();
    void on_plainTagName_textChanged();
    void on_plainTagValue_textChanged();
    void on_pushTagsLoad_clicked();
    void on_pushTagsSave_clicked();
    void on_pushTagsClear_clicked();

    void on_plainSpeakerName_textChanged();
    void on_comboSpeakerTag_currentIndexChanged(int index);
    void on_spinSpeakerVolume_valueChanged(double arg1);
    void on_spinSpeakerSize_valueChanged(double arg1);
    void on_spinSpeakerPan_valueChanged(double arg1);
    void on_checkSpeakerAuto_toggled(bool checked);
    void on_checkSpeakerTrigger_toggled(bool checked);
    void on_checkSpeakerToggle_toggled(bool checked);
    void on_checkSpeakerLoop_toggled(bool checked);
    void on_checkSpeakerOmni_toggled(bool checked);
    void on_pushSpeakerBrowse_clicked();
    void on_pushSpeakerDelete_clicked();

    void on_comboPathsList_currentIndexChanged(int index);
    void on_comboPathsList_lineEdit_returnPressed();
    void on_plainPathName_textChanged();
    void on_comboPathTag_currentIndexChanged(int index);
    void on_pushPathNew_clicked();
    void on_pushPathDelete_clicked();
    void on_pushPathNodeUp_clicked();
    void on_pushPathNodeDown_clicked();
    void on_pushPathNodesAdd_clicked();
    void on_pushPathNodesClear_clicked();
    void on_listPathNodes_itemSelectionChanged();

    void on_spinViewerX_valueChanged(double arg1);
    void on_spinViewerY_valueChanged(double arg1);
    void on_spinViewerZ_valueChanged(double arg1);
    void on_spinViewerPan_valueChanged(double arg1);

    void on_pushViewerTop_toggled(bool checked);
    void on_pushViewerFront_toggled(bool checked);
    void on_pushViewerSide_toggled(bool checked);
    void on_pushViewer3D_toggled(bool checked);

    void on_spinViewerMinY_valueChanged(double arg1);
    void on_spinViewerMaxY_valueChanged(double arg1);

    void on_pushSunAmbient_clicked();
    void on_pushSunRay_clicked();
    void on_spinSunHour_valueChanged(double arg1);
    void on_spinSunAngle_valueChanged(double arg1);
    void on_scrollSunAmbient_valueChanged(int value);
    void on_scrollSunRay_valueChanged(int value);

    void on_pushFogColor_clicked();
    void on_spinFogNear_valueChanged(double arg1);
    void on_spinFogFar_valueChanged(double arg1);
    void on_checkFogEnable_toggled(bool checked);

    void on_spinFOVAngle_valueChanged(double arg1);
    void on_spinFOVNear_valueChanged(double arg1);
    void on_spinFOVFar_valueChanged(double arg1);

    void on_checkEditorGravity_toggled(bool checked);
    void on_checkEditorCollisions_toggled(bool checked);
    void on_checkEditorWallSelector_toggled(bool checked);

    void on_checkRendererMultithreadingEnable_toggled(bool checked);
    void on_checkRendererWallsEnable_toggled(bool checked);
    void on_checkRendererSurfacesEnable_toggled(bool checked);
    void on_checkRendererOcclusionEnable_toggled(bool checked);
    void on_checkRendererLightsEnable_toggled(bool checked);
    void on_checkRendererMotionblur_toggled(bool checked);
    void on_checkRendererVignetting_toggled(bool checked);
    void on_checkRendererAlphaFeatures_toggled(bool checked);
    void on_checkRendererGamma_toggled(bool checked);
    void on_spinRendererOcclusionLength_valueChanged(double arg1);
    void on_scrollRendererOcclusionDarken_valueChanged(int value);
    void on_scrollRendererMotionblurPercent_valueChanged(int value);
    void on_scrollRendererVignettingInner_valueChanged(int value);
    void on_scrollRendererVignettingOuter_valueChanged(int value);
    void on_scrollRendererGammaKRed_valueChanged(int value);
    void on_scrollRendererGammaKGreen_valueChanged(int value);
    void on_scrollRendererGammaKBlue_valueChanged(int value);

    void on_actionNew_triggered();
    void on_actionLoad_map_triggered();
    void on_actionSave_map_triggered();
    void on_actionSave_submap_triggered();
    void on_actionQuit_triggered();

    void on_pushTextureBrowse_clicked();
    void on_pushMapCenter_clicked();
    void on_scrollMapH_sliderMoved(int position);
    void on_scrollMapV_sliderMoved(int position);

    void on_pushTextureSlice_clicked();
    void on_pushTextureConcat_clicked();

    void on_actionSelectAll_triggered();
    void on_actionDeselect_triggered();
    void on_actionCut_triggered();
    void on_actionCopy_triggered();
    void on_actionPaste_triggered();
    void on_actionDelete_triggered();
    void on_actionAlign_triggered();

    void on_actionUndo_triggered();
    void on_actionRedo_triggered();

    void on_actionAbout_triggered();
    void on_actionAboutQt_triggered();

    void on_pushViewportReset_clicked();
    void on_comboGlowmapArea_currentIndexChanged(int index);
    void on_comboGlowmapSize_currentIndexChanged(int index);
    void on_spinEditorFloor_valueChanged(double arg1);
    void on_pushRendererGlowmapRebuild_clicked();

private:
    static constexpr int UNDO_MAX = 50;
    QList<MapState> undoHistory;
    int undoIndex;
    QTimer * undoDebounceTimer;

    void applyUndoState();
    void updateUndoActions();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
