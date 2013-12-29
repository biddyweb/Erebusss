#pragma once

#include <QListWidget>

#include "common.h" // just to be safe for consistency, and ensure we get platform #defines...

class ScrollingListWidget : public QListWidget {
    Q_OBJECT

    int saved_x;
    int saved_y;

    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
public:
    explicit ScrollingListWidget(QWidget *parent = 0);
    virtual ~ScrollingListWidget() {
    }
};
