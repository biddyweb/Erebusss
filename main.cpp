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

    Game game;
    game.run(argc, argv);
    //game.runTests();
    return 0;
}
