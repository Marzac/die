/**
    WALLER
    Map editor for the DIE engine
    (c) Fred's Lab 2024-2026
    Frédéric Meslin / info@fredslab.net
    SPDX-License-Identifier: MIT
    If used commercially, contributions, donations are highly appreciated.

    editor main window
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "renderer.h"
#include "audio.h"
#include "editor.h"
#include "walker.h"
#include "tags.h"

#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QShortcut>
#include <QFileDialog>
#include <QColorDialog>
#include <QStandardPaths>
#include <QMessageBox>

#include <stdint.h>
#include <math.h>

/*****************************************************************************/
static constexpr int glowmapSizes[] = {64, 128, 256, 512, 1024, 2048, 4096};
static constexpr int glowmapAreas[] = {256, 512, 1024, 2048, 4096, 8192, 16384};
static constexpr int glowmapCount = 7;

/*****************************************************************************/
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    uiEditMode(EDIT_MODE_NODES), once(true),
    renderTimer(nullptr), uiTimer(nullptr),
    renderWindow(nullptr),
    undoIndex(-1), undoDebounceTimer(nullptr),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    static const QStringList easingNames = {
        "Linear",
        "In Sine",     "Out Sine",     "In-Out Sine",
        "In Quad",     "Out Quad",     "In-Out Quad",
        "In Cubic",    "Out Cubic",    "In-Out Cubic",
        "In Quart",    "Out Quart",    "In-Out Quart",
        "In Quint",    "Out Quint",    "In-Out Quint",
        "In Expo",     "Out Expo",     "In-Out Expo",
        "In Circ",     "Out Circ",     "In-Out Circ",
        "In Back",     "Out Back",     "In-Out Back",
        "In Elastic",  "Out Elastic",  "In-Out Elastic",
        "In Bounce",   "Out Bounce",   "In-Out Bounce",
    };
    ui->comboDoorEasing->addItems(easingNames);
    ui->comboLiftEasing->addItems(easingNames);

    connect(ui->comboPathsList->lineEdit(), &QLineEdit::returnPressed, this, &MainWindow::on_comboPathsList_lineEdit_returnPressed);
    connect(ui->comboTagsList->lineEdit(), &QLineEdit::returnPressed, this, &MainWindow::on_comboTagsList_lineEdit_returnPressed);

    createRenderWindow();

    renderTimer = new QTimer(this);
    connect(renderTimer, SIGNAL(timeout()), this, SLOT(timerRender_tick()));
    renderTimer->start(16);

    uiTimer = new QTimer(this);
    connect(uiTimer, SIGNAL(timeout()), this, SLOT(timerUI_tick()));
    uiTimer->start(32);

    createContextMenu();
    createShortcuts();

    undoDebounceTimer = new QTimer(this);
    undoDebounceTimer->setSingleShot(true);
    undoDebounceTimer->setInterval(300);
    connect(undoDebounceTimer, &QTimer::timeout, this, &MainWindow::pushUndoState);

    editor.gridSnap = true;
    editor.gridSize = 8;
    ui->checkEditorSnap->setChecked(true);

    setEditMode(EDIT_MODE_NODES);
    viewEnableTop = true;
    viewEnableSide = true;
    viewEnableFront = true;
    viewEnable3D = true;
    updateViewMode();

    updateSunProperties();
    updateFogProperties();
    updateEngineProperties();
    updateEditorProperties();

    if (renderWindow)
        renderWindow->setFocus();
}

MainWindow::~MainWindow()
{
    uiTimer->stop();
    renderTimer->stop();

    delete ui;
}

/*****************************************************************************/
void MainWindow::createRenderWindow()
{
    int screens = QGuiApplication::screens().count();
    if (screens == 1) {
        if (renderWindow) renderWindow->close();
        renderWindow = nullptr;
        return;
    }

    if (renderWindow) {
        renderWindow->show();
        return;
    }

    renderWindow = new RenderWindow();
    QRect rect = QGuiApplication::screens()[1]->geometry();
    renderWindow->move(rect.x(), rect.y());

    renderWindow->showMaximized();
    renderWindow->activateWindow();
}

/*****************************************************************************/
void MainWindow::createShortcuts()
{
    shortcutEditNodes = new QShortcut(QKeySequence(Qt::Key_N), this);
    connect(shortcutEditNodes, SIGNAL(activated()), this, SLOT(on_setNodeMode()));

    shortcutEditWalls = new QShortcut(QKeySequence(Qt::Key_W), this);
    connect(shortcutEditWalls, SIGNAL(activated()), this, SLOT(on_setWallMode()));

    shortcutEditSubmaps = new QShortcut(QKeySequence(Qt::Key_M), this);
    connect(shortcutEditSubmaps, SIGNAL(activated()), this, SLOT(on_setSubmapMode()));

    shortcutEditDoors = new QShortcut(QKeySequence(Qt::Key_D), this);
    connect(shortcutEditDoors, SIGNAL(activated()), this, SLOT(on_setDoorMode()));

    shortcutEditLifts = new QShortcut(QKeySequence(Qt::Key_E), this);
    connect(shortcutEditLifts, SIGNAL(activated()), this, SLOT(on_setLiftMode()));

    shortcutEditSprites = new QShortcut(QKeySequence(Qt::Key_B), this);
    connect(shortcutEditSprites, SIGNAL(activated()), this, SLOT(on_setSpriteMode()));

    shortcutEditStaircases = new QShortcut(QKeySequence(Qt::Key_H), this);
    connect(shortcutEditStaircases, SIGNAL(activated()), this, SLOT(on_setStaircaseMode()));

    shortcutEditLights = new QShortcut(QKeySequence(Qt::Key_L), this);
    connect(shortcutEditLights, SIGNAL(activated()), this, SLOT(on_setLightMode()));

    shortcutEditSpeakers = new QShortcut(QKeySequence(Qt::Key_S), this);
    connect(shortcutEditSpeakers, SIGNAL(activated()), this, SLOT(on_setSpeakerMode()));

    shortcutEditDelete = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    connect(shortcutEditDelete, SIGNAL(activated()), this, SLOT(on_deleteCurrent()));
}

void MainWindow::createContextMenu()
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenu_show(QPoint)));
}

/*****************************************************************************/
void MainWindow::pushUndoState()
{
    MapState current = editor.editedMap->captureState();
    if (undoIndex >= 0 && current == undoHistory[undoIndex])
        return;

    while (undoHistory.size() > undoIndex + 1)
        undoHistory.removeLast();

    undoHistory.append(current);
    undoIndex = undoHistory.size() - 1;

    if (undoHistory.size() > UNDO_MAX) {
        undoHistory.removeFirst();
        undoIndex--;
    }
    updateUndoActions();
}

void MainWindow::scheduleUndoPush()
{
    undoDebounceTimer->start();
}

/*****************************************************************************/
void MainWindow::undo()
{
    undoDebounceTimer->stop();
    if (undoIndex <= 0) return;
    undoIndex--;
    applyUndoState();
}

void MainWindow::redo()
{
    undoDebounceTimer->stop();
    if (undoIndex >= undoHistory.size() - 1) return;
    undoIndex++;
    applyUndoState();
}

void MainWindow::applyUndoState()
{
    editor.editedMap->restoreState(undoHistory[undoIndex]);
    editor.editedMap->computeAllWalls();
    editor.clearSelection();

    updateAllProperties();
    updateSunProperties();
    updateFogProperties();
    updateUndoActions();
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
}

void MainWindow::updateUndoActions()
{
    ui->actionUndo->setEnabled(undoIndex > 0);
    ui->actionRedo->setEnabled(undoIndex < undoHistory.size() - 1);
}

void MainWindow::on_actionUndo_triggered() { undo(); }
void MainWindow::on_actionRedo_triggered() { redo(); }

/*****************************************************************************/
void MainWindow::on_deleteCurrent()
{
    switch(editor.editMode) {
        case EDIT_MODE_NODES:
        case EDIT_MODE_PATHS:
            on_pushNodeDelete_clicked();
            break;
        case EDIT_MODE_WALLS: on_pushWallDelete_clicked(); break;
        case EDIT_MODE_SUBMAPS: on_pushSubmapDelete_clicked(); break;
        case EDIT_MODE_DOORS: on_pushDoorDelete_clicked(); break;
        case EDIT_MODE_LIFTS: on_pushLiftDelete_clicked(); break;
        case EDIT_MODE_SPRITES: on_pushSpriteDelete_clicked(); break;
        case EDIT_MODE_STAIRCASES: on_pushStaircaseDelete_clicked(); break;
        case EDIT_MODE_LIGHTS: on_pushLightDelete_clicked(); break;
        case EDIT_MODE_SPEAKERS: on_pushSpeakerDelete_clicked(); break;
        default: break;
    }
}

void MainWindow::on_setNodeMode() {setEditMode(EDIT_MODE_NODES);}
void MainWindow::on_setWallMode() {setEditMode(EDIT_MODE_WALLS);}
void MainWindow::on_setSubmapMode() {setEditMode(EDIT_MODE_SUBMAPS);}
void MainWindow::on_setDoorMode() {setEditMode(EDIT_MODE_DOORS);}
void MainWindow::on_setLiftMode() {setEditMode(EDIT_MODE_LIFTS);}
void MainWindow::on_setSpriteMode() {setEditMode(EDIT_MODE_SPRITES);};
void MainWindow::on_setStaircaseMode() {setEditMode(EDIT_MODE_STAIRCASES);};
void MainWindow::on_setLightMode() {setEditMode(EDIT_MODE_LIGHTS);};
void MainWindow::on_setSpeakerMode() {setEditMode(EDIT_MODE_SPEAKERS);};

/*****************************************************************************/
void MainWindow::contextMenu_show(const QPoint &pos)
{
    QMenu contextMenu(this);

    contextMenu.addAction(ui->actionUndo);
    contextMenu.addAction(ui->actionRedo);
    contextMenu.addSeparator();
    QAction * selectAllAction = contextMenu.addAction("Select All");
    QAction * deselectAction = contextMenu.addAction("Deselect");
    contextMenu.addSeparator();
    QAction * cutAction = contextMenu.addAction("Cut");
    QAction * copyAction = contextMenu.addAction("Copy");
    QAction * pasteAction = contextMenu.addAction("Paste");
    QAction * deleteAction = contextMenu.addAction("Delete");
    contextMenu.addSeparator();
    QAction * alignAction = contextMenu.addAction("Align");

    QAction * selectedAction = contextMenu.exec(mapToGlobal(pos));
    if (!selectedAction) return;

    if (selectedAction == selectAllAction)
        editor.selectAll();
    else if (selectedAction == deselectAction)
        editor.deselect();
    else if (selectedAction == cutAction)
        editor.cut();
    else if (selectedAction == copyAction)
        editor.copy();
    else if (selectedAction == pasteAction)
        editor.paste(VIEW_TOP, QVector2D(pos.x(), pos.y()));
    else if (selectedAction == deleteAction)
        on_deleteCurrent();
    else if (selectedAction == alignAction)
        editor.align();
}

/*****************************************************************************/
void MainWindow::timerRender_tick()
{
    walker.update();
    editor.editedMap->update();
    editor.env.pass();
    editor.editedMap->pass(editor.viewPoint.pos);
    renderer.render(editor.viewPoint);
    ui->widgetMap4->repaint();
    if (renderWindow) renderWindow->repaint();
}

void MainWindow::timerUI_tick()
{
    if (once) {
        if (renderWindow) renderWindow->activateWindow();
        once = false;
    }

    if (editor.editMode != uiEditMode)
        setEditMode(editor.editMode);

    ui->widgetMap1->repaint();
    ui->widgetMap2->repaint();
    ui->widgetMap3->repaint();

    ui->scrollMapH->blockSignals(true);
    ui->scrollMapH->setValue(ui->widgetMap1->getOffsetX());
    ui->scrollMapH->blockSignals(false);

    ui->scrollMapV->blockSignals(true);
    ui->scrollMapV->setValue(ui->widgetMap1->getOffsetZ());
    ui->scrollMapV->blockSignals(false);

    ui->scrollTextures->blockSignals(true);
    ui->scrollTextures->setValue(ui->widgetTextureSelector->getScroll());
    ui->scrollTextures->blockSignals(false);

    updateViewpointProperties();

    ui->spinViewerMinY->blockSignals(true);
    ui->spinViewerMinY->setValue(editor.viewMinY);
    ui->spinViewerMinY->blockSignals(false);

    ui->spinViewerMaxY->blockSignals(true);
    ui->spinViewerMaxY->setValue(editor.viewMaxY);
    ui->spinViewerMaxY->blockSignals(false);

    QString stats("N:%1, W:%2, M%3, D%4, B%5, A%6, Floor:%7, Ceil:%8");
    stats = stats.arg(editor.editedMap->nodes.count());
    stats = stats.arg(editor.editedMap->walls.count());
    stats = stats.arg(editor.editedMap->submaps.count());
    stats = stats.arg(editor.editedMap->doors.count());
    stats = stats.arg(editor.editedMap->sprites.count());
    stats = stats.arg(editor.editedMap->speakers.count());

    stats = stats.arg(renderer.lastFloor);
    stats = stats.arg(renderer.lastCeiling);
    ui->labelStatistics->setText(stats);
}

/*****************************************************************************/
void MainWindow::on_checkEditorSnap_toggled(bool checked)
{
    editor.gridSnap = checked;
}

void MainWindow::on_comboEditorSnap_currentIndexChanged(int index)
{
    const float sizes[] = {64.0f, 32.0f, 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f};
    editor.gridSize = sizes[index];
}

