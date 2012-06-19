#pragma once

#include <QtGui/QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

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

    void showExpanded();

/*public slots:
    void updateScene();*/
};
