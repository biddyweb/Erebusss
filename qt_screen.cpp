#include <QApplication>

#include "qt_screen.h"
#include "game.h"
#include "logiface.h"

//const int time_per_frame_c = 1000/25;
const int time_per_frame_c = 1000/60;
//const int time_per_frame_c = 1000/200;

GameClock::GameClock() : paused(false), saved_paused_time_ms(0), saved_elapsed_time_ms(0), game_time_total_ms(0), game_time_frame_ms(0), accumulator(0), multiplier(1) {
}

void GameClock::setPaused(bool paused, int time_now_ms) {
    if( this->paused != paused ) {
        this->paused = paused;
        if( paused ) {
            this->saved_paused_time_ms = time_now_ms;
            //qDebug("pausing at: %d", this->saved_paused_time_ms);
            //qDebug("### Saved elapsed time: %d", this->saved_elapsed_time_ms);
            //qDebug("### Accumulator: %d", accumulator);
        }
        else {
            // factor out the time we were paused
            // n.b., we should always have saved_paused_time_ms >= saved_elapsed_time_ms
            // => we will have saved_elapsed_time_ms set to be <= time_now
            int paused_time = time_now_ms - this->saved_paused_time_ms;
            this->saved_elapsed_time_ms += paused_time;
            //qDebug("unpausing at: %d , paused for %d", time_now, paused_time);
            //qDebug("### Saved elapsed time: %d", this->saved_elapsed_time_ms);
            //qDebug("### Accumulator: %d", accumulator);
            if( this->saved_elapsed_time_ms > time_now_ms ) {
                LOG("time corruption when unpausing\n");
                LOG("time now: %d\n", time_now_ms);
                LOG("paused time: %d\n", paused_time);
                LOG("saved_paused_time_ms: %d\n", this->saved_paused_time_ms);
                LOG("saved_elapsed_time_ms: %d\n", this->saved_elapsed_time_ms);
                throw string("time corruption when unpausing");
            }
        }
    }
}

int GameClock::update(int time_now_ms) {
    int n_updates = 0;
    this->game_time_frame_ms = 0;
    if( !paused ) {
        // based on http://gafferongames.com/game-physics/fix-your-timestep/
        /*
        // Fixed timestep
        const int update_dt_c = time_per_frame_c;
        //qDebug("time from %d to %d", this->saved_elapsed_time_ms, new_elapsed_time_ms);
        int elapsed_time_ms = time_now_ms - this->saved_elapsed_time_ms;
        elapsed_time_ms = std::min(elapsed_time_ms, 200); // prevent death spiral
        this->saved_elapsed_time_ms = time_now_ms;
        this->accumulator += elapsed_time_ms;
        //qDebug("Elapsed time: %d", elapsed_time_ms);
        //qDebug("Accumulator: %d", accumulator);

        this->game_time_frame_ms = update_dt_c;
        while( accumulator >= update_dt_c ) {
            //qDebug("    Update: frame time: %d", this->game_time_frame_ms);
            accumulator -= update_dt_c;
            n_updates++;
        }
        */
        // Semi-fixed timestep
        // (Fixed timestep has problems that if time_per_frame_c is too small, we have trouble updating and end up in a death spiral due to calling update() too many times per frame;
        // if time_per_frame_c is too large, we don't get smooth update on faster platforms.)
        // Note that a proper implementation of semi-fixed timestep would typically have a smaller value for max_interval_c,
        // and then do the sufficient number of updates to catch up (of length max_interval_c, except for 1 iteration to do handle the remainder time),
        // with a max number of iterations to prevent a "death spiral". However here we're setting a largish value of max_interval_c,
        // and effectively setting a max iteration of 1.
        // In practice on slow systems, we are just as likely to be update-bound rather than render-bound, so there isn't any benefit to using a smaller value,
        // and testing as shown that the update is stable up to 200ms.
        // We pick a max_interval_c of 200ms, as (a) we should still be safe with a timestep of this size; (b) Nokia 5800 with lighting effects can sometimes have FPS of just under 10, so we want something larger than 100ms.
        // Also note: normally it would be good practice to only call the update if at least time_per_frame_c has passed, but with Qt we already have this function being called on a timer.
        const int max_interval_c = 200;
        int elapsed_time_ms = time_now_ms - this->saved_elapsed_time_ms;
        elapsed_time_ms *= multiplier;
        elapsed_time_ms = std::min(elapsed_time_ms, max_interval_c); // prevent too large a timestep being sent to the update, to avoid instability
        this->saved_elapsed_time_ms = time_now_ms;
        if( elapsed_time_ms > 0 ) {
            this->game_time_frame_ms = elapsed_time_ms;
            n_updates = 1;
            //qDebug("    Update: frame time: %d", this->game_time_frame_ms);
        }
    }
    return n_updates;
}