/*****************************************************************************/
void MainWindow::setEditMode(EDIT_MODES mode)
{
    uiEditMode = mode;
    editor.editMode = mode;

    ui->tabEditorModes->setCurrentIndex(mode);

    editor.clearSelection();
    updateAllProperties();
}

void MainWindow::updateAllProperties()
{
    updateNodeProperties();
    updateWallProperties();
    updateSubmapProperties();
    updateDoorProperties();
    updateLiftProperties();
    updateSpriteProperties();
    updateStaircaseProperties();
    updateLightProperties();
    updateSpeakerProperties();
    updatePathProperties();
}

void MainWindow::updateViewMode()
{
    if (viewEnableTop) ui->pushViewerTop->setChecked(true);
    if (viewEnableFront) ui->pushViewerFront->setChecked(true);
    if (viewEnableSide) ui->pushViewerSide->setChecked(true);
    if (viewEnable3D) ui->pushViewer3D->setChecked(true);
    resizeUI();
}

/*****************************************************************************/
void MainWindow::setTexture(uint16_t texId)
{
    switch(editor.editMode) {
    case EDIT_MODE_WALLS: ui->spinWallTexture->setValue(texId); break;
    case EDIT_MODE_DOORS: ui->spinDoorTexture->setValue(texId); break;
    case EDIT_MODE_LIFTS: ui->spinLiftTexture->setValue(texId); break;
    case EDIT_MODE_SPRITES: ui->spinSpriteTexture->setValue(texId); break;
    case EDIT_MODE_STAIRCASES: ui->spinStaircaseTexture->setValue(texId); break;
    default: break;
    }
}

/*****************************************************************************/
void MainWindow::setSpinValueSilently(QAbstractSpinBox* box, double value)
{
    if (auto* doubleSpin = qobject_cast<QDoubleSpinBox*>(box)) {
        doubleSpin->blockSignals(true);
        doubleSpin->setValue(value);
        doubleSpin->blockSignals(false);

    } else if (auto* intSpin = qobject_cast<QSpinBox*>(box)) {
        intSpin->blockSignals(true);
        intSpin->setValue(static_cast<int>(value));
        intSpin->blockSignals(false);
    }
}
void MainWindow::setCheckboxStateSilently(QCheckBox* box, bool checked)
{
    box->blockSignals(true);
    box->setChecked(checked);
    box->blockSignals(false);
}

/*****************************************************************************/
void MainWindow::updateNodeProperties()
{
    if (ui->listNodes->count() != editor.editedMap->nodes.count()) {
        ui->listNodes->blockSignals(true);
        ui->listNodes->clear();
        for (int i = 0; i < editor.editedMap->nodes.count(); i++)
            ui->listNodes->addItem(QString::asprintf("Node %04X", (unsigned int)i));
        ui->listNodes->blockSignals(false);
    }

    ui->listNodes->blockSignals(true);
    for (int i = 0; i < editor.editedMap->nodes.count(); i++)
        ui->listNodes->item(i)->setSelected(editor.editedMap->nodes[i].selected);
    ui->listNodes->blockSignals(false);

    if (editor.selectedNode < 0) {
        ui->plainNodeID->setPlainText("None");
        setSpinValueSilently(ui->spinNodeX, 0.0);
        setSpinValueSilently(ui->spinNodeY, 0.0);
        setSpinValueSilently(ui->spinNodeZ, 0.0);
        setSpinValueSilently(ui->spinNodeMetaA, 0.0);
        setSpinValueSilently(ui->spinNodeMetaB, 0.0);
        setSpinValueSilently(ui->spinNodeMetaC, 0.0);
        ui->comboNodeTag->setCurrentIndex(0);
        return;
    }

    Node & n = editor.editedMap->nodes[editor.selectedNode];
    ui->plainNodeID->setPlainText(QString::number(editor.selectedNode));
    setSpinValueSilently(ui->spinNodeX, n.pos.x());
    setSpinValueSilently(ui->spinNodeY, n.pos.y());
    setSpinValueSilently(ui->spinNodeZ, n.pos.z());
    setSpinValueSilently(ui->spinNodeMetaA, n.metaA);
    setSpinValueSilently(ui->spinNodeMetaB, n.metaB);
    setSpinValueSilently(ui->spinNodeMetaC, n.metaC);
    ui->comboNodeTag->setCurrentIndex(n.tag);
}

void MainWindow::on_listNodes_itemSelectionChanged()
{
    editor.nodeDeselectAll();
    editor.selectedNode = EDIT_UNSELECTED;

    for (int row = 0; row < ui->listNodes->count(); row++) {
        if (!ui->listNodes->item(row)->isSelected()) continue;
        if (row >= editor.editedMap->nodes.count()) continue;
        editor.editedMap->nodes[row].selected = true;
        editor.selectedNode = row;
    }

    int current = ui->listNodes->currentRow();
    if (current >= 0 && current < editor.editedMap->nodes.count() &&
        ui->listNodes->item(current)->isSelected())
        editor.selectedNode = current;

    updateNodeProperties();
}

void MainWindow::updateWallProperties()
{
    if (editor.selectedWall < 0) {
        ui->plainWallID->setPlainText("None");

        setSpinValueSilently(ui->spinWallHeight, 0.0);
        setCheckboxStateSilently(ui->checkWallInvisible, false);
        setCheckboxStateSilently(ui->checkWallBackculled, false);
        setCheckboxStateSilently(ui->checkWallAlpha, false);
        setCheckboxStateSilently(ui->checkWallCeilingFront, false);
        setCheckboxStateSilently(ui->checkWallCeilingBack, false);
        setCheckboxStateSilently(ui->checkWallFloorFront, false);
        setCheckboxStateSilently(ui->checkWallFloorBack, false);
        setSpinValueSilently(ui->spinWallTexture, 0);
        ui->widgetWallTexture->setID(0);
        setSpinValueSilently(ui->spinWallScaleX, 0.0);
        setSpinValueSilently(ui->spinWallScaleY, 0.0);
        setSpinValueSilently(ui->spinWallShiftX, 0.0);
        setSpinValueSilently(ui->spinWallShiftY, 0.0);
        return;
    }

    Wall& w = editor.editedMap->walls[editor.selectedWall];
    ui->plainWallID->setPlainText(QString::number(editor.selectedWall));

    setSpinValueSilently(ui->spinWallHeight, w.height);
    setCheckboxStateSilently(ui->checkWallInvisible, w.flags & WALL_FLAG_INVISIBLE);
    setCheckboxStateSilently(ui->checkWallBackculled, w.flags & WALL_FLAG_BACKCULLED);
    setCheckboxStateSilently(ui->checkWallAlpha, w.flags & WALL_FLAG_ALPHA);
    setCheckboxStateSilently(ui->checkWallCeilingFront, w.flags & WALL_FLAG_CEILING_FRONT);
    setCheckboxStateSilently(ui->checkWallCeilingBack, w.flags & WALL_FLAG_CEILING_BACK);
    setCheckboxStateSilently(ui->checkWallFloorFront, w.flags & WALL_FLAG_FLOOR_FRONT);
    setCheckboxStateSilently(ui->checkWallFloorBack, w.flags & WALL_FLAG_FLOOR_BACK);

    updateWallTexture();
}
void MainWindow::updateWallTexture()
{
    if (editor.selectedWall < 0) return;
    Wall & w = editor.editedMap->walls[editor.selectedWall];

    int type = ui->comboWallTexture->currentIndex();
    Surface & s = w.surfaces[type];

    setSpinValueSilently(ui->spinWallTexture, s.id);
    ui->widgetWallTexture->setID(s.id);
    setSpinValueSilently(ui->spinWallScaleX, s.scaleX);
    setSpinValueSilently(ui->spinWallScaleY, s.scaleY);
    setSpinValueSilently(ui->spinWallShiftX, s.shiftX);
    setSpinValueSilently(ui->spinWallShiftY, s.shiftY);
}

void MainWindow::updateSubmapProperties()
{
    if (editor.selectedSubmap < 0) {
        ui->plainSubmapID->setPlainText("None");
        ui->plainSubmapName->setPlainText("");
        ui->comboSubmapTag->setCurrentIndex(0);
        ui->plainSubmapFilepath->setPlainText("");
        setSpinValueSilently(ui->spinSubmapPan, 0.0);
        setSpinValueSilently(ui->spinSubmapScale, 0.0);
        return;
    }

    Submap & m = editor.editedMap->submaps[editor.selectedSubmap];

    ui->plainSubmapID->setPlainText(QString::number(editor.selectedSubmap));
    ui->plainSubmapName->setPlainText(QString::fromUtf8(m.name));
    ui->comboSubmapTag->setCurrentIndex(m.tag);
    QString path = QString::fromUtf8(m.path);
    ui->plainSubmapFilepath->setPlainText(editor.editedMap->getRelativePath(path));
    setSpinValueSilently(ui->spinSubmapPan, m.pan);
    setSpinValueSilently(ui->spinSubmapScale, m.scale);
}

void MainWindow::updateDoorProperties()
{
    if (editor.selectedDoor < 0) {
        ui->plainDoorID->setPlainText("None");
        ui->plainDoorName->setPlainText("");
        ui->comboDoorTag->setCurrentIndex(0);
        ui->spinDoorWidth->setValue(0.0);
        ui->spinDoorHeight->setValue(0.0);
        ui->spinDoorThick->setValue(0.0);
        ui->comboDoorMode->setCurrentIndex(0);
        ui->comboDoorEasing->setCurrentIndex(0);
        ui->spinDoorAngle->setValue(0.0);
        ui->spinDoorSwing->setValue(0.0);
        ui->spinDoorTime->setValue(0.0);
        ui->checkDoorAlpha->setChecked(false);
        ui->checkDoorLocked->setChecked(false);
        return;
    }

    Door & d = editor.editedMap->doors[editor.selectedDoor];

    ui->plainDoorID->setPlainText(QString::number(editor.selectedDoor));
    ui->plainDoorName->setPlainText(QString::fromUtf8(d.name));
    ui->comboDoorTag->setCurrentIndex(d.tag);
    ui->spinDoorWidth->setValue(d.width);
    ui->spinDoorHeight->setValue(d.height);
    ui->spinDoorThick->setValue(d.thick);
    ui->comboDoorMode->setCurrentIndex(d.mode);
    ui->comboDoorEasing->setCurrentIndex(d.easing);
    ui->spinDoorAngle->setValue(d.angle);
    ui->spinDoorSwing->setValue(d.swing);
    ui->spinDoorTime->setValue(d.time);
    ui->checkDoorAlpha->setChecked(d.flags & DOOR_FLAG_ALPHA);
    ui->checkDoorLocked->setChecked(d.flags & DOOR_FLAG_LOCKED);

    updateDoorTexture();
}

void MainWindow::updateDoorTexture()
{
    if (editor.selectedDoor < 0) return;
    Door & d = editor.editedMap->doors[editor.selectedDoor];

    int type = ui->comboDoorTexture->currentIndex();
    Surface & s = d.surfaces[type];

    ui->spinDoorTexture->setValue(s.id);
    ui->widgetDoorTexture->setID(s.id);
    ui->spinDoorScaleX->setValue(s.scaleX);
    ui->spinDoorScaleY->setValue(s.scaleY);
}

void MainWindow::updateLiftProperties()
{
    if (editor.selectedLift < 0) {
        ui->plainLiftD->setPlainText("None");
        ui->plainLiftName->setPlainText("");
        ui->comboLiftTag->setCurrentIndex(0);
        setSpinValueSilently(ui->spinLiftWidth, 0.0);
        setSpinValueSilently(ui->spinLiftLength, 0.0);
        setSpinValueSilently(ui->spinLiftThick, 0.0);
        setSpinValueSilently(ui->spinLiftTravel, 0.0);
        setSpinValueSilently(ui->spinLiftTime, 0.0);
        ui->comboLiftMode->setCurrentIndex(0);
        ui->comboLiftEasing->setCurrentIndex(0);
        setCheckboxStateSilently(ui->checkLiftAlpha, false);
        setCheckboxStateSilently(ui->checkLiftLocked, false);
        setCheckboxStateSilently(ui->checkLiftHaltable, false);
        setCheckboxStateSilently(ui->checkLiftContinuous, false);
        setCheckboxStateSilently(ui->checkLiftReturn, false);
        return;
    }

    Lift & e = editor.editedMap->lifts[editor.selectedLift];
    ui->plainLiftD->setPlainText(QString::number(editor.selectedLift));
    ui->plainLiftName->setPlainText(QString::fromUtf8(e.name));
    ui->comboLiftTag->setCurrentIndex(e.tag);
    setSpinValueSilently(ui->spinLiftWidth, e.width);
    setSpinValueSilently(ui->spinLiftLength, e.length);
    setSpinValueSilently(ui->spinLiftThick, e.thick);
    setSpinValueSilently(ui->spinLiftTravel, e.travel);
    setSpinValueSilently(ui->spinLiftTime, e.time);
    ui->comboLiftMode->setCurrentIndex(e.mode);
    ui->comboLiftEasing->setCurrentIndex(e.easing);
    setCheckboxStateSilently(ui->checkLiftAlpha, e.flags & LIFT_FLAG_ALPHA);
    setCheckboxStateSilently(ui->checkLiftLocked, e.flags & LIFT_FLAG_LOCKED);
    setCheckboxStateSilently(ui->checkLiftHaltable, e.flags & LIFT_FLAG_HALTABLE);
    setCheckboxStateSilently(ui->checkLiftContinuous, e.flags & LIFT_FLAG_CONTINUOUS);
    setCheckboxStateSilently(ui->checkLiftReturn, e.flags & LIFT_FLAG_RETURN);

    updateLiftTexture();
}

