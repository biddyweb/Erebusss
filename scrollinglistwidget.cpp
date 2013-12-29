#include <QScrollBar>
#include <QMouseEvent>

#include "scrollinglistwidget.h"

ScrollingListWidget::ScrollingListWidget(QWidget *parent) : QListWidget(parent), saved_x(0), saved_y(0) {
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
#ifdef Q_OS_ANDROID
    this->setStyleSheet("color: black; background-color: white"); // workaround for Android colour problem
#endif
}

void ScrollingListWidget::mouseMoveEvent(QMouseEvent *event) {
    qDebug("ScrollingListWidget::mouseMoveEvent()");
    //QListWidget::mouseMoveEvent(event); // don't want to select items whilst dragging
    QScrollBar *scroll_x =  this->horizontalScrollBar();
    scroll_x->setValue(scroll_x->value() - event->x() + saved_x);
    saved_x = event->x();
    QScrollBar *scroll_y =  this->verticalScrollBar();
    scroll_y->setValue(scroll_y->value() - event->y() + saved_y);
    saved_y = event->y();
}

void ScrollingListWidget::mousePressEvent(QMouseEvent *event) {
    qDebug("ScrollingListWidget::mousePressEvent()");
    QListWidget::mousePressEvent(event);
    saved_x = event->x();
    saved_y = event->y();
}
