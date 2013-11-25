#include "mainwindow.h"
#include "qt_screen.h"
#include "game.h"
#include "gamestate.h"
#include "logiface.h"

#include <QApplication>
#include <QDesktopWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)/*, elapsed_time(0)*/
{
}

MainWindow::~MainWindow()
{
}

/*void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    qDebug("mouse press");
    if( event->button() == Qt::LeftButton ) {
        game_g->mouseClick(event->x(), event->y());
    }
}*/

void MainWindow::setOrientation(ScreenOrientation orientation)
{
#if defined(Q_OS_SYMBIAN)
    // If the version of Qt on the device is < 4.7.2, that attribute won't work
    if (orientation != ScreenOrientationAuto) {
        const QStringList v = QString::fromAscii(qVersion()).split(QLatin1Char('.'));
        if (v.count() == 3 && (v.at(0).toInt() << 16 | v.at(1).toInt() << 8 | v.at(2).toInt()) < 0x040702) {
            qWarning("Screen orientation locking only supported with Qt 4.7.2 and above");
            return;
        }
    }

    Qt::WidgetAttribute attribute;
    switch (orientation) {
#if QT_VERSION < 0x040702
    // Qt < 4.7.2 does not yet have the Qt::WA_*Orientation attributes
    case ScreenOrientationLockPortrait:
        attribute = static_cast<Qt::WidgetAttribute>(128);
        break;
    case ScreenOrientationLockLandscape:
        attribute = static_cast<Qt::WidgetAttribute>(129);
        break;
    default:
    case ScreenOrientationAuto:
        attribute = static_cast<Qt::WidgetAttribute>(130);
        break;
#else // QT_VERSION < 0x040702
    case ScreenOrientationLockPortrait:
        attribute = Qt::WA_LockPortraitOrientation;
        break;
    case ScreenOrientationLockLandscape:
        attribute = Qt::WA_LockLandscapeOrientation;
        break;
    default:
    case ScreenOrientationAuto:
        attribute = Qt::WA_AutoOrientation;
        break;
#endif // QT_VERSION < 0x040702
    };
    setAttribute(attribute, true);
#endif // Q_OS_SYMBIAN
}

void MainWindow::showExpanded(bool fullscreen)
{
/*#if defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR)
    showFullScreen();
#elif defined(Q_WS_MAEMO_5)
    showMaximized();
#else
    show();
#endif*/
    if( smallscreen_c || fullscreen ) {
        // smallscreen platforms always fullscreen
#ifdef Q_OS_ANDROID
        this->resize(QApplication::desktop()->availableGeometry().width(), QApplication::desktop()->availableGeometry().height()); // workaround for Android Qt 5 bug where windows open at 640x480?! See http://www.qtcentre.org/threads/55453-Android-screen-res-problem-(it-s-always-640x480-instead-of-maximized)-Qt-5-1-XP
#endif
        showFullScreen();
    }
    else {
        show();
    }
    LOG("window size %d x %d\n", this->width(), this->height());
}

#if 0
void MainWindow::updateScene() {
    //qDebug("MainWindow::updateScene()");
    //qApp->beep();
    /*if( this->signalsBlocked() ) {
        qDebug("no signals");
        return;
    }*/
    /*int new_time = game_g->getScreen()->getElapsedMS();
    int d_time = new_time - elapsed_time;*/
    //qDebug("time: %d", d_time);

    int new_elapsed_time_ms = game_g->getScreen()->getElapsedMS();
    if( paused ) {
        this->game_time_frame_ms = 0;
    }
    else {
        this->game_time_frame_ms = new_elapsed_time_ms - this->saved_elapsed_time_ms;
        this->game_time_total_ms += this->game_time_frame_ms;
    }
    this->saved_elapsed_time_ms = new_elapsed_time_ms;
    time_ms = this->game_time_frame_ms;

    game_g->update(game_time_frame_ms);

    //elapsed_time = new_time;
}
#endif

void MainWindow::closeEvent(QCloseEvent *event) {
    LOG("MainWindow received close event\n");
    if( game_g != NULL && game_g->getGamestate() != NULL ) {
        game_g->getGamestate()->quitGame();
        event->ignore();
    }
    else {
        event->accept();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    qDebug("mainwindow key press: %d", event->key());
    if( game_g != NULL && game_g->getScreen() != NULL ) {
        game_g->keyPress(event);
    }
}