void MainWindow::updateLiftTexture()
{
    if (editor.selectedLift < 0) return;
    Lift & e = editor.editedMap->lifts[editor.selectedLift];

    int type = ui->comboLiftTexture->currentIndex();
    Surface & s = e.surfaces[type];

    ui->spinLiftTexture->setValue(s.id);
    ui->widgetLiftTexture->setID(s.id);
    ui->spinLiftScaleX->setValue(s.scaleX);
    ui->spinLiftScaleY->setValue(s.scaleY);
}

void MainWindow::updateSpriteProperties()
{
    if (editor.selectedSprite < 0) {
        ui->plainSpriteID->setPlainText("None");
        ui->plainSpriteName->setPlainText("");
        ui->comboSpriteTag->setCurrentIndex(0);
        ui->spinSpriteWidth->setValue(0.0);
        ui->spinSpriteHeight->setValue(0.0);
        ui->spinSpritePan->setValue(0.0);
        ui->checkSpriteInvisible->setChecked(false);
        ui->checkSpriteBackculled->setChecked(false);
        ui->checkSpriteAutopan->setChecked(false);
        ui->checkSpriteShadows->setChecked(false);
        ui->widgetSpriteTexture->setID(0);
        return;
    }

    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];

    ui->plainSpriteID->setPlainText(QString::number(editor.selectedSprite));
    ui->plainSpriteName->setPlainText(QString::fromUtf8(b.name));
    ui->comboSpriteTag->setCurrentIndex(b.tag);
    ui->spinSpriteWidth->setValue(b.width);
    ui->spinSpriteHeight->setValue(b.height);
    ui->spinSpritePan->setValue(b.pan);
    ui->checkSpriteInvisible->setChecked(b.flags & SPRITE_FLAG_INVISIBLE);
    ui->checkSpriteBackculled->setChecked(b.flags & SPRITE_FLAG_BACKCULLED);
    ui->checkSpriteAutopan->setChecked(b.flags & SPRITE_FLAG_AUTOPAN);
    ui->checkSpriteShadows->setChecked(b.flags & SPRITE_FLAG_SHADOWS);

    updateSpriteTexture();
}

void MainWindow::updateSpriteTexture()
{
    if (editor.selectedSprite < 0) return;
    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];
    Surface & s = b.surface;

    ui->spinSpriteTexture->setValue(s.id);
    ui->widgetSpriteTexture->setID(s.id);
}

void MainWindow::updateStaircaseProperties()
{
    if (editor.selectedStaircase < 0) {
        ui->plainStaircaseID->setPlainText("None");
        ui->spinStaircasePan->setValue(0.0);
        ui->spinStaircaseHeight->setValue(0.0);
        ui->spinStaircaseWidth->setValue(0.0);
        ui->spinStaircaseLength->setValue(0.0);
        ui->spinStaircaseSteps->setValue(0);
        return;
    }

    Staircase & h = editor.editedMap->staircases[editor.selectedStaircase];

    ui->plainStaircaseID->setPlainText(QString::number(editor.selectedStaircase));

    ui->spinStaircasePan->setValue(h.pan);
    ui->spinStaircaseHeight->setValue(h.height);
    ui->spinStaircaseWidth->setValue(h.width);
    ui->spinStaircaseLength->setValue(h.length);
    ui->spinStaircaseSteps->setValue(h.steps);
    updateStaircaseTexture();
}

void MainWindow::updateStaircaseTexture()
{
    if (editor.selectedStaircase < 0) return;
    Staircase & h = editor.editedMap->staircases[editor.selectedStaircase];

    int type = ui->comboStaircaseTexture->currentIndex();
    Surface & s = h.surfaces[type];

    ui->spinStaircaseTexture->setValue(s.id);
    ui->widgetStaircaseTexture->setID(s.id);
    ui->spinStaircaseScaleX->setValue(s.scaleX);
    ui->spinStaircaseScaleY->setValue(s.scaleY);
}

void MainWindow::updateLightProperties()
{
    if (editor.selectedLight < 0) {
        ui->plainLightID->setPlainText("None");
        ui->scrollLightStrength->setValue(0);
        ui->spinLightFalloff->setValue(0);
        ui->scrollLightSpeed->setValue(0);
        ui->pushLightColorA->setStyleSheet("background-color: #000");
        ui->pushLightColorB->setStyleSheet("background-color: #000");
        ui->comboLightAnimation->setCurrentIndex(0);
        ui->checkLightEnable->setChecked(false);
        ui->comboLightTag->setCurrentIndex(0);
        return;
    }

    Light & l = editor.editedMap->lights[editor.selectedLight];

    ui->plainLightID->setPlainText(QString::number(editor.selectedLight));

    ui->scrollLightStrength->setValue((int)(l.strength * 32));
    ui->spinLightFalloff->setValue(l.falloff);
    ui->scrollLightSpeed->setValue((int)(l.speed * 16));
    ui->pushLightColorA->setStyleSheet("background-color: " + QColor(l.colorA).name());
    ui->pushLightColorB->setStyleSheet("background-color: " + QColor(l.colorB).name());
    ui->comboLightAnimation->setCurrentIndex(l.anim);
    ui->checkLightEnable->setChecked(l.flags & LIGHT_FLAG_ENABLE);
    ui->comboLightTag->setCurrentIndex(l.tag);
}

void MainWindow::updateSpeakerProperties()
{
    if (editor.selectedSpeaker < 0) {
        ui->plainSpeakerID->setPlainText("None");
        ui->plainSpeakerName->setPlainText("");
        ui->spinSpeakerVolume->setValue(0.0);
        ui->spinSpeakerSize->setValue(0.0);
        ui->spinSpeakerPan->setValue(0.0);
        ui->comboSpeakerTag->setCurrentIndex(0);
        ui->plainSpeakerFilepath->setPlainText("");
        return;
    }

    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];

    ui->plainSpeakerID->setPlainText(QString::number(editor.selectedSpeaker));
    ui->plainSpeakerName->setPlainText(QString::fromUtf8(a.name));
    ui->spinSpeakerVolume->setValue(a.volume);
    ui->spinSpeakerSize->setValue(a.size);
    ui->spinSpeakerPan->setValue(a.pan);
    ui->checkSpeakerAuto->setChecked(a.flags & SPEAKER_FLAG_AUTOPLAY);
    ui->checkSpeakerTrigger->setChecked(a.flags & SPEAKER_FLAG_TRIGGER);
    ui->checkSpeakerToggle->setChecked(a.flags & SPEAKER_FLAG_TOGGLE);
    ui->checkSpeakerLoop->setChecked(a.flags & SPEAKER_FLAG_LOOP);
    ui->checkSpeakerOmni->setChecked(a.flags & SPEAKER_FLAG_OMNI);
    ui->comboSpeakerTag->setCurrentIndex(a.tag);
    ui->plainSpeakerFilepath->setPlainText(QString::fromUtf8(a.path));
}

/*****************************************************************************/
void MainWindow::updateViewpointProperties()
{
    QVector3D & pos = walker.pos;
    float pan = walker.pan;

    ui->spinViewerX->blockSignals(true);
    ui->spinViewerX->setValue(pos.x());
    ui->spinViewerX->blockSignals(false);

    ui->spinViewerY->blockSignals(true);
    ui->spinViewerY->setValue(pos.y());
    ui->spinViewerY->blockSignals(false);

    ui->spinViewerZ->blockSignals(true);
    ui->spinViewerZ->setValue(pos.z());
    ui->spinViewerZ->blockSignals(false);

    ui->spinViewerPan->blockSignals(true);
    ui->spinViewerPan->setValue(pan);
    ui->spinViewerPan->blockSignals(false);
}

/*****************************************************************************/
void MainWindow::updateSunProperties()
{
    Sun & sun = editor.env.sun;
    ui->pushSunAmbient->setStyleSheet("background-color: " + QColor(sun.ambientColor).name());
    ui->scrollSunAmbient->setValue(sun.ambientStrength * 100.0f);
    ui->pushSunRay->setStyleSheet("background-color: " + QColor(sun.rayColor).name());
    ui->scrollSunRay->setValue(sun.rayStrength * 100.0f);
    ui->spinSunHour->setValue(sun.hour);
    ui->spinSunAngle->setValue(sun.angle);
}

void MainWindow::updateFogProperties()
{
    ui->pushFogColor->setStyleSheet("background-color: " + QColor(editor.env.fog.color).name());
    ui->spinFogNear->setValue(editor.env.fog.distanceNear);
    ui->spinFogFar->setValue(editor.env.fog.distanceFar);
    ui->checkFogEnable->setChecked(editor.env.fog.flags & FOG_FLAG_ENABLE);
}

/*****************************************************************************/
void MainWindow::updateEditorProperties()
{
    setCheckboxStateSilently(ui->checkEditorCollisions, editor.collisions);
    setCheckboxStateSilently(ui->checkEditorGravity, editor.gravity);
    setCheckboxStateSilently(ui->checkEditorWallSelector, editor.wallSelector);
}

void MainWindow::updateEngineProperties()
{
    RENDERER_FLAGS flags = renderer.getFlags();
    setCheckboxStateSilently(ui->checkRendererMultithreadingEnable, flags & RENDERER_FLAG_MULTITHREADING);
    setCheckboxStateSilently(ui->checkRendererWallsEnable, flags & RENDERER_FLAG_WALLS);
    setCheckboxStateSilently(ui->checkRendererSurfacesEnable, flags & RENDERER_FLAG_SURFACES);
    setCheckboxStateSilently(ui->checkRendererLightsEnable, flags & RENDERER_FLAG_LIGHTS);
    setCheckboxStateSilently(ui->checkRendererOcclusionEnable, flags & RENDERER_FLAG_AMBIENT_OCCLUSION);
    setCheckboxStateSilently(ui->checkRendererMotionblur, flags & RENDERER_FLAG_MOTIONBLUR);
    setCheckboxStateSilently(ui->checkRendererVignetting, flags & RENDERER_FLAG_VIGNETTE);
    setCheckboxStateSilently(ui->checkRendererAlphaFeatures, flags & RENDERER_FLAG_ALPHA_FEATURES);
    setCheckboxStateSilently(ui->checkRendererGamma, flags & RENDERER_FLAG_GAMMA);

    ui->scrollRendererMotionblurPercent->blockSignals(true);
    ui->scrollRendererMotionblurPercent->setValue((int)renderer.motionBlurFactor);
    ui->scrollRendererMotionblurPercent->blockSignals(false);
    ui->scrollRendererVignettingInner->blockSignals(true);
    ui->scrollRendererVignettingInner->setValue((int)renderer.vignetteInnerRadius);
    ui->scrollRendererVignettingInner->blockSignals(false);
    ui->scrollRendererVignettingOuter->blockSignals(true);
    ui->scrollRendererVignettingOuter->setValue((int)renderer.vignetteOuterRadius);
    ui->scrollRendererVignettingOuter->blockSignals(false);

    ui->scrollRendererGammaKRed->blockSignals(true);
    ui->scrollRendererGammaKRed->setValue((int)(renderer.gammaKRed * 100.0f + 0.5f));
    ui->scrollRendererGammaKRed->blockSignals(false);
    ui->scrollRendererGammaKGreen->blockSignals(true);
    ui->scrollRendererGammaKGreen->setValue((int)(renderer.gammaKGreen * 100.0f + 0.5f));
    ui->scrollRendererGammaKGreen->blockSignals(false);
    ui->scrollRendererGammaKBlue->blockSignals(true);
    ui->scrollRendererGammaKBlue->setValue((int)(renderer.gammaKBlue * 100.0f + 0.5f));
    ui->scrollRendererGammaKBlue->blockSignals(false);

    setSpinValueSilently(ui->spinViewportResoX, renderer.getFrameResoX());
    setSpinValueSilently(ui->spinViewportResoY, renderer.getFrameResoY());

    setSpinValueSilently(ui->spinRendererOcclusionLength, renderer.occlusionLength);
    ui->scrollRendererOcclusionDarken->blockSignals(true);
    ui->scrollRendererOcclusionDarken->setValue((int)(renderer.occlusionDarken * 100.0f));
    ui->scrollRendererOcclusionDarken->blockSignals(false);

    int currentSize = renderer.getGlowmapSize();
    for (int i = 0; i < glowmapCount; i++) {
        if (glowmapSizes[i] == currentSize) {
            ui->comboGlowmapSize->blockSignals(true);
            ui->comboGlowmapSize->setCurrentIndex(i);
            ui->comboGlowmapSize->blockSignals(false);
            break;
        }
    }

    int currentArea = (int)renderer.getGlowmapArea();
    for (int i = 0; i < glowmapCount; i++) {
        if (glowmapAreas[i] == currentArea) {
            ui->comboGlowmapArea->blockSignals(true);
            ui->comboGlowmapArea->setCurrentIndex(i);
            ui->comboGlowmapArea->blockSignals(false);
            break;
        }
    }
}

/*****************************************************************************/
void MainWindow::on_spinNodeX_valueChanged(double arg1)
{
    if (editor.selectedNode < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->nodes.count(); i++) {
        Node & n = editor.editedMap->nodes[i];
        if (!n.selected) continue;
        n.pos.setX(arg1);
    }
}

