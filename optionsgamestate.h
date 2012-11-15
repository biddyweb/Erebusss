#pragma once

#include "common.h"

#include <QtGui>

#include "gamestate.h"

class ScrollingListWidget;
//class Sound;

class OptionsGamestate : public Gamestate {
    Q_OBJECT

    static OptionsGamestate *optionsGamestate; // singleton pointer, needed for static member functions

    QStackedWidget *main_stacked_widget;

    //QComboBox *difficultyComboBox;
    QButtonGroup *difficultyButtonGroup;

    ScrollingListWidget *load_list;

    QCheckBox *soundCheck;
    QCheckBox *lightingCheck;

/*#ifndef Q_OS_ANDROID
    Sound *music;
#endif*/

private slots:
    void clickedStart();
    void clickedStartGame();
    void clickedLoad();
    void clickedLoadGame();
    void clickedOptions();
    void clickedOptionsOkay();
    void clickedQuit();
    void clickedOnlineHelp();
    void clickedOfflineHelp();
    void closeAllSubWindows();
/*#ifndef Q_OS_ANDROID
    void mediaStateChanged(Phonon::State newstate, Phonon::State oldstate);
#endif*/

public:
    OptionsGamestate();
    virtual ~OptionsGamestate();

    virtual void quitGame();
    virtual void update() {
    }
};
