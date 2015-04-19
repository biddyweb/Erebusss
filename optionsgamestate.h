#pragma once

#include <vector>
using std::vector;

#include <QStackedWidget>
#include <QComboBox>
#include <QLabel>
#include <QTextEdit>

#include "common.h"

#include "gamestate.h"
#include "scrollinglistwidget.h"

class OptionsGamestate : public Gamestate {
    Q_OBJECT

    friend class CharacterHelp;

    static OptionsGamestate *optionsGamestate; // singleton pointer, needed for static member functions

    QStackedWidget *main_stacked_widget;

    int options_page_index;
    int n_options_pages;

    QComboBox *gametypeComboBox;
    QComboBox *characterComboBox;
    QLabel *portraitLabel;
    QComboBox *difficultyComboBox;
    //QButtonGroup *difficultyButtonGroup;
    QComboBox *permadeathComboBox;
    QComboBox *firstQuestComboBox;

    QLineEdit *nameLineEdit;

    ScrollingListWidget *load_list;
    vector<QString> load_filenames;

#ifndef Q_OS_ANDROID
    QSlider *soundSlider;
    QSlider *soundSliderMusic;
    QSlider *soundSliderEffects;
#endif
    QComboBox *lightingComboBox;

    void cleanup();

/*#ifndef Q_OS_ANDROID
    Sound *music;
#endif*/

private slots:
    void clickedStart();
    void clickedStartGame();
    void clickedNext();
    void clickedCancel();
    void clickedLoad();
    void clickedLoadGame();
    void clickedImportButton(); // although this is only used on Android, shouldn't be under an Android #define, as then we can't find the slot for some reason
    void clickedExportButton(); // although this is only used on Android, shouldn't be under an Android #define, as then we can't find the slot for some reason
    void clickedOptions();
    void clickedOptionsOkay();
    void clickedQuit();
    void clickedOnlineHelp();
    void clickedOfflineHelp();
    void closeSubWindow();
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

class Help : public QTextEdit {
    Q_OBJECT

protected:
    OptionsGamestate *options_gamestate;

public:
    Help(OptionsGamestate *options_gamestate);

public slots:
    virtual void changedGameType(int index)=0;
};

class GameTypeHelp : public Help {
    Q_OBJECT
public:
    GameTypeHelp(OptionsGamestate *options_gamestate);

public slots:
    virtual void changedGameType(int index);
};

class CharacterHelp : public Help {
    Q_OBJECT
public:
    CharacterHelp(OptionsGamestate *options_gamestate);

public slots:
    virtual void changedGameType(int index);
};