void MainWindow::on_spinNodeY_valueChanged(double arg1)
{
    if (editor.selectedNode < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->nodes.count(); i++) {
        Node & n = editor.editedMap->nodes[i];
        if (!n.selected) continue;
        n.pos.setY(arg1);
    }
}

void MainWindow::on_spinNodeZ_valueChanged(double arg1)
{
    if (editor.selectedNode < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->nodes.count(); i++) {
        Node & n = editor.editedMap->nodes[i];
        if (!n.selected) continue;
        n.pos.setZ(arg1);
    }
}

void MainWindow::on_spinNodeMetaA_valueChanged(double arg1)
{
    if (editor.selectedNode < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->nodes.count(); i++) {
        Node & n = editor.editedMap->nodes[i];
        if (!n.selected) continue;
        n.metaA = arg1;
    }
}

void MainWindow::on_spinNodeMetaB_valueChanged(double arg1)
{
    if (editor.selectedNode < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->nodes.count(); i++) {
        Node & n = editor.editedMap->nodes[i];
        if (!n.selected) continue;
        n.metaB = arg1;
    }
}

void MainWindow::on_spinNodeMetaC_valueChanged(double arg1)
{
    if (editor.selectedNode < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->nodes.count(); i++) {
        Node & n = editor.editedMap->nodes[i];
        if (!n.selected) continue;
        n.metaC = arg1;
    }
}

void MainWindow::on_comboNodeTag_currentIndexChanged(int index)
{
    if (editor.selectedNode < 0) return;
    scheduleUndoPush();
    editor.editedMap->nodes[editor.selectedNode].tag = index;
}

/*****************************************************************************/
void MainWindow::on_pushNodeAddSubmap_clicked()
{
    if (editor.selectedNode < 0) return;
    pushUndoState();
    int mId;
    if (!editor.submapAdd(editor.selectedNode, mId))
        return;
    setEditMode(EDIT_MODE_SUBMAPS);
    editor.selectedSubmap = mId;
    updateSubmapProperties();
}

void MainWindow::on_pushNodeAddDoor_clicked()
{
    if (editor.selectedNode < 0) return;
    pushUndoState();
    int dId;
    if (!editor.doorAdd(editor.selectedNode, editor.selectedTextureID, dId))
        return;
    setEditMode(EDIT_MODE_DOORS);
    editor.selectedDoor = dId;
    updateDoorProperties();
}

void MainWindow::on_pushNodeAddSprite_clicked()
{
    if (editor.selectedNode < 0) return;
    pushUndoState();
    int bId;
    if (!editor.spriteAdd(editor.selectedNode, editor.selectedTextureID, bId))
        return;
    setEditMode(EDIT_MODE_SPRITES);
    editor.selectedSprite = bId;
    updateSpriteProperties();
}

void MainWindow::on_pushNodeAddStaircase_clicked()
{
    if (editor.selectedNode < 0) return;
    pushUndoState();
    int hId;
    if (!editor.staircaseAdd(editor.selectedNode, editor.selectedTextureID, hId))
        return;
    setEditMode(EDIT_MODE_STAIRCASES);
    editor.selectedStaircase = hId;
    updateStaircaseProperties();
}

void MainWindow::on_pushNodeAddLight_clicked()
{
    if (editor.selectedNode < 0) return;
    pushUndoState();
    int lId;
    if (!editor.lightAdd(editor.selectedNode, lId))
        return;
    setEditMode(EDIT_MODE_LIGHTS);
    editor.selectedLight = lId;
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
    updateLightProperties();
}

void MainWindow::on_pushNodeAddSpeaker_clicked()
{
    if (editor.selectedNode < 0) return;
    pushUndoState();
    int aId;
    if (!editor.speakerAdd(editor.selectedNode, aId))
        return;

    editor.editedMap->sounds.append(nullptr);

    setEditMode(EDIT_MODE_SPEAKERS);
    editor.selectedSpeaker = aId;
    updateSpeakerProperties();
}

void MainWindow::on_pushNodeAddLift_clicked()
{
    if (editor.selectedNode < 0) return;
    pushUndoState();
    int eId;
    if (!editor.liftAdd(editor.selectedNode, editor.selectedTextureID, eId))
        return;
    setEditMode(EDIT_MODE_LIFTS);
    editor.selectedLift = eId;
    updateLiftProperties();
}

void MainWindow::on_pushNodeDelete_clicked()
{
    pushUndoState();
    for (int i = 0; i < editor.editedMap->nodes.count(); i++) {
        Node & n = editor.editedMap->nodes[i];
        if (!n.selected) continue;
        editor.nodeDelete(i--);
    }
    editor.selectedNode = EDIT_UNSELECTED;
    updateNodeProperties();
}

/*****************************************************************************/
void MainWindow::on_spinWallHeight_valueChanged(double arg1)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.height = arg1;
    }
}

void MainWindow::on_comboWallTexture_currentIndexChanged(int)
{
    updateWallTexture();
}

void MainWindow::on_spinWallTexture_valueChanged(int arg1)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    int type = ui->comboWallTexture->currentIndex();

    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.surfaces[type].id = arg1;
    }
    ui->widgetWallTexture->setID(arg1);
}

void MainWindow::on_spinWallScaleX_valueChanged(double arg1)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    int type = ui->comboWallTexture->currentIndex();

    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.surfaces[type].scaleX = arg1;
    }
}

void MainWindow::on_spinWallScaleY_valueChanged(double arg1)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    int type = ui->comboWallTexture->currentIndex();

    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.surfaces[type].scaleY = arg1;
    }
}

void MainWindow::on_spinWallShiftX_valueChanged(double arg1)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    int type = ui->comboWallTexture->currentIndex();

    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.surfaces[type].shiftX = arg1;
    }
}

void MainWindow::on_spinWallShiftY_valueChanged(double arg1)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    int type = ui->comboWallTexture->currentIndex();

    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.surfaces[type].shiftY = arg1;
    }
}

void MainWindow::on_checkWallInvisible_toggled(bool checked)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.flags &= ~WALL_FLAG_INVISIBLE;
        if (checked) w.flags |= WALL_FLAG_INVISIBLE;
    }
}

void MainWindow::on_checkWallAlpha_toggled(bool checked)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.flags &= ~WALL_FLAG_ALPHA;
        if (checked) w.flags |= WALL_FLAG_ALPHA;
    }
}

void MainWindow::on_checkWallBackculled_toggled(bool checked)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.flags &= ~WALL_FLAG_BACKCULLED;
        if (checked) w.flags |= WALL_FLAG_BACKCULLED;
    }
}

void MainWindow::on_checkWallCeilingFront_toggled(bool checked)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.flags &= ~WALL_FLAG_CEILING_FRONT;
        if (checked) w.flags |= WALL_FLAG_CEILING_FRONT;
    }
}

void MainWindow::on_checkWallCeilingBack_toggled(bool checked)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.flags &= ~WALL_FLAG_CEILING_BACK;
        if (checked) w.flags |= WALL_FLAG_CEILING_BACK;
    }
}

void MainWindow::on_checkWallFloorFront_toggled(bool checked)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.flags &= ~WALL_FLAG_FLOOR_FRONT;
        if (checked) w.flags |= WALL_FLAG_FLOOR_FRONT;
    }
}

void MainWindow::on_checkWallFloorBack_toggled(bool checked)
{
    if (editor.selectedWall < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        w.flags &= ~WALL_FLAG_FLOOR_BACK;
        if (checked) w.flags |= WALL_FLAG_FLOOR_BACK;
    }
}

void MainWindow::on_pushWallSwap_clicked()
{
    pushUndoState();
    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        int tmp = w.nodeID1;
        w.nodeID1 = w.nodeID2;
        w.nodeID2 = tmp;
        editor.editedMap->computeWall(i);
    }
}

void MainWindow::on_pushWallDelete_clicked()
{
    pushUndoState();
    for (int i = 0; i < editor.editedMap->walls.count(); i++) {
        Wall & w = editor.editedMap->walls[i];
        if (!w.selected) continue;
        editor.editedMap->nodes[w.nodeID1].selected = false;
        editor.editedMap->nodes[w.nodeID2].selected = false;
        editor.selectedNode = -1;
        editor.wallDelete(i--);
    }
    editor.selectedWall = EDIT_UNSELECTED;
    updateWallProperties();
}

/*****************************************************************************/
void MainWindow::on_plainSubmapName_textChanged()
{
    if (editor.selectedSubmap < 0) return;
    scheduleUndoPush();
    Submap & m = editor.editedMap->submaps[editor.selectedSubmap];
    strncpy(m.name, ui->plainSubmapName->toPlainText().toLocal8Bit().constData(), OBJECT_NAME_MAX);
    m.name[OBJECT_NAME_MAX] = 0;
}

void MainWindow::on_comboSubmapTag_currentIndexChanged(int index)
{
    if (editor.selectedSubmap < 0) return;
    scheduleUndoPush();
    Submap & m = editor.editedMap->submaps[editor.selectedSubmap];
    m.tag = index;
}

void MainWindow::on_spinSubmapPan_valueChanged(double arg1)
{
    if (editor.selectedSubmap < 0) return;
    scheduleUndoPush();
    Submap & m = editor.editedMap->submaps[editor.selectedSubmap];
    m.pan = arg1;
}

void MainWindow::on_spinSubmapScale_valueChanged(double arg1)
{
    if (editor.selectedSubmap < 0) return;
    scheduleUndoPush();
    Submap & m = editor.editedMap->submaps[editor.selectedSubmap];
    m.scale = arg1;
}

void MainWindow::on_pushSubmapBrowse_clicked()
{
    if (editor.selectedSubmap < 0) return;
    QString path = QDir::currentPath();
    QString file = QFileDialog::getOpenFileName(this, "Open game submap", path, "Map File (*.map)");
    if (file.isEmpty()) return;

    Submap & m = editor.editedMap->submaps[editor.selectedSubmap];
    auto & maps = editor.editedMap->maps;
    maps.append(Map());
    Map & subMap = maps[maps.count() - 1];

    QString relPath = editor.editedMap->getRelativePath(file);
    QString absPath = editor.editedMap->resolveRelativePath(file);
    subMap.path = relPath;

    subMap.load(absPath);
    strncpy(m.path, relPath.toUtf8().constData(), OBJECT_PATH_MAX);
    m.path[OBJECT_PATH_MAX] = 0;
}

void MainWindow::on_pushSubmapDelete_clicked()
{
    pushUndoState();
    for (int i = 0; i < editor.editedMap->submaps.count(); i++) {
        Submap & m = editor.editedMap->submaps[i];
        if (!m.selected) continue;
        editor.submapDelete(i--);
    }
    editor.selectedSubmap = EDIT_UNSELECTED;
    updateSubmapProperties();
}

/*****************************************************************************/
void MainWindow::on_plainSpriteName_textChanged()
{
    if (editor.selectedSprite < 0) return;
    scheduleUndoPush();
    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];
    strncpy(b.name, ui->plainSpriteName->toPlainText().toLocal8Bit().constData(), OBJECT_NAME_MAX);
    b.name[OBJECT_NAME_MAX] = 0;
}

void MainWindow::on_comboSpriteTag_currentIndexChanged(int index)
{
    if (editor.selectedSprite < 0) return;
    scheduleUndoPush();
    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];
    b.tag = index;
}

void MainWindow::on_spinSpriteWidth_valueChanged(double arg1)
{
    if (editor.selectedSprite < 0) return;
    scheduleUndoPush();
    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];
    b.width = arg1;
}

void MainWindow::on_spinSpriteHeight_valueChanged(double arg1)
{
    if (editor.selectedSprite < 0) return;
    scheduleUndoPush();
    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];
    b.height = arg1;
}

void MainWindow::on_spinSpritePan_valueChanged(double arg1)
{
    if (editor.selectedSprite < 0) return;
    scheduleUndoPush();
    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];
    b.pan = arg1;
}

void MainWindow::on_checkSpriteInvisible_toggled(bool checked)
{
    if (editor.selectedSprite < 0) return;
    scheduleUndoPush();
    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];
    b.flags &= ~SPRITE_FLAG_INVISIBLE;
    if (checked) b.flags |= SPRITE_FLAG_INVISIBLE;
}

void MainWindow::on_checkSpriteBackculled_toggled(bool checked)
{
    if (editor.selectedSprite < 0) return;
    scheduleUndoPush();
    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];
    b.flags &= ~SPRITE_FLAG_BACKCULLED;
    if (checked) b.flags |= SPRITE_FLAG_BACKCULLED;
}

void MainWindow::on_checkSpriteShadows_toggled(bool checked)
{
    if (editor.selectedSprite < 0) return;
    scheduleUndoPush();
    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];
    b.flags &= ~SPRITE_FLAG_SHADOWS;
    if (checked) b.flags |= SPRITE_FLAG_SHADOWS;
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
}

void MainWindow::on_checkSpriteAutopan_toggled(bool checked)
{
    if (editor.selectedSprite < 0) return;
    scheduleUndoPush();
    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];
    b.flags &= ~SPRITE_FLAG_AUTOPAN;
    if (checked) b.flags |= SPRITE_FLAG_AUTOPAN;
}

void MainWindow::on_spinSpriteTexture_valueChanged(int arg1)
{
    if (editor.selectedSprite < 0) return;
    scheduleUndoPush();
    Sprite & b = editor.editedMap->sprites[editor.selectedSprite];
    b.surface.id = arg1;
    ui->widgetSpriteTexture->setID(arg1);
}

