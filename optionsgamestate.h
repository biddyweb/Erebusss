#pragma once

#include "common.h"

#include <vector>
using std::vector;

#include <QtGui>

#include "gamestate.h"

class ScrollingListWidget;
//class Sound;

class OptionsGamestate : public Gamestate {
    Q_OBJECT

    static OptionsGamestate *optionsGamestate; // singleton pointer, needed for static member functions

    QStackedWidget *main_stacked_widget;

    QComboBox *characterComboBox;
    QComboBox *difficultyComboBox;
    //QButtonGroup *difficultyButtonGroup;
    QCheckBox *permadeathCheckBox;

    ScrollingListWidget *load_list;
    vector<QString> load_filenames;

    QCheckBox *soundCheck;
    QCheckBox *lightingCheck;

    bool cheat_mode;
    int cheat_start_level;

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
    virtual void keyPress(QKeyEvent *key_event);
};
