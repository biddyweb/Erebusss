#include <QMouseEvent>
#include <QScrollBar>
#include <QDesktopServices>
#include <QUrl>
#include <QTextStream>
#include <QApplication>

#include "webvieweventfilter.h"
#include "game.h"
#include "logiface.h"

const int update_interval_c = 16;

WebViewEventFilter::WebViewEventFilter(QObject *parent) : QObject(parent),
    textEdit(NULL),
    filterMouseMove(false), orig_mouse_x(0), orig_mouse_y(0), saved_mouse_x(0), saved_mouse_y(0), saved_mouse_ms(0), last_scroll_y(-1),
    has_kinetic_scroll(false), kinetic_scroll_speed(0.0f) {

    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(timerSlot()));
    timer.start(update_interval_c);
}

void WebViewEventFilter::setTextEdit(QTextEdit *textEdit) {
    qDebug("setTextEdit");
    if( this->textEdit != NULL ) {
        disconnect(this->textEdit, 0, this, 0);
    }
    this->textEdit = textEdit;
    this->textEdit->installEventFilter(this);
    //this->webView = NULL;
    this->filterMouseMove = false;
    this->orig_mouse_x = 0;
    this->orig_mouse_y = 0;
    this->saved_mouse_x = 0;
    this->saved_mouse_y = 0;
    this->saved_mouse_ms = 0;
    this->last_scroll_y = -1;

    connect(textEdit, SIGNAL(destroyed(QObject *)), this, SLOT(textEditDestroyed(QObject *)));
}

void WebViewEventFilter::textEditDestroyed(QObject *obj) {
    if( obj == this->textEdit ) {
        this->textEdit = NULL;
    }
}