void MainWindow::on_pushSpriteDelete_clicked()
{
    pushUndoState();
    for (int i = 0; i < editor.editedMap->sprites.count(); i++) {
        Sprite & s = editor.editedMap->sprites[i];
        if (!s.selected) continue;
        editor.spriteDelete(i--);
    }
    editor.selectedSprite = EDIT_UNSELECTED;
    updateSpriteProperties();
}

/*****************************************************************************/
void MainWindow::on_plainDoorName_textChanged()
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    strncpy(d.name, ui->plainDoorName->toPlainText().toLocal8Bit().constData(), OBJECT_NAME_MAX);
    d.name[OBJECT_NAME_MAX] = 0;
}

void MainWindow::on_comboDoorTag_currentIndexChanged(int index)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    d.tag = index;
}

void MainWindow::on_spinDoorWidth_valueChanged(double arg1)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    d.width = arg1;
}

void MainWindow::on_spinDoorHeight_valueChanged(double arg1)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    d.height = arg1;
}

void MainWindow::on_spinDoorThick_valueChanged(double arg1)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    d.thick = arg1;
}

void MainWindow::on_comboDoorMode_currentIndexChanged(int index)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    d.mode = index;
}

void MainWindow::on_spinDoorAngle_valueChanged(double arg1)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    d.angle = arg1;
}

void MainWindow::on_spinDoorSwing_valueChanged(double arg1)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    d.swing = arg1;
}

void MainWindow::on_spinDoorTime_valueChanged(double arg1)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    d.time = arg1;
}

void MainWindow::on_checkDoorAlpha_toggled(bool checked)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    d.flags &= ~DOOR_FLAG_ALPHA;
    if (checked) d.flags |= DOOR_FLAG_ALPHA;
}

void MainWindow::on_checkDoorLocked_toggled(bool checked)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    d.flags &= ~DOOR_FLAG_LOCKED;
    if (checked) {
        d.flags |= DOOR_FLAG_LOCKED;
        editor.editedMap->doorClose(editor.selectedDoor);
    }
}

void MainWindow::on_comboDoorEasing_currentIndexChanged(int index)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    editor.editedMap->doors[editor.selectedDoor].easing = index;
}

void MainWindow::on_comboDoorTexture_currentIndexChanged(int)
{
    updateDoorTexture();
}

void MainWindow::on_spinDoorTexture_valueChanged(int arg1)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    int type = ui->comboDoorTexture->currentIndex();
    d.surfaces[type].id = arg1;
    ui->widgetDoorTexture->setID(arg1);
}

void MainWindow::on_spinDoorScaleX_valueChanged(double arg1)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    int type = ui->comboDoorTexture->currentIndex();
    d.surfaces[type].scaleX = arg1;
}

void MainWindow::on_spinDoorScaleY_valueChanged(double arg1)
{
    if (editor.selectedDoor < 0) return;
    scheduleUndoPush();
    Door & d = editor.editedMap->doors[editor.selectedDoor];
    int type = ui->comboDoorTexture->currentIndex();
    d.surfaces[type].scaleY = arg1;
}

void MainWindow::on_pushDoorOpen_clicked()
{
    if (editor.selectedDoor < 0) return;
    editor.editedMap->doorOpen(editor.selectedDoor);
}

void MainWindow::on_pushDoorClose_clicked()
{
    if (editor.selectedDoor < 0) return;
    editor.editedMap->doorClose(editor.selectedDoor);
}

void MainWindow::on_pushDoorShake_clicked()
{
    if (editor.selectedDoor < 0) return;
    editor.editedMap->doorShake(editor.selectedDoor);
}

void MainWindow::on_pushDoorDelete_clicked()
{
    pushUndoState();
    for (int i = 0; i < editor.editedMap->doors.count(); i++) {
        Door & d = editor.editedMap->doors[i];
        if (!d.selected) continue;
        editor.doorDelete(i--);
    }
    editor.selectedDoor = EDIT_UNSELECTED;
    updateDoorProperties();
}

/*****************************************************************************/
void MainWindow::on_plainLiftName_textChanged()
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    Lift & e = editor.editedMap->lifts[editor.selectedLift];
    strncpy(e.name, ui->plainLiftName->toPlainText().toLocal8Bit().constData(), OBJECT_NAME_MAX);
    e.name[OBJECT_NAME_MAX] = 0;
}

void MainWindow::on_comboLiftTag_currentIndexChanged(int index)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    editor.editedMap->lifts[editor.selectedLift].tag = index;
}

void MainWindow::on_spinLiftWidth_valueChanged(double arg1)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    editor.editedMap->lifts[editor.selectedLift].width = arg1;
}

void MainWindow::on_spinLiftLength_valueChanged(double arg1)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    editor.editedMap->lifts[editor.selectedLift].length = arg1;
}

void MainWindow::on_spinLiftThick_valueChanged(double arg1)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    editor.editedMap->lifts[editor.selectedLift].thick = arg1;
}

void MainWindow::on_spinLiftTravel_valueChanged(double arg1)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    editor.editedMap->lifts[editor.selectedLift].travel = arg1;
}

void MainWindow::on_spinLiftTime_valueChanged(double arg1)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    editor.editedMap->lifts[editor.selectedLift].time = arg1;
}

void MainWindow::on_comboLiftMode_currentIndexChanged(int index)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    editor.editedMap->lifts[editor.selectedLift].mode = index;
}

void MainWindow::on_checkLiftAlpha_toggled(bool checked)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    Lift & e = editor.editedMap->lifts[editor.selectedLift];
    e.flags &= ~LIFT_FLAG_ALPHA;
    if (checked) e.flags |= LIFT_FLAG_ALPHA;
}

void MainWindow::on_checkLiftLocked_toggled(bool checked)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    Lift & e = editor.editedMap->lifts[editor.selectedLift];
    e.flags &= ~LIFT_FLAG_LOCKED;
    if (checked) e.flags |= LIFT_FLAG_LOCKED;
}

void MainWindow::on_checkLiftHaltable_toggled(bool checked)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    Lift & e = editor.editedMap->lifts[editor.selectedLift];
    e.flags &= ~LIFT_FLAG_HALTABLE;
    if (checked) e.flags |= LIFT_FLAG_HALTABLE;
}

void MainWindow::on_checkLiftContinuous_toggled(bool checked)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    Lift & e = editor.editedMap->lifts[editor.selectedLift];
    e.flags &= ~LIFT_FLAG_CONTINUOUS;
    if (checked) e.flags |= LIFT_FLAG_CONTINUOUS;
}

void MainWindow::on_checkLiftReturn_toggled(bool checked)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    Lift & e = editor.editedMap->lifts[editor.selectedLift];
    e.flags &= ~LIFT_FLAG_RETURN;
    if (checked) e.flags |= LIFT_FLAG_RETURN;
}

void MainWindow::on_comboLiftEasing_currentIndexChanged(int index)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    editor.editedMap->lifts[editor.selectedLift].easing = index;
}

void MainWindow::on_comboLiftTexture_currentIndexChanged(int)
{
    updateLiftTexture();
}

void MainWindow::on_spinLiftTexture_valueChanged(int arg1)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    int type = ui->comboLiftTexture->currentIndex();
    editor.editedMap->lifts[editor.selectedLift].surfaces[type].id = arg1;
    ui->widgetLiftTexture->setID(arg1);
}

void MainWindow::on_spinLiftScaleX_valueChanged(double arg1)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    int type = ui->comboLiftTexture->currentIndex();
    editor.editedMap->lifts[editor.selectedLift].surfaces[type].scaleX = arg1;
}

void MainWindow::on_spinLiftScaleY_valueChanged(double arg1)
{
    if (editor.selectedLift < 0) return;
    scheduleUndoPush();
    int type = ui->comboLiftTexture->currentIndex();
    editor.editedMap->lifts[editor.selectedLift].surfaces[type].scaleY = arg1;
}

void MainWindow::on_pushLiftStart_clicked()
{
    if (editor.selectedLift < 0) return;
    editor.editedMap->liftStart(editor.selectedLift);
}

void MainWindow::on_pushLiftStop_clicked()
{
    if (editor.selectedLift < 0) return;
    editor.editedMap->liftStop(editor.selectedLift);
}

void MainWindow::on_pushLiftDelete_clicked()
{
    pushUndoState();
    for (int i = 0; i < editor.editedMap->lifts.count(); i++) {
        Lift & e = editor.editedMap->lifts[i];
        if (!e.selected) continue;
        editor.liftDelete(i--);
    }
    editor.selectedLift = EDIT_UNSELECTED;
    updateLiftProperties();
}

/*****************************************************************************/
void MainWindow::on_spinStaircasePan_valueChanged(double arg1)
{
    if (editor.selectedStaircase < 0) return;
    scheduleUndoPush();
    Staircase & h = editor.editedMap->staircases[editor.selectedStaircase];
    h.pan = arg1;
}

void MainWindow::on_spinStaircaseHeight_valueChanged(double arg1)
{
    if (editor.selectedStaircase < 0) return;
    scheduleUndoPush();
    Staircase & h = editor.editedMap->staircases[editor.selectedStaircase];
    h.height = arg1;
}

void MainWindow::on_spinStaircaseWidth_valueChanged(double arg1)
{
    if (editor.selectedStaircase < 0) return;
    scheduleUndoPush();
    Staircase & h = editor.editedMap->staircases[editor.selectedStaircase];
    h.width = arg1;
}

void MainWindow::on_spinStaircaseLength_valueChanged(double arg1)
{
    if (editor.selectedStaircase < 0) return;
    scheduleUndoPush();
    Staircase & h = editor.editedMap->staircases[editor.selectedStaircase];
    h.length = arg1;
}

void MainWindow::on_spinStaircaseSteps_valueChanged(int arg1)
{
    if (editor.selectedStaircase < 0) return;
    scheduleUndoPush();
    Staircase & h = editor.editedMap->staircases[editor.selectedStaircase];
    h.steps = arg1;
}

void MainWindow::on_comboStaircaseTexture_currentIndexChanged(int)
{
    updateStaircaseTexture();
}

void MainWindow::on_spinStaircaseTexture_valueChanged(int arg1)
{
    if (editor.selectedStaircase < 0) return;
    scheduleUndoPush();
    Staircase & h = editor.editedMap->staircases[editor.selectedStaircase];
    int type = ui->comboStaircaseTexture->currentIndex();
    h.surfaces[type].id = arg1;
    ui->widgetStaircaseTexture->setID(arg1);
}

void MainWindow::on_spinStaircaseScaleX_valueChanged(double arg1)
{
    if (editor.selectedStaircase < 0) return;
    scheduleUndoPush();
    Staircase & h = editor.editedMap->staircases[editor.selectedStaircase];
    int type = ui->comboStaircaseTexture->currentIndex();
    h.surfaces[type].scaleX = arg1;
}

void MainWindow::on_spinStaircaseScaleY_valueChanged(double arg1)
{
    if (editor.selectedStaircase < 0) return;
    scheduleUndoPush();
    Staircase & h = editor.editedMap->staircases[editor.selectedStaircase];
    int type = ui->comboStaircaseTexture->currentIndex();
    h.surfaces[type].scaleY = arg1;
}

void MainWindow::on_pushStaircaseDelete_clicked()
{
    pushUndoState();
    for (int i = 0; i < editor.editedMap->staircases.count(); i++) {
        Staircase & h = editor.editedMap->staircases[i];
        if (!h.selected) continue;
        editor.staircaseDelete(i--);
    }
    editor.selectedStaircase = EDIT_UNSELECTED;
    updateStaircaseProperties();
}

/*****************************************************************************/
void MainWindow::on_pushLightColorA_clicked()
{
    if (editor.selectedLight < 0) return;
    pushUndoState();
    Light & sl = editor.editedMap->lights[editor.selectedLight];
    QColor picked = QColorDialog::getColor(QColor(sl.colorA), this, "Pick color A", QColorDialog::DontUseNativeDialog);
    if (!picked.isValid()) return;
    uint32_t color = picked.rgba();
    ui->pushLightColorA->setStyleSheet("background-color: " + QColor(color).name());

    for (int i = 0; i < editor.editedMap->lights.count(); i++) {
        Light & l = editor.editedMap->lights[i];
        if (!l.selected) continue;
        l.colorA = color;
    }
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
}

void MainWindow::on_pushLightColorB_clicked()
{
    if (editor.selectedLight < 0) return;
    pushUndoState();
    Light & sl = editor.editedMap->lights[editor.selectedLight];
    QColor picked = QColorDialog::getColor(QColor(sl.colorB), this, "Pick color B", QColorDialog::DontUseNativeDialog);
    if (!picked.isValid()) return;
    uint32_t color = picked.rgba();
    ui->pushLightColorB->setStyleSheet("background-color: " + QColor(color).name());

    for (int i = 0; i < editor.editedMap->lights.count(); i++) {
        Light & l = editor.editedMap->lights[i];
        if (!l.selected) continue;
        l.colorB = color;
    }
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
}

void MainWindow::on_scrollLightStrength_valueChanged(int value)
{
    if (editor.selectedLight < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->lights.count(); i++) {
        Light & l = editor.editedMap->lights[i];
        if (!l.selected) continue;
        l.strength = value * 0.03125f;
    }
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
}

void MainWindow::on_spinLightFalloff_valueChanged(double value)
{
    if (editor.selectedLight < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->lights.count(); i++) {
        Light & l = editor.editedMap->lights[i];
        if (!l.selected) continue;
        l.falloff = (float) value;
    }
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
}

void MainWindow::on_comboLightAnimation_currentIndexChanged(int index)
{
    if (editor.selectedLight < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->lights.count(); i++) {
        Light & l = editor.editedMap->lights[i];
        if (!l.selected) continue;
        l.anim = index;
    }
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
}

