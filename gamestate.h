#pragma once

#include <QObject>

class Gamestate : public QObject {
public:
    //virtual VI_Panel *getPanel()=0;
    virtual void quitGame()=0;
    virtual void update()=0;
    /*virtual void mouseClick(int m_x, int m_y) {
    }*/
};

