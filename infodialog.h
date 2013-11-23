#pragma once

#include "common.h"

#include <QDialog>

#include <string>
using std::string;

#include <vector>
using std::vector;

#ifdef USING_WEBKIT
class QWebView;
#else
class QTextEdit;
#endif

class InfoDialog : public QDialog {
    Q_OBJECT

#ifdef USING_WEBKIT
    QWebView *label;
#else
    QTextEdit *label;
#endif
    vector<QPushButton *> buttons_list;

private slots:
    void clicked();

public:
    InfoDialog(const string &text, const string &picture, const vector<string> &buttons, bool horiz, bool small_buttons, bool numbered_shortcuts);
    virtual ~InfoDialog() {
    }

    static InfoDialog *createInfoDialogOkay(const string &text, const string &picture) {
        vector<string> buttons;
        buttons.push_back("Okay");
        return new InfoDialog(text, picture, buttons, true, false, false);
    }
    static InfoDialog *createInfoDialogOkay(const string &text) {
        return createInfoDialogOkay(text, "");
    }
    static InfoDialog *createInfoDialogYesNo(const string &text) {
        vector<string> buttons;
        buttons.push_back("Yes");
        buttons.push_back("No");
        return new InfoDialog(text, "", buttons, true, false, false);
    }

    void scrollToBottom();
    virtual int exec();

public slots:
    virtual void reject();
};