void MainWindow::on_scrollLightSpeed_valueChanged(int value)
{
    if (editor.selectedLight < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->lights.count(); i++) {
        Light & l = editor.editedMap->lights[i];
        if (!l.selected) continue;
        l.speed = value * 0.0625f;
    }
}


void MainWindow::on_checkLightEnable_toggled(bool checked)
{
    if (editor.selectedLight < 0) return;
    scheduleUndoPush();
    for (int i = 0; i < editor.editedMap->lights.count(); i++) {
        Light & l = editor.editedMap->lights[i];
        if (!l.selected) continue;
        l.flags &= ~LIGHT_FLAG_ENABLE;
        if (checked) l.flags |= LIGHT_FLAG_ENABLE;
    }
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
}

void MainWindow::on_comboLightTag_currentIndexChanged(int index)
{
    if (editor.selectedLight < 0) return;
    scheduleUndoPush();
    editor.editedMap->lights[editor.selectedLight].tag = index;
}

void MainWindow::on_pushLightDelete_clicked()
{
    pushUndoState();
    for (int i = 0; i < editor.editedMap->lights.count(); i++) {
        Light & l = editor.editedMap->lights[i];
        if (!l.selected) continue;
        editor.lightDelete(i--);
    }
    editor.selectedLight = EDIT_UNSELECTED;
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
    updateLightProperties();
}

/*****************************************************************************/
void MainWindow::on_plainSpeakerName_textChanged()
{
    if (editor.selectedSpeaker < 0) return;
    scheduleUndoPush();
    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];
    strncpy(a.name, ui->plainSpeakerName->toPlainText().toLocal8Bit().constData(), OBJECT_NAME_MAX);
    a.name[OBJECT_NAME_MAX] = 0;
}

void MainWindow::on_comboSpeakerTag_currentIndexChanged(int index)
{
    if (editor.selectedSpeaker < 0) return;
    scheduleUndoPush();
    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];
    a.tag = index;
}

void MainWindow::on_pushSpeakerBrowse_clicked()
{
    if (editor.selectedSpeaker < 0) return;

    QString path = QDir::currentPath();
    QString file = QFileDialog::getOpenFileName(this, "Open sound file", path, "Sound File (*.wav)");
    if (file.isEmpty()) return;

    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];
    QString absPath = editor.editedMap->resolveRelativePath(file);
    QString relPath = editor.editedMap->getRelativePath(file);

    QSpatialSound * o = editor.editedMap->sounds[editor.selectedSpeaker];
    QSpatialSound * s = audio.soundLoad(QUrl::fromLocalFile(absPath).toString());
    editor.editedMap->sounds.replace(editor.selectedSpeaker, s);
    if (o) delete o;
    editor.editedMap->speakerApplyFlags(editor.selectedSpeaker);

    strncpy(a.path, relPath.toUtf8().constData(), OBJECT_PATH_MAX);
    a.path[OBJECT_PATH_MAX] = 0;
    ui->plainSpeakerFilepath->setPlainText(QString::fromUtf8(a.path));
}

void MainWindow::on_spinSpeakerVolume_valueChanged(double arg1)
{
    if (editor.selectedSpeaker < 0) return;
    scheduleUndoPush();
    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];
    a.volume = arg1;
    editor.editedMap->speakerApplyFlags(editor.selectedSpeaker);
}

void MainWindow::on_spinSpeakerSize_valueChanged(double arg1)
{
    if (editor.selectedSpeaker < 0) return;
    scheduleUndoPush();
    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];
    a.size = arg1;
    editor.editedMap->speakerApplyFlags(editor.selectedSpeaker);
}

void MainWindow::on_spinSpeakerPan_valueChanged(double arg1)
{
    if (editor.selectedSpeaker < 0) return;
    scheduleUndoPush();
    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];
    a.pan = arg1;
    editor.editedMap->speakerApplyFlags(editor.selectedSpeaker);
}

void MainWindow::on_checkSpeakerAuto_toggled(bool checked)
{
    if (editor.selectedSpeaker < 0) return;
    scheduleUndoPush();
    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];
    a.flags &= ~SPEAKER_FLAG_AUTOPLAY;
    if (checked) a.flags |= SPEAKER_FLAG_AUTOPLAY;
    editor.editedMap->speakerApplyFlags(editor.selectedSpeaker);
}

void MainWindow::on_checkSpeakerTrigger_toggled(bool checked)
{
    if (editor.selectedSpeaker < 0) return;
    scheduleUndoPush();
    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];
    a.flags &= ~SPEAKER_FLAG_TRIGGER;
    if (checked) a.flags |= SPEAKER_FLAG_TRIGGER;
    editor.editedMap->speakerApplyFlags(editor.selectedSpeaker);
}

void MainWindow::on_checkSpeakerToggle_toggled(bool checked)
{
    if (editor.selectedSpeaker < 0) return;
    scheduleUndoPush();
    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];
    a.flags &= ~SPEAKER_FLAG_TOGGLE;
    if (checked) a.flags |= SPEAKER_FLAG_TOGGLE;
    editor.editedMap->speakerApplyFlags(editor.selectedSpeaker);
}

void MainWindow::on_checkSpeakerLoop_toggled(bool checked)
{
    if (editor.selectedSpeaker < 0) return;
    scheduleUndoPush();
    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];
    a.flags &= ~SPEAKER_FLAG_LOOP;
    if (checked) a.flags |= SPEAKER_FLAG_LOOP;
    editor.editedMap->speakerApplyFlags(editor.selectedSpeaker);
}

void MainWindow::on_checkSpeakerOmni_toggled(bool checked)
{
    if (editor.selectedSpeaker < 0) return;
    scheduleUndoPush();
    Speaker & a = editor.editedMap->speakers[editor.selectedSpeaker];
    a.flags &= ~SPEAKER_FLAG_OMNI;
    if (checked) a.flags |= SPEAKER_FLAG_OMNI;
    editor.editedMap->speakerApplyFlags(editor.selectedSpeaker);
}

void MainWindow::on_pushSpeakerDelete_clicked()
{
    pushUndoState();
    for (int i = 0; i < editor.editedMap->speakers.count(); i++) {
        Speaker & a = editor.editedMap->speakers[i];
        if (!a.selected) continue;
        editor.speakerDelete(i--);
    }
    editor.selectedSpeaker = EDIT_UNSELECTED;
    updateSpeakerProperties();
}

/*****************************************************************************/
void MainWindow::updatePathProperties()
{
    ui->comboPathsList->blockSignals(true);
    ui->comboPathsList->clear();
    ui->comboPathsList->addItem("None");
    for (Path & p : editor.editedMap->paths)
        ui->comboPathsList->addItem(QString::fromUtf8(p.name));
    ui->comboPathsList->setCurrentIndex(editor.selectedPath + 1);
    ui->comboPathsList->blockSignals(false);

    if (editor.selectedPath < 0 || editor.selectedPath >= editor.editedMap->paths.count()) {
        ui->plainPathID->setPlainText("None");
        ui->plainPathName->setPlainText("");
        ui->comboPathTag->setCurrentIndex(0);
        ui->listPathNodes->blockSignals(true);
        ui->listPathNodes->clear();
        ui->listPathNodes->blockSignals(false);
        return;
    }

    Path & p = editor.editedMap->paths[editor.selectedPath];

    ui->plainPathID->setPlainText(QString::number(editor.selectedPath));

    ui->plainPathName->blockSignals(true);
    ui->plainPathName->setPlainText(QString::fromUtf8(p.name));
    ui->plainPathName->blockSignals(false);

    ui->comboPathTag->setCurrentIndex(p.tag);

    ui->listPathNodes->blockSignals(true);
    ui->listPathNodes->clear();
    for (int i = 0; i < p.nodesCount; i++)
        ui->listPathNodes->addItem(QString::asprintf("Node %04X", (unsigned int)p.nodes[i]));
    ui->listPathNodes->blockSignals(false);
}

void MainWindow::on_comboPathsList_currentIndexChanged(int index)
{
    if (index < 0 || index > editor.editedMap->paths.count()) return;
    editor.selectedPath = index - 1;
    updatePathProperties();
}

void MainWindow::on_plainPathName_textChanged()
{
    if (editor.selectedPath < 0) return;
    scheduleUndoPush();

    Path & p = editor.editedMap->paths[editor.selectedPath];
    strncpy(p.name, ui->plainPathName->toPlainText().toLocal8Bit().constData(), OBJECT_NAME_MAX);
    p.name[OBJECT_NAME_MAX] = 0;

    ui->comboPathsList->blockSignals(true);
    ui->comboPathsList->setItemText(editor.selectedPath + 1, QString::fromUtf8(p.name));
    ui->comboPathsList->blockSignals(false);
}

void MainWindow::on_comboPathTag_currentIndexChanged(int index)
{
    if (editor.selectedPath < 0) return;
    scheduleUndoPush();
    Path & p = editor.editedMap->paths[editor.selectedPath];
    p.tag = index;
}

void MainWindow::on_pushPathNew_clicked()
{
    pushUndoState();
    int pId;
    editor.pathAdd(pId);
    editor.selectedPath = pId;
    updatePathProperties();
}

void MainWindow::on_comboPathsList_lineEdit_returnPressed()
{
    QString name = ui->comboPathsList->currentText().trimmed();
    if (name.isEmpty()) return;

    pushUndoState();
    int pId;
    editor.pathAdd(pId);
    Path & p = editor.editedMap->paths[pId];
    strncpy(p.name, name.toUtf8().constData(), OBJECT_NAME_MAX);
    p.name[OBJECT_NAME_MAX] = 0;

    editor.selectedPath = pId;
    updatePathProperties();
}

void MainWindow::on_pushPathDelete_clicked()
{
    if (editor.selectedPath < 0) return;
    pushUndoState();
    editor.pathDelete(editor.selectedPath);
    editor.selectedPath = EDIT_UNSELECTED;
    updatePathProperties();
}

void MainWindow::on_pushPathNodeUp_clicked()
{
    if (editor.selectedPath < 0) return;
    int row = ui->listPathNodes->currentRow();
    if (row <= 0) return;

    pushUndoState();
    Path & p = editor.editedMap->paths[editor.selectedPath];
    uint16_t tmp = p.nodes[row];
    p.nodes[row] = p.nodes[row - 1];
    p.nodes[row - 1] = tmp;

    updatePathProperties();
    ui->listPathNodes->setCurrentRow(row - 1);
}

void MainWindow::on_pushPathNodeDown_clicked()
{
    if (editor.selectedPath < 0) return;
    Path & p = editor.editedMap->paths[editor.selectedPath];
    int row = ui->listPathNodes->currentRow();
    if (row < 0 || row >= p.nodesCount - 1) return;

    pushUndoState();
    uint16_t tmp = p.nodes[row];
    p.nodes[row] = p.nodes[row + 1];
    p.nodes[row + 1] = tmp;

    updatePathProperties();
    ui->listPathNodes->setCurrentRow(row + 1);
}

void MainWindow::on_pushPathNodesAdd_clicked()
{
    if (editor.selectedPath < 0) return;
    pushUndoState();

    Path & p = editor.editedMap->paths[editor.selectedPath];
    for (int i = 0; i < editor.editedMap->nodes.count(); i++) {
        if (!editor.editedMap->nodes[i].selected) continue;
        if (p.nodesCount >= PATH_NODES_MAX) break;
        p.nodes[p.nodesCount++] = (uint16_t)i;
    }

    updatePathProperties();
}

void MainWindow::on_pushPathNodesClear_clicked()
{
    if (editor.selectedPath < 0) return;
    pushUndoState();

    Path & p = editor.editedMap->paths[editor.selectedPath];
    p.nodesCount = 0;

    updatePathProperties();
}

void MainWindow::on_listPathNodes_itemSelectionChanged()
{
    if (editor.selectedPath < 0) return;
    Path & p = editor.editedMap->paths[editor.selectedPath];

    editor.nodeDeselectAll();
    editor.selectedNode = EDIT_UNSELECTED;

    for (int row = 0; row < ui->listPathNodes->count(); row++) {
        if (!ui->listPathNodes->item(row)->isSelected()) continue;
        if (row >= p.nodesCount) continue;
        int nodeID = p.nodes[row];
        if (nodeID >= editor.editedMap->nodes.count()) continue;
        editor.editedMap->nodes[nodeID].selected = true;
        editor.selectedNode = nodeID;
    }

    int current = ui->listPathNodes->currentRow();
    if (current >= 0 && current < p.nodesCount &&
        ui->listPathNodes->item(current)->isSelected()) {
        int nodeID = p.nodes[current];
        if (nodeID < editor.editedMap->nodes.count())
            editor.selectedNode = nodeID;
    }

    updateNodeProperties();
}

/*****************************************************************************/
void MainWindow::updateTagProperties()
{
    int selected = ui->comboTagsList->currentIndex();
    ui->comboTagsList->blockSignals(true);
    ui->comboTagsList->clear();
    for (Tag & t : tags.tags)
        ui->comboTagsList->addItem(QString::fromUtf8(t.name));
    if (selected >= 0 && selected < ui->comboTagsList->count())
        ui->comboTagsList->setCurrentIndex(selected);
    ui->comboTagsList->blockSignals(false);

    ui->plainTagsFilepath->setPlainText(tags.path);

    int idx = ui->comboTagsList->currentIndex();
    if (idx < 0 || idx >= tags.tags.count()) {
        ui->plainTagID->setPlainText("None");
        ui->plainTagName->setPlainText("");
        ui->plainTagValue->setPlainText("");
    } else {
        ui->plainTagID->setPlainText(QString::number(idx));
        ui->plainTagName->blockSignals(true);
        ui->plainTagValue->blockSignals(true);
        ui->plainTagName->setPlainText(QString::fromUtf8(tags.tags[idx].name));
        ui->plainTagValue->setPlainText(QString::fromUtf8(tags.tags[idx].value));
        ui->plainTagName->blockSignals(false);
        ui->plainTagValue->blockSignals(false);
    }
}

