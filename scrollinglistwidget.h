#pragma once

#include <QListWidget>
#include <QTimer>

#include "common.h"

class ScrollingListWidget : public QListWidget {
    Q_OBJECT

    QTimer timer;

    int saved_mouse_x;
    int saved_mouse_y;
    int saved_mouse_ms;
    bool has_kinetic_scroll;
    float kinetic_scroll_speed;

    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);

private slots:
    void timerSlot();

public:
    explicit ScrollingListWidget(QWidget *parent = 0);
    virtual ~ScrollingListWidget() {
    }
};
