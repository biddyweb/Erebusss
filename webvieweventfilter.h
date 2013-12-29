#pragma once

#include <QTextEdit>
#include <QEvent>

#include "common.h"

class WebViewEventFilter : public QObject {
    Q_OBJECT

    QTextEdit *textEdit;
    bool filterMouseMove;
    int orig_mouse_x, orig_mouse_y;
    int saved_mouse_x, saved_mouse_y;
    int last_scroll_y;

protected:
    bool eventFilter(QObject *obj, QEvent *event);

public:
    explicit WebViewEventFilter(QObject *parent = 0) : QObject(parent),
        textEdit(NULL),
        filterMouseMove(false), orig_mouse_x(0), orig_mouse_y(0), saved_mouse_x(0), saved_mouse_y(0), last_scroll_y(-1) {
    }

    void setTextEdit(QTextEdit *textEdit);
};
