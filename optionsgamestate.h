#pragma once

#include "common.h"

#include <vector>
using std::vector;

#include <QStackedWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>

#ifdef USING_WEBKIT
#include <QWebView>
#else
#include <QTextEdit>
#endif

#include "gamestate.h"

class ScrollingListWidget;

class OptionsGamestate : public Gamestate {
    Q_OBJECT

    friend class CharacterHelp;

    static OptionsGamestate *optionsGamestate; // singleton pointer, needed for static member functions

    QStackedWidget *main_stacked_widget;

    int options_page_index;

    QComboBox *gametypeComboBox;
    QComboBox *characterComboBox;
    QLabel *portraitLabel;
    QComboBox *difficultyComboBox;
    //QButtonGroup *difficultyButtonGroup;
    QCheckBox *permadeathCheckBox;

    QLineEdit *nameLineEdit;

    ScrollingListWidget *load_list;
    vector<QString> load_filenames;

#ifndef Q_OS_ANDROID
    QSlider *soundSlider;
    QSlider *soundSliderMusic;
    QSlider *soundSliderEffects;
#endif
    QCheckBox *lightingCheck;

    bool cheat_mode;
    int cheat_start_level;

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

#ifdef USING_WEBKIT
class Help : public QWebView {
#else
class Help : public QTextEdit {
#endif
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
