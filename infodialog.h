#pragma once

#include <QtGui>

#include <string>
using std::string;

#include <vector>
using std::vector;

class QWebView;

class InfoDialog : public QDialog {
    Q_OBJECT

    QWebView *label;
    vector<QPushButton *> buttons_list;

private slots:
    void clicked();

public:
    InfoDialog(const string &text, const vector<string> &buttons, bool horiz, bool small_buttons);
    virtual ~InfoDialog() {
    }

    static InfoDialog *createInfoDialogOkay(const string &text) {
        vector<string> buttons;
        buttons.push_back("Okay");
        return new InfoDialog(text, buttons, true, false);
    }
    static InfoDialog *createInfoDialogYesNo(const string &text) {
        vector<string> buttons;
        buttons.push_back("Yes");
        buttons.push_back("No");
        return new InfoDialog(text, buttons, true, false);
    }

    void scrollToBottom();
    virtual int exec();
};
