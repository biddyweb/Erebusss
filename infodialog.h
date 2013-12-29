#pragma once

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <QDialog>
#include <QTextEdit>

#include "common.h"

class InfoDialog : public QDialog {
    Q_OBJECT

    QTextEdit *label;
    vector<QPushButton *> buttons_list;

private slots:
    void clicked();

public:
    InfoDialog(const string &text, const string &picture, const vector<string> &buttons, bool horiz, bool small_buttons, bool numbered_shortcuts);
    virtual ~InfoDialog() {
    }

    static InfoDialog *createInfoDialogOkay(const string &text, const string &picture);
    static InfoDialog *createInfoDialogOkay(const string &text);
    static InfoDialog *createInfoDialogYesNo(const string &text);

    void scrollToBottom();
    virtual int exec();

public slots:
    virtual void reject();
};