void MainWindow::refreshTagCombos()
{
    QStringList items = {"None"};
    for (Tag & t : tags.tags)
        items << QString::fromUtf8(t.name);

    auto refresh = [&](QComboBox * combo, int current) {
        combo->blockSignals(true);
        combo->clear();
        combo->addItems(items);
        int clamped = qBound(0, current, combo->count() - 1);
        combo->setCurrentIndex(clamped);
        combo->blockSignals(false);
    };

    int nodeTag    = (editor.selectedNode    >= 0) ? editor.editedMap->nodes[editor.selectedNode].tag    : 0;
    int submapTag  = (editor.selectedSubmap  >= 0) ? editor.editedMap->submaps[editor.selectedSubmap].tag : 0;
    int doorTag    = (editor.selectedDoor    >= 0) ? editor.editedMap->doors[editor.selectedDoor].tag    : 0;
    int liftTag    = (editor.selectedLift    >= 0) ? editor.editedMap->lifts[editor.selectedLift].tag    : 0;
    int spriteTag  = (editor.selectedSprite  >= 0) ? editor.editedMap->sprites[editor.selectedSprite].tag : 0;
    int lightTag   = (editor.selectedLight   >= 0) ? editor.editedMap->lights[editor.selectedLight].tag   : 0;
    int speakerTag = (editor.selectedSpeaker >= 0) ? editor.editedMap->speakers[editor.selectedSpeaker].tag : 0;
    int pathTag    = (editor.selectedPath    >= 0) ? editor.editedMap->paths[editor.selectedPath].tag    : 0;

    refresh(ui->comboNodeTag,    nodeTag);
    refresh(ui->comboSubmapTag,  submapTag);
    refresh(ui->comboDoorTag,    doorTag);
    refresh(ui->comboLiftTag,    liftTag);
    refresh(ui->comboSpriteTag,  spriteTag);
    refresh(ui->comboLightTag,   lightTag);
    refresh(ui->comboSpeakerTag, speakerTag);
    refresh(ui->comboPathTag,    pathTag);
}

void MainWindow::on_pushTagNew_clicked()
{
    if (tags.tags.count() >= TAG_COUNT_MAX) return;

    Tag t{};
    strcpy(t.name, "NewTag");
    tags.tags.append(t);

    updateTagProperties();
    ui->comboTagsList->setCurrentIndex(tags.tags.count() - 1);
    refreshTagCombos();
}

void MainWindow::on_comboTagsList_lineEdit_returnPressed()
{
    QString name = ui->comboTagsList->currentText().trimmed().replace(' ', '_').remove(',');
    if (name.isEmpty()) return;
    if (tags.tags.count() >= TAG_COUNT_MAX) return;

    Tag t{};
    strncpy(t.name, name.toUtf8().constData(), TAG_NAME_MAX);
    t.name[TAG_NAME_MAX] = 0;
    tags.tags.append(t);

    updateTagProperties();
    ui->comboTagsList->setCurrentIndex(tags.tags.count() - 1);
    refreshTagCombos();
}

void MainWindow::on_pushTagDelete_clicked()
{
    int idx = ui->comboTagsList->currentIndex();
    if (idx < 0 || idx >= tags.tags.count()) return;

    uint16_t deletedTag = (uint16_t)(idx + 1);
    tags.tags.removeAt(idx);

    for (Node    & n : editor.editedMap->nodes)    { if (n.tag == deletedTag) n.tag = 0; else if (n.tag > deletedTag) n.tag--; }
    for (Submap  & m : editor.editedMap->submaps)  { if (m.tag == deletedTag) m.tag = 0; else if (m.tag > deletedTag) m.tag--; }
    for (Door    & d : editor.editedMap->doors)    { if (d.tag == deletedTag) d.tag = 0; else if (d.tag > deletedTag) d.tag--; }
    for (Lift    & e : editor.editedMap->lifts)    { if (e.tag == deletedTag) e.tag = 0; else if (e.tag > deletedTag) e.tag--; }
    for (Sprite  & b : editor.editedMap->sprites)  { if (b.tag == deletedTag) b.tag = 0; else if (b.tag > deletedTag) b.tag--; }
    for (Light   & l : editor.editedMap->lights)   { if (l.tag == deletedTag) l.tag = 0; else if (l.tag > deletedTag) l.tag--; }
    for (Speaker & a : editor.editedMap->speakers) { if (a.tag == deletedTag) a.tag = 0; else if (a.tag > deletedTag) a.tag--; }
    for (Path    & p : editor.editedMap->paths)    { if (p.tag == deletedTag) p.tag = 0; else if (p.tag > deletedTag) p.tag--; }

    updateTagProperties();
    refreshTagCombos();
}

void MainWindow::on_comboTagsList_currentIndexChanged(int)
{
    updateTagProperties();
}

void MainWindow::on_plainTagName_textChanged()
{
    int idx = ui->comboTagsList->currentIndex();
    if (idx < 0 || idx >= tags.tags.count()) return;

    QString name = ui->plainTagName->toPlainText().trimmed().replace(' ', '_').remove(',');
    strncpy(tags.tags[idx].name, name.toUtf8().constData(), TAG_NAME_MAX);
    tags.tags[idx].name[TAG_NAME_MAX] = 0;

    ui->comboTagsList->blockSignals(true);
    ui->comboTagsList->setItemText(idx, name);
    ui->comboTagsList->blockSignals(false);

    refreshTagCombos();
}

void MainWindow::on_plainTagValue_textChanged()
{
    int idx = ui->comboTagsList->currentIndex();
    if (idx < 0 || idx >= tags.tags.count()) return;

    QString value = ui->plainTagValue->toPlainText();
    strncpy(tags.tags[idx].value, value.toUtf8().constData(), TAG_VALUE_MAX);
    tags.tags[idx].value[TAG_VALUE_MAX] = 0;
}

void MainWindow::on_pushTagsLoad_clicked()
{
    QString path = tags.path.isEmpty() ? QDir::currentPath() : tags.path;
    QString file = QFileDialog::getOpenFileName(this, "Open tags file", path, "Tags File (*.tag)");
    if (file.isEmpty()) return;

    tags.load(file);
    updateTagProperties();
    refreshTagCombos();
}

void MainWindow::on_pushTagsSave_clicked()
{
    QString path = tags.path.isEmpty() ? QDir::currentPath() : tags.path;
    QString file = QFileDialog::getSaveFileName(this, "Save tags file", path, "Tags File (*.tag)");
    if (file.isEmpty()) return;

    tags.save(file);
    ui->plainTagsFilepath->setPlainText(tags.path);
}

void MainWindow::on_pushTagsClear_clicked()
{
    tags.clear();
    updateTagProperties();
    refreshTagCombos();
}

/*****************************************************************************/
void MainWindow::closeEvent(QCloseEvent *)
{
    if (renderWindow) {
        delete renderWindow;
        renderWindow = nullptr;
    }
}

void MainWindow::resizeEvent(QResizeEvent *)
{
    resizeUI();
    update();
}

void MainWindow::resizeUI()
{
    ui->tabEditorModes->setGeometry(10, 10, 201, height() - 41);

    int dh = (height() - 41) - 781;

    ui->listNodes->setGeometry(10, 500, 151, 271 + dh);

    ui->listPathNodes->setGeometry(10, 210, 151, 491 + dh);
    ui->pushPathNodeUp->setGeometry(10, 710 + dh, 61, 23);
    ui->pushPathNodeDown->setGeometry(10, 740 + dh, 61, 23);
    ui->pushPathNodesAdd->setGeometry(100, 710 + dh, 61, 23);
    ui->pushPathNodesClear->setGeometry(100, 740 + dh, 61, 23);

    int x0 = 220;
    int y0 = 10;

    int viewPortWidth  = width()  - 1140 + 740;
    int viewPortHeight = height() - 822  + 630;

// Collect enabled views in priority order
    QVector<VIEW_MODES> views;
    if (viewEnableTop) views.append(VIEW_TOP);
    if (viewEnableSide) views.append(VIEW_SIDE);
    if (viewEnableFront) views.append(VIEW_FRONT);
    if (viewEnable3D) views.append(VIEW_3D);

    int count = views.size();
    if (count == 0)
        return;

// Hide all first
    ui->widgetMap1->hide();
    ui->widgetMap2->hide();
    ui->widgetMap3->hide();
    ui->widgetMap4->hide();

    int halfW = viewPortWidth / 2 - 3;
    int halfH = viewPortHeight / 2 - 3;

    if (count == 1) {
        if (!viewEnable3D) {
            ui->widgetMap1->setViewMode(views[0]);
            ui->widgetMap1->setGeometry(x0, y0, viewPortWidth + 1, viewPortHeight + 1);
            ui->widgetMap1->show();
        }else{
            ui->widgetMap4->setGeometry(x0, y0, viewPortWidth + 1, viewPortHeight + 1);
            ui->widgetMap4->show();
        }

    }else if (count == 2) {
        ui->widgetMap1->setViewMode(views[0]);
        ui->widgetMap1->setGeometry(x0, y0, viewPortWidth + 1, halfH + 1);
        ui->widgetMap1->show();
        if (!viewEnable3D) {
            ui->widgetMap2->setViewMode(views[1]);
            ui->widgetMap2->setGeometry(x0, y0 + halfH + 6, viewPortWidth + 1, halfH + 1);
            ui->widgetMap2->show();
        }else{
            ui->widgetMap4->setGeometry(x0, y0 + halfH + 6, viewPortWidth + 1, halfH + 1);
            ui->widgetMap4->show();
        }

    }else if (count == 3) {
        ui->widgetMap1->setViewMode(views[0]);
        ui->widgetMap1->setGeometry(x0, y0, viewPortWidth + 1, halfH + 1);
        ui->widgetMap1->show();
        ui->widgetMap2->setViewMode(views[1]);
        ui->widgetMap2->setGeometry(x0, y0 + halfH + 6, halfW + 1, halfH + 1);
        ui->widgetMap2->show();
        if (!viewEnable3D) {
            ui->widgetMap3->setViewMode(views[2]);
            ui->widgetMap3->setGeometry(x0 + halfW + 6, y0 + halfH + 6, halfW + 1, halfH + 1);
            ui->widgetMap3->show();
        }else{
            ui->widgetMap4->setGeometry(x0 + halfW + 6, y0 + halfH + 6, halfW + 1, halfH + 1);
            ui->widgetMap4->show();
        }

    }else if (count == 4) {
        ui->widgetMap1->setViewMode(views[0]);
        ui->widgetMap1->setGeometry(x0, y0, halfW + 1, halfH + 1);
        ui->widgetMap1->show();

        ui->widgetMap2->setViewMode(views[1]);
        ui->widgetMap2->setGeometry(x0 + halfW + 6, y0, halfW + 1, halfH + 1);
        ui->widgetMap2->show();

        ui->widgetMap3->setViewMode(views[2]);
        ui->widgetMap3->setGeometry(x0, y0 + halfH + 6, halfW + 1, halfH + 1);
        ui->widgetMap3->show();

        ui->widgetMap4->setGeometry(x0 + halfW + 6, y0 + halfH + 6, halfW + 1, halfH + 1);
        ui->widgetMap4->show();
    }

    if (count >= 2) {
        //ui->widgetMap2->setOffsetZ(ui->widgetMap1->getOffsetZ());
        //ui->widgetMap3->setOffsetX(ui->widgetMap1->getOffsetX());
    }

    int refX = width() - 1140 + 964;
    int refY = height() - 822 + 644;

    ui->scrollMapV->setGeometry(refX, 10, 18, viewPortHeight + 1);
    ui->scrollMapH->setGeometry(220, refY, viewPortWidth + 1, 18);
    ui->pushMapCenter->setGeometry(refX-2, refY-2, 21, 21);
    refY += 22;

    ui->widgetTextureSelector->setGeometry(220, refY, viewPortWidth + 21, 51);
    ui->pushTextureBrowse->setGeometry(viewPortWidth - 10, 20, 21, 21);
    refY += 54;
    ui->scrollTextures->setGeometry(220, refY, viewPortWidth + 21, 18);
    refY += 20;

    ui->groupInfos->setGeometry(220, refY, viewPortWidth + 21, 51);
    ui->groupViewer->setGeometry(refX + 26, 10, 141, 251);
    ui->groupFOV->setGeometry(refX + 26, 270, 141, 91);
    ui->groupEditor->setGeometry(refX + 26, 370, 141, 141);
    ui->pushRendererGlowmapRebuild->setGeometry(refX + 26, 520, 141, 22);

    int infoY = height() - 822 + 760;
    ui->labelCopyright->setGeometry(refX + 26, infoY, 141, 16);
    ui->labelEmail->setGeometry(refX + 26, infoY + 16, 141, 16);
}

/*****************************************************************************/
void MainWindow::on_spinViewerX_valueChanged(double arg1)
{
    walker.pos.setX(arg1);
}

void MainWindow::on_spinViewerY_valueChanged(double arg1)
{
    walker.pos.setY(arg1);
}

