#pragma once

#include "mainwindow.h"

#include <QtGui>

class Screen : public QObject {
    Q_OBJECT

    MainWindow mainWindow;
    QTimer timer; // used to call update() per frame
    QElapsedTimer elapsed_timer; // used to measure game time

    bool paused;
    int saved_paused_time_ms;
    int saved_elapsed_time_ms;
    int game_time_total_ms;
    int game_time_frame_ms;

private slots:
    void update();

public:
    Screen();
    ~Screen();

    MainWindow *getMainWindow() {
        return &mainWindow;
    }
    int getElapsedMS();
    void runMainLoop();
    bool isPaused() const {
        return this->paused;
    }
    void setPaused(bool paused);
    void restartElapsedTimer();
    void enableUpdateTimer(bool enabled);
    int getGameTimeFrameMS() const {
        return this->game_time_frame_ms;
    }
    int getGameTimeTotalMS() const {
        return this->game_time_total_ms;
    }

public slots:
    void togglePaused() {
        setPaused(!paused);
    }
};
