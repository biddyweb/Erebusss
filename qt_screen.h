#pragma once

#include "mainwindow.h"

#include <QtGui>

class Screen {
    MainWindow mainWindow;
    QTimer timer;
    QElapsedTimer elapsed_timer;
public:
    Screen();
    ~Screen();

    MainWindow *getMainWindow() {
        return &mainWindow;
    }
    int getElapsedMS();
    void runMainLoop();
    void enableTimers(bool enabled);
};
