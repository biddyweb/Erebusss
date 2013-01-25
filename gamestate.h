#pragma once

#include "common.h"

#include <QObject>
#include <QKeyEvent>

class Gamestate : public QObject {
public:
    //virtual VI_Panel *getPanel()=0;
    virtual void quitGame()=0;
    virtual void update()=0;
    virtual void updateInput() {
    }
    virtual void render() {
    }
    virtual void activate(bool active) {
    }
    /*virtual void mouseClick(int m_x, int m_y) {
    }*/
    virtual void keyPress(QKeyEvent *key_event) {
    }
};

