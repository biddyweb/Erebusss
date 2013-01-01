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
    if( event->type() == QEvent::ApplicationActivate ) {
        LOG("application activated\n");
        if( game_g != NULL && game_g->getScreen() != NULL ) {
            game_g->activate(true);
        }
    }
    else if( event->type() == QEvent::ApplicationDeactivate ) {
        LOG("application deactivated\n");
        if( game_g != NULL && game_g->getScreen() != NULL ) {
            game_g->activate(false);
        }
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

    bool fullscreen = true;
    bool help = false;

    //fullscreen = false;

#if !defined(Q_OS_ANDROID)
        // n.b., crashes when run on Galaxy Nexus (even though fine in the emulator)
    for(int i=0;i<argc;i++) {
        if( strcmp(argv[i], "-windowed") == 0 )
            fullscreen = false;
        else if( strcmp(argv[i], "-help") == 0 )
            help = true;
    }
#endif

    if( help ) {
        // n.b., won't show on Windows due to being a Windowed application
        printf("Erebus v%d.%d\n", versionMajor, versionMinor);
        printf("Available options:\n");
        printf("    -windowed - Run in windowed mode\n");
        printf("    -help     - Display this message\n");
        printf("Please see the file erebus.html in docs/ for full instructions.\n");
        return 0;
    }

    Game game;
    game.run(fullscreen);
    //game.runTests();
    return 0;
}
