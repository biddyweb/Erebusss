#include <QScrollBar>
#include <QMouseEvent>

#include "scrollinglistwidget.h"
#include "game.h"

ScrollingListWidget::ScrollingListWidget(QWidget *parent) : QListWidget(parent), saved_x(0), saved_y(0), has_kinetic_scroll(false), kinetic_scroll_speed(0.0f) {
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
#ifdef Q_OS_ANDROID
    this->setStyleSheet("color: black; background-color: white"); // workaround for Android colour problem
#endif

    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timerSlot()));
    timer.start(16);
}

const int drag_tol_c = 0;

void ScrollingListWidget::mouseMoveEvent(QMouseEvent *event) {
    //qDebug("ScrollingListWidget::mouseMoveEvent()");
    //QListWidget::mouseMoveEvent(event); // don't want to select items whilst dragging
    // n.b., scene scrolls in opposite direction to mouse movement
    int diff_x = saved_x - event->x();
    int diff_y = saved_y - event->y();
    QScrollBar *scroll_x =  this->horizontalScrollBar();
    scroll_x->setValue(scroll_x->value() + diff_x);
    saved_x = event->x();
    QScrollBar *scroll_y =  this->verticalScrollBar();
    scroll_y->setValue(scroll_y->value() + diff_y);
    saved_y = event->y();

    qDebug("diff_y: %d", diff_y);
    if( abs(diff_y) > drag_tol_c ) {
        int time_ms = game_g->getElapsedFrameMS();
        qDebug("time_ms: %d", time_ms);
        if( time_ms > 0 ) {
            this->has_kinetic_scroll = true;
            this->kinetic_scroll_speed = ((float)diff_y) / (float)time_ms;
            qDebug("    speed: %f", this->kinetic_scroll_speed);
            this->kinetic_scroll_speed = std::min(this->kinetic_scroll_speed, 1.0f);
            this->kinetic_scroll_speed = std::max(this->kinetic_scroll_speed, -1.0f);
        }
    }
    else {
        this->has_kinetic_scroll = false;
    }
}

void ScrollingListWidget::mousePressEvent(QMouseEvent *event) {
    //qDebug("ScrollingListWidget::mousePressEvent()");
    QListWidget::mousePressEvent(event);
    saved_x = event->x();
    saved_y = event->y();
    this->has_kinetic_scroll = false;
}

void ScrollingListWidget::timerSlot() {
    //qDebug("ScrollingListWidget::timerSlot()");
    if( this->has_kinetic_scroll ) {
        int time_ms = game_g->getElapsedFrameMS();
        float step = time_ms*this->kinetic_scroll_speed;
        qDebug("kinetic scroll: %f", step);
        QScrollBar *scroll_y =  this->verticalScrollBar();
        scroll_y->setValue(scroll_y->value() + step);

        float decel = 0.001f * (float)time_ms;
        if( this->kinetic_scroll_speed > 0.0f ) {
            this->kinetic_scroll_speed -= decel;
            if( this->kinetic_scroll_speed <= 0.0f ) {
                this->has_kinetic_scroll = false;
                this->kinetic_scroll_speed = 0.0f;
            }
        }
        else {
            this->kinetic_scroll_speed += decel;
            if( this->kinetic_scroll_speed >= 0.0f ) {
                this->has_kinetic_scroll = false;
                this->kinetic_scroll_speed = 0.0f;
            }
        }
    }
}