// returns true to filter the event
bool WebViewEventFilter::eventFilter(QObject *, QEvent *event) {
    const int tolerance = 16;
    //qDebug("eventFilter type: %d", event->type());
    switch( event->type() ) {
        case QEvent::Timer:
            {
                // hack to fix problem where when drag-scrolling with mouse or touch, when mouse/pointer moves outside the textedit, it starts scrolling in the opposite direction
                return true;
                break;
            }
        case QEvent::MouseButtonPress:
            {
                //qDebug("MouseButtonPress");
                QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                if( mouseEvent->button() == Qt::LeftButton ) {
                    //filterMouseMove = true;
                    // disallow selection - but we need to allow click and drag for the scroll bars!
                    if( textEdit != NULL ) {
                        filterMouseMove = true;
                        orig_mouse_x = saved_mouse_x = mouseEvent->globalX();
                        orig_mouse_y = saved_mouse_y = mouseEvent->globalY();
                        saved_mouse_ms = game_g->getElapsedMS();
                        has_kinetic_scroll = false;
                    }
                }
                break;
            }
        case QEvent::MouseButtonRelease:
            {
                //qDebug("MouseButtonRelease");
                if( textEdit != NULL && last_scroll_y != -1 ) {
                    // fix problem where when drag-scrolling with mouse or touch, if mouse/pointer has moved outside the textedit, it scrolls back to original position when we let go
                    textEdit->verticalScrollBar()->setValue(last_scroll_y);
                }
                QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                if( mouseEvent->button() == Qt::LeftButton ) {
                    filterMouseMove = false;
                    // if we've clicked and dragged, we don't want to allow clicking links
                    int mouse_x = mouseEvent->globalX();
                    int mouse_y = mouseEvent->globalY();
                    int diff_x = mouse_x - orig_mouse_x;
                    int diff_y = mouse_y - orig_mouse_y;
                    int dist2 = diff_x*diff_x + diff_y*diff_y;
                    qDebug("drag %d, %d : dist2: %d", diff_x, diff_y, dist2);
                    if( dist2 > tolerance*tolerance ) {
                        // need to allow some tolerance, otherwise hard to click on a touchscreen device!
                        return true;
                    }
                    else {
                        // if we haven't moved far, then don't do kinetic scrolling
                        this->has_kinetic_scroll = false;
                    }
                }
                if( textEdit != NULL ) {
                    // need to handle links manually!
                    // problem that Qt::LinksAccessibleByMouse messes up the drag scrolling, so have to use Qt::NoTextInteraction
                    QString url = textEdit->anchorAt(mouseEvent->pos());
                    if( url.length() > 0 ) {
                        LOG("textEdit: clicked on: %s\n", url.toStdString().c_str());
                        if( url.at(0) == '#' ) {
                            last_scroll_y = -1;
                            textEdit->scrollToAnchor(url.mid(1));
                        }
                        else if( url.contains("://") ) {
                            QDesktopServices::openUrl(url);
                        }
                        else {
                            QFile file(QString(DEPLOYMENT_PATH) + "docs/" + url);
                            if( file.open(QFile::ReadOnly | QFile::Text) ) {
                                QTextStream in(&file);
                                textEdit->setHtml(in.readAll());
                            }
                            else {
                                LOG("failed to load: %s\n", url.toStdString().c_str());
                            }
                        }
                    }
                }
                break;
            }
        case QEvent::MouseMove:
            {
                //qDebug("MouseMove");
                /*if( filterMouseMove && webView != NULL ) {
                    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                    int scrollbar_x = webView->page()->mainFrame()->scrollBarGeometry(Qt::Vertical).left();
                    int scrollbar_y = webView->page()->mainFrame()->scrollBarGeometry(Qt::Horizontal).top();
                    if( mouseEvent->x() <= scrollbar_x && mouseEvent->y() <= scrollbar_y ) {
                        // filter
                        return true;
                    }
                }*/
                if( filterMouseMove ) {
                    //if( webView != NULL || textEdit != NULL ) {
                    if( textEdit != NULL ) {
                        // support for swipe scrolling
                        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                        int new_mouse_x = mouseEvent->globalX();
                        int new_mouse_y = mouseEvent->globalY();
                        //qDebug("mouse %d, %d", new_mouse_x, new_mouse_y);
                        if( textEdit != NULL ){
                            qDebug("scroll %d, %d", saved_mouse_x - new_mouse_x, saved_mouse_y - new_mouse_y);
                            int value = textEdit->verticalScrollBar()->value();
                            value += saved_mouse_y - new_mouse_y;
                            if( value < textEdit->verticalScrollBar()->minimum() )
                                value = textEdit->verticalScrollBar()->minimum();
                            if( value > textEdit->verticalScrollBar()->maximum() )
                                value = textEdit->verticalScrollBar()->maximum();
                            textEdit->verticalScrollBar()->setValue(value);
                            //qDebug("    value is now %d", value);
                            last_scroll_y = value;

                            value = textEdit->horizontalScrollBar()->value();
                            value += saved_mouse_x - new_mouse_x;
                            if( value < textEdit->horizontalScrollBar()->minimum() )
                                value = textEdit->horizontalScrollBar()->minimum();
                            if( value > textEdit->horizontalScrollBar()->maximum() )
                                value = textEdit->horizontalScrollBar()->maximum();
                            textEdit->horizontalScrollBar()->setValue(value);
                            //qDebug("    value is now %d", value);
                        }

                        QPointF old_pos(this->saved_mouse_x, this->saved_mouse_y);
                        QPointF new_pos(new_mouse_x, new_mouse_y);
                        QPointF diff = old_pos - new_pos; // n.b., scene scrolls in opposite direction to mouse movement
                        qDebug("    diff: %f, %f", diff.x(), diff.y());
                        if( fabs(diff.x()) > 0 || fabs(diff.y()) > 0 ) {
                            int time_ms = game_g->getElapsedMS() - saved_mouse_ms; // we don't use getElapsedFrameMS(), as we don't know how many times we have received mouse move event per frame
                            qDebug("    time_ms: %d", time_ms);
                            if( time_ms > 0 ) {
                                diff /= (float)time_ms;
                                this->has_kinetic_scroll = true;
                                this->kinetic_scroll_dir.set(diff.x(), diff.y());
                                this->kinetic_scroll_speed = this->kinetic_scroll_dir.magnitude();
                                this->kinetic_scroll_dir /= this->kinetic_scroll_speed;
                                //this->kinetic_scroll_speed = std::min(this->kinetic_scroll_speed, 1.0f);
                                qDebug("    speed: %f", this->kinetic_scroll_speed);
                            }
                        }
                        else {
                            this->has_kinetic_scroll = false;
                        }

                        saved_mouse_x = new_mouse_x;
                        saved_mouse_y = new_mouse_y;
                        saved_mouse_ms = game_g->getElapsedMS();
                    }
                    return true;
                }
            }
            break;
        case QEvent::MouseButtonDblClick:
            return true;
            break;
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            last_scroll_y = -1;
            break;
        default:
            break;
    }
    return false;
}

void WebViewEventFilter::timerSlot() {
    if( (qApp->mouseButtons() & Qt::LeftButton) == 0 && has_kinetic_scroll && textEdit != NULL ) {
        int time_ms = game_g->getElapsedFrameMS();
        //qDebug("centre was: %f, %f", centre.x(), centre.y());
        float step = time_ms*this->kinetic_scroll_speed;
        qDebug("kinetic scroll: %f", step);
        //float move_x = step * this->kinetic_scroll_dir.x;
        float move_y = step * this->kinetic_scroll_dir.y;
        qDebug("    move_y: %f", move_y);

        int value_y = textEdit->verticalScrollBar()->value();
        value_y += (int)move_y;
        if( value_y < textEdit->verticalScrollBar()->minimum() )
            value_y = textEdit->verticalScrollBar()->minimum();
        if( value_y > textEdit->verticalScrollBar()->maximum() )
            value_y = textEdit->verticalScrollBar()->maximum();
        textEdit->verticalScrollBar()->setValue(value_y);

        float decel = 0.002f * (float)time_ms;
        this->kinetic_scroll_speed -= decel;
        if( this->kinetic_scroll_speed <= 0.0f ) {
            this->has_kinetic_scroll = false;
            this->kinetic_scroll_speed = 0.0f;
        }
        //qDebug("    kinetic is now %f, %f", this->kinetic_scroll_x, this->kinetic_scroll_y);
    }
}
