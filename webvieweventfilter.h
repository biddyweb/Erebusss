#pragma once

#include <QTextEdit>
#include <QEvent>

#include "common.h"

#include "rpg/utils.h" // for Vector2D

class WebViewEventFilter : public QObject {
    Q_OBJECT

    QTextEdit *textEdit;
    bool filterMouseMove;
    int orig_mouse_x, orig_mouse_y;
    int saved_mouse_x, saved_mouse_y;
    int last_scroll_y;
    bool has_kinetic_scroll;
    Vector2D kinetic_scroll_dir;
    float kinetic_scroll_speed;

private slots:
    void textEditDestroyed(QObject *obj);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

public:
    explicit WebViewEventFilter(QObject *parent = 0) : QObject(parent),
        textEdit(NULL),
        filterMouseMove(false), orig_mouse_x(0), orig_mouse_y(0), saved_mouse_x(0), saved_mouse_y(0), last_scroll_y(-1),
        has_kinetic_scroll(false), kinetic_scroll_speed(0.0f) {
    }

    void setTextEdit(QTextEdit *textEdit);
    void updateInput();
};
