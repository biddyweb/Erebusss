#include "mainwindow.h"
#include "qt_screen.h"
#include "game.h"
#include "logiface.h"

#include <QtGui/QApplication>

class MyApplication : public QApplication {

protected:
    bool event(QEvent *event);

public:
    MyApplication(int argc, char *argv[]) : QApplication(argc, argv) {
    }
};

bool MyApplication::event(QEvent *event) {
    /* Activate/deactivate events aren't received on Android when we switch applications. But worse, we seem
     * to receive these events on Android when we don't on Windows, e.g., when opening/closing dialogs, which
     * can lead to a bug where the game pauses unexpectedly (e.g., talk to an NPC, say at least one thing, then
     * close the talk window - the game will still be paused).
     * Also disabled for Linux, as we get an inactive message when the game first starts - probably best to
     * disable, until we understand what's happening.
     */
    if( event->type() == QEvent::ApplicationActivate ) {
#if !defined(Q_OS_ANDROID) && !defined(__linux)
        LOG("application activated\n");
        if( game_g != NULL && game_g->getScreen() != NULL ) {
            game_g->activate(true);
        }
#endif
    }
    else if( event->type() == QEvent::ApplicationDeactivate ) {
#if !defined(Q_OS_ANDROID) && !defined(__linux)
        LOG("application deactivated\n");
        if( game_g != NULL && game_g->getScreen() != NULL ) {
            game_g->activate(false);
        }
#endif
    }
    return QApplication::event(event);
}

int main(int argc, char *argv[])
{
    MyApplication app(argc, argv);
    {
        QDir path = QDir::currentPath();
        path.cd("plugins/");
        QApplication::addLibraryPath(path.absolutePath());
    }

    if( !smallscreen_c ) {
        // not full tested on mobile platforms (Android doesn't pick up device language settings anyway; need to ensure that strings aren't too long for Android/Symbian)
        //qDebug("current dir path: %s", QDir::currentPath().toStdString().c_str());
        // Translation
        QString locale = QLocale::system().name().section('_', 0, 0);
        //locale = QLocale(QLocale::French, QLocale::France).name().section('_', 0, 0); // test
        qDebug("locale: %s", locale.toStdString().c_str());
        QTranslator translator;
        if( !translator.load(DEPLOYMENT_PATH + QString("ts/erebus_") + locale) ) {
            qDebug("failed to load translator file");
        }
        app.installTranslator(&translator);
    }

    bool fullscreen = true;
    bool runtests = false;
    bool help = false;

    //fullscreen = false;
    //runtests = true;

#if !defined(Q_OS_ANDROID)
        // n.b., crashes when run on Galaxy Nexus (even though fine in the emulator)
    for(int i=0;i<argc;i++) {
        if( strcmp(argv[i], "-windowed") == 0 )
            fullscreen = false;
        else if( strcmp(argv[i], "-runtests") == 0 )
            runtests = true;
        else if( strcmp(argv[i], "-help") == 0 )
            help = true;
    }
#endif

    if( help ) {
        // n.b., won't show on Windows due to being a Windowed application
        printf("Erebus v%d.%d\n", versionMajor, versionMinor);
        printf("Available options:\n");
        printf("    -windowed - Run in windowed mode\n");
        printf("    -runtests - Run tests\n");
        printf("    -help     - Display this message\n");
        printf("Please see the file erebus.html in docs/ for full instructions.\n");
        return 0;
    }

    Game game;
    if( runtests ) {
        game.runTests();
    }
    else {
        game.run(fullscreen);
    }
    return 0;
}
