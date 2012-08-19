#pragma once

#include <QtGui>

#include <string>
using std::string;

#include <vector>
using std::vector;

class InfoDialog : public QDialog {
    Q_OBJECT

    QTextEdit *label;
    vector<QPushButton *> buttons_list;

private slots:
    void clicked();

public:
    InfoDialog(const string &text, const vector<string> &buttons, bool horiz);
    virtual ~InfoDialog() {
    }

    static InfoDialog *createInfoDialogOkay(const string &text) {
        vector<string> buttons;
        buttons.push_back("Okay");
        return new InfoDialog(text, buttons, true);
    }
    static InfoDialog *createInfoDialogYesNo(const string &text) {
        vector<string> buttons;
        buttons.push_back("Yes");
        buttons.push_back("No");
        return new InfoDialog(text, buttons, true);
    }

    void scrollToBottom();
    virtual int exec();
};
