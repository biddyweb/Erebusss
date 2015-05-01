#pragma once

#include <QTextEdit>
#include <QEvent>
#include <QTimer>

#include "common.h"

#include "rpg/utils.h" // for Vector2D

class WebViewEventFilter : public QObject {
    Q_OBJECT

    QTimer timer;

    QTextEdit *textEdit;
    bool filterMouseMove;
    int orig_mouse_x, orig_mouse_y;
    int saved_mouse_x, saved_mouse_y;
    int saved_mouse_ms;
    int last_scroll_y;
    bool has_kinetic_scroll;
    Vector2D kinetic_scroll_dir;
    float kinetic_scroll_speed;

private slots:
    void textEditDestroyed(QObject *obj);
    void timerSlot();

protected:
    bool eventFilter(QObject *obj, QEvent *event);

public:
    explicit WebViewEventFilter(QObject *parent = 0);

    void setTextEdit(QTextEdit *textEdit);
};
