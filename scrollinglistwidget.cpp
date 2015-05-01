#include <QScrollBar>
#include <QMouseEvent>

#include "scrollinglistwidget.h"
#include "game.h"

const int update_interval_c = 16;

ScrollingListWidget::ScrollingListWidget(QWidget *parent) : QListWidget(parent), saved_mouse_x(0), saved_mouse_y(0), saved_mouse_ms(0), has_kinetic_scroll(false), kinetic_scroll_speed(0.0f) {
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
#ifdef Q_OS_ANDROID
    this->setStyleSheet("color: black; background-color: white"); // workaround for Android colour problem
#endif

    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timerSlot()));
    timer.start(update_interval_c);
}

void ScrollingListWidget::mouseMoveEvent(QMouseEvent *event) {
    //qDebug("ScrollingListWidget::mouseMoveEvent()");
    //QListWidget::mouseMoveEvent(event); // don't want to select items whilst dragging
    // n.b., scene scrolls in opposite direction to mouse movement
    int diff_x = saved_mouse_x - event->x();
    int diff_y = saved_mouse_y - event->y();
    QScrollBar *scroll_x =  this->horizontalScrollBar();
    scroll_x->setValue(scroll_x->value() + diff_x);
    saved_mouse_x = event->x();
    QScrollBar *scroll_y =  this->verticalScrollBar();
    scroll_y->setValue(scroll_y->value() + diff_y);
    saved_mouse_y = event->y();

    int time_ms = game_g->getElapsedMS() - saved_mouse_ms; // we don't use getElapsedFrameMS(), as we don't know how many times we have received mouse move event per frame
    saved_mouse_ms = game_g->getElapsedMS();

    qDebug("diff_y: %d", diff_y);
    if( abs(diff_y) > 0 ) {
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
    saved_mouse_x = event->x();
    saved_mouse_y = event->y();
    saved_mouse_ms = game_g->getElapsedMS();
    this->has_kinetic_scroll = false;
}

void ScrollingListWidget::timerSlot() {
    //qDebug("ScrollingListWidget::timerSlot()");
    if( this->has_kinetic_scroll ) {
        float step = update_interval_c*this->kinetic_scroll_speed;
        qDebug("kinetic scroll: %f", step);
        QScrollBar *scroll_y =  this->verticalScrollBar();
        scroll_y->setValue(scroll_y->value() + step);

        float decel = 0.001f * (float)update_interval_c;
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
