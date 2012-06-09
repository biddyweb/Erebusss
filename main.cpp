#include "mainwindow.h"
#include "qt_screen.h"
#include "game.h"

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
        qDebug("application activated");
        if( game_g != NULL && game_g->getScreen() != NULL ) {
            game_g->getScreen()->enableUpdateTimer(true);
        }
    }
    else if( event->type() == QEvent::ApplicationDeactivate ) {
        qDebug("application deactivated");
        if( game_g != NULL && game_g->getScreen() != NULL ) {
            game_g->getScreen()->enableUpdateTimer(false);
            game_g->getScreen()->setPaused(true); // automatically pause when application goes inactive
        }
    }
    return QApplication::event(event);
}

int main(int argc, char *argv[])
{
    MyApplication app(argc, argv);

    Game game;
    game.run();
    return 0;
}