void GameClock::callingUpdate() {
    this->game_time_total_ms += this->game_time_frame_ms;
}

void GameClock::restart() {
    this->saved_elapsed_time_ms = 0;
    this->saved_paused_time_ms = 0;
    this->accumulator = 0;
}

Screen::Screen(bool fullscreen) :
    fullscreen(fullscreen), mainWindow(NULL), last_time_ms(-1), elapsed_frame_ms(0)
{
    LOG("Screen::Screen()\n");
    mainWindow = new MainWindow();
    mainWindow->setOrientation(MainWindow::ScreenOrientationLockLandscape);
    mainWindow->showExpanded(fullscreen);
}

Screen::~Screen() {
    LOG("Screen::~Screen()\n");
    if( mainWindow != NULL )
        delete mainWindow;
}

int Screen::getElapsedMS() const {
    return static_cast<int>(elapsed_timer.elapsed());
}

int Screen::getElapsedFrameMS() const {
    return elapsed_frame_ms;
}

void Screen::initMainLoop() {
    qDebug("init main loop...");
    //QObject::connect(&timer, SIGNAL(timeout()), &mainWindow, SLOT(updateScene()));
    last_time_ms = -1;
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(update()));
    timer.start(time_per_frame_c);
    elapsed_timer.start();
}

void Screen::runMainLoop() {
    qDebug("run main loop...");
    qApp->exec();
}

void Screen::update() {
    //qApp->beep();
    //qDebug("Screen::update()");

    int time_now_ms = this->getElapsedMS();
    //qDebug("time_now_ms: %d", time_now_ms);
    if( last_time_ms != -1 ) {
        elapsed_frame_ms = time_now_ms - last_time_ms;
    }
    last_time_ms = time_now_ms;

    {
        int n_updates = game_clock.update(time_now_ms);
        for(int i=0;i<n_updates;i++) {
            //qDebug("%d : frame time %d total time %d", i, game_clock.getGameTimeFrameMS(), game_clock.getGameTimeTotalMS());
            game_clock.callingUpdate();
            game_g->update();
        }
        //qDebug("    time for %d updates: %d", n_updates, this->getElapsedMS() - time_now_ms);
    }

    {
        int n_updates = input_clock.update(time_now_ms);
        for(int i=0;i<n_updates;i++) {
            input_clock.callingUpdate();
            game_g->updateInput();
        }
        //qDebug("    time for %d input updates: %d", n_updates, this->getElapsedMS() - time_now_ms);
    }

    //qDebug("    Render");
    game_g->render();

    game_g->handleMessages();
}

void Screen::setPaused(bool paused, bool also_input) {
    //qDebug("Screen::setPaused(%d, %d)", paused, also_input);
    int time_now_ms = this->getElapsedMS();
    game_clock.setPaused(paused, time_now_ms);
    if( also_input || !paused ) {
        // if unpausing, always also unpause the input clock
        input_clock.setPaused(paused, time_now_ms);
    }
}

void Screen::restartElapsedTimer() {
    qDebug("Screen::restartElapsedTimer()");
    this->elapsed_timer.restart();
    this->game_clock.restart();
    this->input_clock.restart();
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
