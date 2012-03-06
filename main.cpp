#include "mainwindow.h"
#include "game.h"

#include <QtGui/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    /*game_g = new Game();
    game_g->run();
    delete game_g;*/
    Game game;
    game.run();
    return 0;
}
