#include "qt_screen.h"

Screen::Screen() {
    mainWindow.setOrientation(MainWindow::ScreenOrientationLockLandscape);
    mainWindow.showExpanded();
}

Screen::~Screen() {
}

int Screen::getElapsedMS() {
    return static_cast<int>(elapsed_timer.elapsed());
}

void Screen::runMainLoop() {
    //LOG("run main loop...\n");
    const int time_per_frame_c = 1000/25;
    QObject::connect(&timer, SIGNAL(timeout()), &mainWindow, SLOT(updateScene()));
    timer.start(time_per_frame_c);
    elapsed_timer.start();

    qApp->exec();
}

void Screen::enableTimers(bool enabled) {
    if( enabled ) {
        timer.start();
    }
    else {
        timer.stop();
    }
}
