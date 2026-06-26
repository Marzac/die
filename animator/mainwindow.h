#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QAbstractSpinBox>

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

    /// \brief Refresh the viewer controls from the rig view
    void updateViewerProperties();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void resizeUI();
    void setSpinValueSilently(QAbstractSpinBox * box, double value);

    Ui::MainWindow *ui;

private slots:
    void on_spinJointX_valueChanged(double arg1);
    void on_spinJointY_valueChanged(double arg1);
    void on_spinJointZ_valueChanged(double arg1);
    void on_listJoints_itemSelectionChanged();
    void on_pushJointDelete_clicked();

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
