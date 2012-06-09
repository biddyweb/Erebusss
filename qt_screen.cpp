#include "qt_screen.h"

#include "game.h"

Screen::Screen() :
    mainWindow(NULL), paused(false), saved_paused_time_ms(0), saved_elapsed_time_ms(0), game_time_total_ms(0), game_time_frame_ms(0)
{
    qDebug("Screen::Screen()");
    mainWindow = new MainWindow();
    mainWindow->setOrientation(MainWindow::ScreenOrientationLockLandscape);
    mainWindow->showExpanded();
}

Screen::~Screen() {
    if( mainWindow != NULL )
        delete mainWindow;
}

int Screen::getElapsedMS() const {
    return static_cast<int>(elapsed_timer.elapsed());
}

void Screen::runMainLoop() {
    qDebug("run main loop...");
    const int time_per_frame_c = 1000/25;
    //QObject::connect(&timer, SIGNAL(timeout()), &mainWindow, SLOT(updateScene()));
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(update()));
    timer.start(time_per_frame_c);
    elapsed_timer.start();

    qApp->exec();
}

void Screen::update() {
    //qApp->beep();
    //qDebug("Screen::update()");

    int new_elapsed_time_ms = this->getElapsedMS();
    if( paused ) {
        this->game_time_frame_ms = 0;
    }
    else {
        this->game_time_frame_ms = new_elapsed_time_ms - this->saved_elapsed_time_ms;
        this->game_time_frame_ms = std::min(this->game_time_frame_ms, 1000);
        this->game_time_total_ms += this->game_time_frame_ms;
        this->saved_elapsed_time_ms = new_elapsed_time_ms;
        //qDebug("Update: frame time: %d", this->game_time_frame_ms);
    }

    game_g->update();
}

void Screen::setPaused(bool paused) {
    if( this->paused != paused ) {
        this->paused = paused;
        if( paused )
            this->saved_paused_time_ms = this->getElapsedMS();
        else {
            // factor out the time we were paused
            // n.b., we should always have saved_paused_time_ms >= saved_elapsed_time_ms
            // => we will have saved_elapsed_time_ms set to be <= time_now
            int time_now = this->getElapsedMS();
            int paused_time = time_now - this->saved_paused_time_ms;
            this->saved_elapsed_time_ms += paused_time;
            if( this->saved_elapsed_time_ms > time_now ) {
                LOG("time corruption when unpausing\n");
                LOG("time now: %d\n", time_now);
                LOG("paused time: %d\n", paused_time);
                LOG("saved_paused_time_ms: %d\n", this->saved_paused_time_ms);
                LOG("saved_elapsed_time_ms: %d\n", this->saved_elapsed_time_ms);
                throw string("time corruption when unpausing");
            }
        }
    }
}

void Screen::restartElapsedTimer() {
    this->elapsed_timer.restart();
    this->saved_elapsed_time_ms = this->elapsed_timer.elapsed();
    this->saved_paused_time_ms = this->saved_elapsed_time_ms;
}

void Screen::enableUpdateTimer(bool enabled) {
    qDebug("Screen:enableUpdateTimer(%d)", enabled);
    if( enabled ) {
        timer.start();
    }
    else {
        timer.stop();
    }
}
