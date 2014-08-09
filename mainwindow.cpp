#include <QApplication>
#include <QDesktopWidget>

#include "mainwindow.h"
#include "qt_screen.h"
#include "game.h"
#include "gamestate.h"
#include "logiface.h"

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

void MainWindow::setOrientation(ScreenOrientation)
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
        // problem on Android Qt 5.x where neither showMaximized() nor showFullscreen() working
        // use desktop()->size rather than desktop()->availableGeometry (the latter returns 0x0 on Qt 5.3)
        this->resize(QApplication::desktop()->size());
        // prefer showMaximized(); showFullScreen() enables the new "immersive" mode which we don't yet want
        showMaximized();
#else
        showFullScreen();
#endif
    }
    else {
        show();
    }
    LOG("desktop size %d x %d\n", QApplication::desktop()->width(), QApplication::desktop()->height());
    LOG("desktop available size %d x %d\n", QApplication::desktop()->availableGeometry().width(), QApplication::desktop()->availableGeometry().height());
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
    if( game_g != NULL ) {
        // if there are already messages, either the user has already requested a quit (so don't want to pile up lots of quit dialogs), or we are changing gamestate and so may not be safe to quit now
        if( !game_g->hasMessages() )
            game_g->pushMessage(new GameMessage(GameMessage::GAMEMESSAGETYPE_QUIT));
        event->ignore();
    }
    else {
        event->accept();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    qDebug("mainwindow key press: %d", event->key());
#ifdef Q_OS_ANDROID
#if QT_VERSION >= 0x050000
    // Standard Android behaviour for back button is to send a quit signal, which works on Qt 4.x.
    // However in Qt 5.3.1, we need to ignore the Key_Back event for this to work this way (see http://qt-project.org/wiki/Qt_for_Android_known_issues )
    if( event->key() == Qt::Key_Back ) {
        event->ignore();
        return;
    }
#endif
#endif
    if( game_g != NULL && game_g->getScreen() != NULL ) {
        game_g->keyPress(event);
    }
}