void MainWindow::on_spinViewerZ_valueChanged(double arg1)
{
    walker.pos.setZ(arg1);
}

void MainWindow::on_spinViewerPan_valueChanged(double arg1)
{
    walker.pan = arg1;
}

void MainWindow::on_spinViewerMinY_valueChanged(double arg1)
{
    editor.viewMinY = arg1;
    if (editor.viewMaxY < editor.viewMinY) {
        editor.viewMaxY = editor.viewMinY;
        ui->spinViewerMaxY->setValue(arg1);
    }
}

void MainWindow::on_spinViewerMaxY_valueChanged(double arg1)
{
    editor.viewMaxY = arg1;
    if (editor.viewMinY > editor.viewMaxY) {
        editor.viewMinY = editor.viewMaxY;
        ui->spinViewerMinY->setValue(arg1);
    }
}

/*****************************************************************************/
void MainWindow::on_actionNew_triggered()
{
    renderer.init();
    editor.rootMap.init();
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;

    undoDebounceTimer->stop();
    undoHistory.clear();
    undoIndex = -1;
    pushUndoState();
}

void MainWindow::on_actionLoad_map_triggered()
{
    QString path = QDir::currentPath();
    QString file = QFileDialog::getOpenFileName(this, "Open game map", path, "Map File (*.map)");
    if (file.isEmpty()) return;

    editor.rootMap.load(file);
    editor.env.sun = editor.rootMap.sun;
    editor.env.fog = editor.rootMap.fog;
    updateSunProperties();
    updateFogProperties();
    updateTagProperties();
    refreshTagCombos();
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;

    undoDebounceTimer->stop();
    undoHistory.clear();
    undoIndex = -1;
    pushUndoState();
    setWindowTitle("Waller Editor : " + QFileInfo(file).fileName());
}

void MainWindow::on_actionSave_map_triggered()
{
    saveMap(false);
}

void MainWindow::on_actionSave_submap_triggered()
{
    saveMap(true);
}

void MainWindow::saveMap(bool submap)
{
    QString path = editor.editedMap->path;
    if (path.isEmpty())
        path = QDir::currentPath();

    QString file = QFileDialog::getSaveFileName(this, "Save game map", path, "Map File (*.map)");
    if (file.isEmpty()) return;

    editor.rootMap.sun = editor.env.sun;
    editor.rootMap.fog = editor.env.fog;
    editor.rootMap.save(file, submap);
    setWindowTitle("Waller Editor : " + QFileInfo(file).fileName());
}

/*****************************************************************************/
void MainWindow::on_actionSelectAll_triggered()
{
    editor.selectAll();
}

void MainWindow::on_actionDeselect_triggered()
{
    editor.clearSelection();
    updateAllProperties();
}

void MainWindow::on_actionCut_triggered()
{
    pushUndoState();
    editor.cut();
}

void MainWindow::on_actionCopy_triggered()
{
    editor.copy();
}

void MainWindow::on_actionPaste_triggered()
{
    pushUndoState();
    QVector2D cursor = ui->widgetMap1->getCursor();
    editor.paste(VIEW_TOP, cursor);
}

void MainWindow::on_actionDelete_triggered()
{
    on_deleteCurrent();
}

void MainWindow::on_actionAlign_triggered()
{
    editor.align();
}

/*****************************************************************************/
void MainWindow::on_pushTextureBrowse_clicked()
{
    QString path = QDir::currentPath();
    QString file = QFileDialog::getOpenFileName(this, "Open texture strip", path, "Image File (*.bmp *.png *.jpg)");
    if (file.isEmpty()) return;
    if (!editor.editedMap) return;

    editor.editedMap->textures.load(file);
    editor.selectedTextureID = 0;
    ui->widgetTextureSelector->setScroll(0);
}


void MainWindow::on_pushTextureSlice_clicked()
{
    QString path = QDir::currentPath();
    QString dir = QFileDialog::getExistingDirectory(this, "Chose slice directory", path);
    if (dir.isEmpty()) return;

    ui->widgetTextureSelector->slice(dir);
}

void MainWindow::on_pushTextureConcat_clicked()
{
    QString path = QDir::currentPath();
    QString dir = QFileDialog::getExistingDirectory(this, "Chose slice directory", path);
    if (dir.isEmpty()) return;

    ui->widgetTextureSelector->concat(dir);
}

/*****************************************************************************/
void MainWindow::on_actionQuit_triggered()
{
    close();
}

/*****************************************************************************/
void MainWindow::on_checkEditorGravity_toggled(bool checked)
{
    editor.gravity = checked;
}

void MainWindow::on_checkEditorCollisions_toggled(bool checked)
{
    editor.collisions = checked;
}

void MainWindow::on_checkEditorWallSelector_toggled(bool checked)
{
    editor.wallSelector = checked;
}

void MainWindow::on_checkRendererMultithreadingEnable_toggled(bool checked)
{
    renderer.checkFlag(RENDERER_FLAG_MULTITHREADING, checked);
}

void MainWindow::on_checkRendererWallsEnable_toggled(bool checked)
{
    renderer.checkFlag(RENDERER_FLAG_WALLS, checked);
}

void MainWindow::on_checkRendererSurfacesEnable_toggled(bool checked)
{
    renderer.checkFlag(RENDERER_FLAG_SURFACES, checked);
}

void MainWindow::on_checkRendererOcclusionEnable_toggled(bool checked)
{
    renderer.checkFlag(RENDERER_FLAG_AMBIENT_OCCLUSION, checked);
}

void MainWindow::on_checkRendererLightsEnable_toggled(bool checked)
{
    renderer.checkFlag(RENDERER_FLAG_LIGHTS, checked);
}

void MainWindow::on_checkRendererMotionblur_toggled(bool checked)
{
    renderer.checkFlag(RENDERER_FLAG_MOTIONBLUR, checked);
}

void MainWindow::on_checkRendererVignetting_toggled(bool checked)
{
    renderer.checkFlag(RENDERER_FLAG_VIGNETTE, checked);
}

void MainWindow::on_checkRendererAlphaFeatures_toggled(bool checked)
{
    renderer.checkFlag(RENDERER_FLAG_ALPHA_FEATURES, checked);
}

void MainWindow::on_spinRendererOcclusionLength_valueChanged(double arg1)
{
    renderer.occlusionLength = arg1;
}

void MainWindow::on_scrollRendererOcclusionDarken_valueChanged(int value)
{
    renderer.occlusionDarken = value * 0.01f;
}

/*****************************************************************************/
void MainWindow::on_scrollRendererMotionblurPercent_valueChanged(int value)
{
    renderer.motionBlurFactor = value;
}

void MainWindow::on_scrollRendererVignettingInner_valueChanged(int value)
{
    renderer.vignetteInnerRadius = value;
}

void MainWindow::on_scrollRendererVignettingOuter_valueChanged(int value)
{
    renderer.vignetteOuterRadius = value;
}

void MainWindow::on_checkRendererGamma_toggled(bool checked)
{
    renderer.checkFlag(RENDERER_FLAG_GAMMA, checked);
}

void MainWindow::on_scrollRendererGammaKRed_valueChanged(int value)
{
    renderer.gammaKRed = value * 0.01f;
}

void MainWindow::on_scrollRendererGammaKGreen_valueChanged(int value)
{
    renderer.gammaKGreen = value * 0.01f;
}

void MainWindow::on_scrollRendererGammaKBlue_valueChanged(int value)
{
    renderer.gammaKBlue = value * 0.01f;
}

/*****************************************************************************/
void MainWindow::on_spinFOVAngle_valueChanged(double arg1)
{
    renderer.setFOVAngle(arg1);
}

void MainWindow::on_spinFOVNear_valueChanged(double arg1)
{
    renderer.setFOVDistanceNear(arg1);
}

void MainWindow::on_spinFOVFar_valueChanged(double arg1)
{
    renderer.setFOVDistanceFar(arg1);
}

/*****************************************************************************/
void MainWindow::on_pushSunAmbient_clicked()
{
    QColor picked = QColorDialog::getColor(QColor(editor.env.sun.ambientColor), this, "Pick Ambient color", QColorDialog::DontUseNativeDialog);
    if (!picked.isValid()) return;
    editor.env.sun.ambientColor = picked.rgba();
    updateSunProperties();
}

void MainWindow::on_pushSunRay_clicked()
{
    QColor picked = QColorDialog::getColor(QColor(editor.env.sun.rayColor), this, "Pick Sun color", QColorDialog::DontUseNativeDialog);
    if (!picked.isValid()) return;
    editor.env.sun.rayColor = picked.rgba();
    updateSunProperties();
}

void MainWindow::on_spinSunHour_valueChanged(double arg1)
{
    editor.env.sun.hour = arg1;
}

void MainWindow::on_spinSunAngle_valueChanged(double arg1)
{
    editor.env.sun.angle = arg1;
}

void MainWindow::on_scrollSunAmbient_valueChanged(int value)
{
    editor.env.sun.ambientStrength = value * 0.01f;
}

void MainWindow::on_scrollSunRay_valueChanged(int value)
{
    editor.env.sun.rayStrength = value * 0.01f;
}

/*****************************************************************************/
void MainWindow::on_pushFogColor_clicked()
{
    QColor picked = QColorDialog::getColor(QColor(editor.env.fog.color), this, "Pick Fog color", QColorDialog::DontUseNativeDialog);
    if (!picked.isValid()) return;
    editor.env.fog.color = picked.rgba();
    updateFogProperties();
}

void MainWindow::on_spinFogNear_valueChanged(double arg1)
{
    editor.env.fog.distanceNear = arg1;
}

void MainWindow::on_spinFogFar_valueChanged(double arg1)
{
    editor.env.fog.distanceFar = arg1;
}

void MainWindow::on_checkFogEnable_toggled(bool checked)
{
    editor.env.fog.flags &= ~FOG_FLAG_ENABLE;
    if (checked) editor.env.fog.flags |= FOG_FLAG_ENABLE;
}

/*****************************************************************************/
void MainWindow::on_scrollTextures_valueChanged(int value)
{
    ui->widgetTextureSelector->setScroll(value);
}

/*****************************************************************************/
// The top view always stays enabled
void MainWindow::on_pushViewerTop_toggled(bool)
{
    viewEnableTop = true;
    ui->pushViewerTop->setChecked(true);
}

void MainWindow::on_pushViewerFront_toggled(bool checked)
{
    viewEnableFront = checked;
    resizeUI();
}

void MainWindow::on_pushViewerSide_toggled(bool checked)
{
    viewEnableSide = checked;
    resizeUI();
}

void MainWindow::on_pushViewer3D_toggled(bool checked)
{
    viewEnable3D = checked;
    resizeUI();
}

/*****************************************************************************/
void MainWindow::on_pushMapCenter_clicked()
{
    ui->widgetMap1->setCenter();
    ui->widgetMap2->setCenter();
    ui->widgetMap3->setCenter();
    ui->scrollMapH->setValue(0);
    ui->scrollMapV->setValue(0);
}

void MainWindow::on_scrollMapH_sliderMoved(int position)
{
    ui->widgetMap1->setOffsetX(position);
    ui->widgetMap2->setOffsetX(position);
    ui->widgetMap3->setOffsetX(position);
}

void MainWindow::on_scrollMapV_sliderMoved(int position)
{
    ui->widgetMap1->setOffsetZ(position);
    ui->widgetMap2->setOffsetZ(position);
    ui->widgetMap3->setOffsetZ(position);
}

/*****************************************************************************/
void MainWindow::on_actionAbout_triggered()
{
    QString aboutText;
    aboutText += "<b>Waller</b><br>";
    aboutText += "Designed for the <b>DIE Engine</b><br><br>";
    aboutText += "© 2024–2026 Frédéric Meslin<br>";
    aboutText += "<a href=\"https://fredslab.net\">Fred's Lab</a><br>";
    aboutText += "<a href=\"mailto:info@fredslab.net\">info@fredslab.net</a><br><br>";
    aboutText += "Open Source under the <b>MIT</b> license.<br>";
    aboutText += "If used commercially, contributions and donations are highly appreciated.<br><br>";
    aboutText += "Waller is an open-source map editing tool developed by Fred's Lab,<br>"
                 "for creating environments for DIE (Depth Integration Engine).";

    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle("About Waller");
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

/*****************************************************************************/
void MainWindow::on_tabEditorModes_currentChanged(int index)
{
    setEditMode((EDIT_MODES) index);
}

/*****************************************************************************/
void MainWindow::on_pushViewportReset_clicked()
{
    int resoX = ui->spinViewportResoX->value();
    int resoY = ui->spinViewportResoY->value();
    if (resoX == renderer.getFrameResoX() && resoY == renderer.getFrameResoY())
        return;
    renderer.setFramesReso(resoX, resoY);
}

/*****************************************************************************/
void MainWindow::on_comboGlowmapArea_currentIndexChanged(int index)
{
    if (index < 0 || index >= glowmapCount) return;
    renderer.setGlowmapArea(glowmapAreas[index]);
}

void MainWindow::on_comboGlowmapSize_currentIndexChanged(int index)
{
    if (index < 0 || index >= glowmapCount) return;
    renderer.setGlowmapSize(glowmapSizes[index]);
}

void MainWindow::on_pushRendererGlowmapRebuild_clicked()
{
    renderer.flags |= RENDERER_FLAG_GLOWMAP_REBUILD;
}

/*****************************************************************************/
void MainWindow::on_spinEditorFloor_valueChanged(double)
{

}


