#pragma once

#include <QtGui>

#include <string>
using std::string;

#include <vector>
using std::vector;

class InfoDialog : public QDialog {
    Q_OBJECT

    vector<QPushButton *> buttons_list;

private slots:
    void clicked();

public:
    InfoDialog(const string &text, const vector<string> &buttons);
    virtual ~InfoDialog() {
    }

    static InfoDialog *createInfoDialogOkay(const string &text) {
        vector<string> buttons;
        buttons.push_back("Okay");
        return new InfoDialog(text, buttons);
    }
    static InfoDialog *createInfoDialogYesNo(const string &text) {
        vector<string> buttons;
        buttons.push_back("Yes");
        buttons.push_back("No");
        return new InfoDialog(text, buttons);
    }

    virtual int exec();
};
