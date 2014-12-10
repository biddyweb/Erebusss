#pragma once

#include <QTimer>
#include <QElapsedTimer>

#include "common.h"

#include "mainwindow.h"

class GameClock {
    bool paused;
    int saved_paused_time_ms;
    int saved_elapsed_time_ms;
    int game_time_total_ms;
    int game_time_frame_ms;
    int accumulator;
    int multiplier;

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
    void setMultiplier(int multiplier) {
        this->multiplier = multiplier;
    }
};

class Screen : public QObject {
    Q_OBJECT

    bool fullscreen;
    QTimer timer; // used to call update() per frame
    MainWindow *mainWindow;
    QElapsedTimer elapsed_timer; // used to measure game time
    int last_time_ms;
    int elapsed_frame_ms;

    GameClock game_clock; // used for real-world game action
    GameClock input_clock; // used for input - as we still want to allow, e.g., changing viewpoint, even when game action is paused, but we might want to pause things like kinetic scrolling of map when sub-windows open

private slots:
    void update();

public:
    Screen(bool fullscreen);
    ~Screen();

    bool isFullscreen() const {
        return this->fullscreen;
    }
    MainWindow *getMainWindow() {
        return mainWindow;
    }
    const MainWindow *getMainWindow() const {
        return mainWindow;
    }
    void initMainLoop();
    void runMainLoop();
    bool isPaused() const {
        return this->game_clock.isPaused();
    }
    void setPaused(bool paused, bool also_input);
    void togglePaused() {
        this->setPaused( !this->isPaused(), false );
    }
    int getElapsedMS() const;
    int getElapsedFrameMS() const;
    void restartElapsedTimer();
    void enableUpdateTimer(bool enabled);
    int getGameTimeFrameMS() const {
        return this->game_clock.getGameTimeFrameMS();
    }
    int getGameTimeTotalMS() const {
        return this->game_clock.getGameTimeTotalMS();
    }
    int getInputTimeFrameMS() const {
        return this->input_clock.getGameTimeFrameMS();
    }
    void setGameTimeMultiplier(int multiplier) {
        this->game_clock.setMultiplier(multiplier);
    }
};
