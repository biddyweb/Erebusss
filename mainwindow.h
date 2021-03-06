#pragma once

#include <QMainWindow>

#include "common.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

protected:
    void closeEvent(QCloseEvent *event);
    //int elapsed_time;
    //virtual void mouseReleaseEvent(QMouseEvent *event);
public:
    enum ScreenOrientation {
        ScreenOrientationLockPortrait,
        ScreenOrientationLockLandscape,
        ScreenOrientationAuto
    };

    explicit MainWindow(QWidget *parent = 0);
    virtual ~MainWindow();

    // Note that this will only have an effect on Symbian and Fremantle.
    void setOrientation(ScreenOrientation orientation);

    void showExpanded(bool fullscreen);

    virtual void keyPressEvent(QKeyEvent *event);
/*public slots:
    void updateScene();*/
};
