#pragma once

#include "common.h"

#include "mainwindow.h"

#include <QtGui>

class GameClock {
    bool paused;
    int saved_paused_time_ms;
    int saved_elapsed_time_ms;
    int game_time_total_ms;
    int game_time_frame_ms;
    int accumulator;

public:
    GameClock();

    bool isPaused() const {
        return this->paused;
    }
    void setPaused(bool paused, int time_now_ms);

    int getGameTimeFrameMS() const {
        return this->game_time_frame_ms;
    }
    int getGameTimeTotalMS() const {
        return this->game_time_total_ms;
    }

    int update(int time_now_ms);
    void callingUpdate();
    void restart();
};

class Screen : public QObject {
    Q_OBJECT

    QTimer timer; // used to call update() per frame
    MainWindow *mainWindow;
    QElapsedTimer elapsed_timer; // used to measure game time

    GameClock game_clock;

    int getElapsedMS() const;

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
        return this->game_clock.isPaused();
    }
    void setPaused(bool paused);
    void restartElapsedTimer();
    void enableUpdateTimer(bool enabled);
    int getGameTimeFrameMS() const {
        return this->game_clock.getGameTimeFrameMS();
    }
    int getGameTimeTotalMS() const {
        return this->game_clock.getGameTimeTotalMS();
    }
    int getRealTimeFrameMS() const {
        return this->game_clock.getGameTimeFrameMS();
    }

public slots:
    void togglePaused() {
        this->setPaused( !this->isPaused() );
    }
};
