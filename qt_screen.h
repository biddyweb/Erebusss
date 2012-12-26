#pragma once

#include "common.h"

#include "mainwindow.h"

#include <QtGui>

class Screen : public QObject {
    Q_OBJECT

    QTimer timer; // used to call update() per frame
    MainWindow *mainWindow;
    QElapsedTimer elapsed_timer; // used to measure game time

    int getElapsedMS() const;

    bool paused;
    int saved_paused_time_ms;
    int saved_elapsed_time_ms;
    int game_time_total_ms;
    int game_time_frame_ms;
    int real_time_frame_ms;
    int accumulator;

private slots:
    void update();

public:
    Screen();
    ~Screen();

    MainWindow *getMainWindow() {
        return mainWindow;
    }
    const MainWindow *getMainWindow() const {
        return mainWindow;
    }
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
    int getRealTimeFrameMS() const {
        return this->real_time_frame_ms;
    }

public slots:
    void togglePaused() {
        setPaused(!paused);
    }
};
