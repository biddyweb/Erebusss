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

    static InfoDialog *createInfoDialogOkay(const string &text, const string &picture);
    static InfoDialog *createInfoDialogOkay(const string &text);
    static InfoDialog *createInfoDialogYesNo(const string &text);

    void scrollToBottom();
    virtual int exec();

public slots:
    virtual void reject();
};
