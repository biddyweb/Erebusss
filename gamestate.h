#pragma once

#include <QObject>
#include <QKeyEvent>

#include "common.h"

class Gamestate : public QObject {
public:
    //virtual VI_Panel *getPanel()=0;
    virtual void quitGame()=0;
    virtual void update()=0;
    virtual void updateInput() {
    }
    virtual void render() {
    }
    virtual void activate(bool, bool) {
    }
    /*virtual void mouseClick(int m_x, int m_y) {
    }*/
    virtual void keyPress(QKeyEvent *) {
    }
};

