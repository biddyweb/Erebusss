#ifdef _DEBUG
#include <cassert>
#endif

#include <QApplication>

#include "maingraphicsview.h"
#include "playinggamestate.h"
#include "game.h"
#include "logiface.h"

const float MainGraphicsView::min_zoom_c = 10.0f;
const float MainGraphicsView::max_zoom_c = 200.0f;

TextEffect::TextEffect(MainGraphicsView *view, const QString &text, int duration_ms, const QColor &color) :
    QGraphicsTextItem(text), time_expire(0), view(view) {

    this->setDefaultTextColor(color);
    this->time_expire = game_g->getGameTimeTotalMS() + duration_ms;
    //this->setFont(game_g->getFontStd());
    //this->setFont(game_g->getFontSmall());
    this->setFont(game_g->getFontScene());

    {
        // centre alignment - see http://www.cesarbs.org/blog/2011/05/30/aligning-text-in-qgraphicstextitem/
        this->setTextWidth(this->boundingRect().width());
        QTextBlockFormat format;
        format.setAlignment(Qt::AlignCenter);
        QTextCursor cursor = this->textCursor();
        cursor.select(QTextCursor::Document);
        cursor.mergeBlockFormat(format);
        cursor.clearSelection();
        this->setTextCursor(cursor);
    }

    this->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
}

void TextEffect::advance(int phase) {
    if( phase == 0 ) {
        if( game_g->getGameTimeTotalMS() >= time_expire ) {
            this->view->removeTextEffect(this);
            this->deleteLater();
        }
    }
}

MainGraphicsView::MainGraphicsView(PlayingGamestate *playing_gamestate, QGraphicsScene *scene, QWidget *parent) :
    QGraphicsView(scene, parent), playing_gamestate(playing_gamestate), mouse_down_x(0), mouse_down_y(0), single_left_mouse_down(false), has_last_mouse(false), last_mouse_x(0), last_mouse_y(0), has_kinetic_scroll(false), kinetic_scroll_speed(0.0f),
    /*gui_overlay_item(NULL),*/ gui_overlay(NULL), c_scale(1.0f), calculated_lighting_pixmap(false), calculated_lighting_pixmap_scaled(false), lasttime_calculated_lighting_pixmap_scaled_ms(0), darkness_alpha(0), fps_frame_count(0),
    has_new_center_on(false)
{
    this->fps_timer.invalidate();
    this->resetKeyboard();
    this->grabKeyboard();
}

void MainGraphicsView::resetKeyboard() {
    for(int i=0;i<N_KEYS;i++) {
        this->key_down[i] = false;
    }
}

void MainGraphicsView::zoomOut() {
    playing_gamestate->zoomoutButton->clearFocus(); // workaround for Android still showing selection
    QPointF zoom_centre = this->getCenter();
    this->zoom(zoom_centre, false);
}

void MainGraphicsView::zoomIn() {
    playing_gamestate->zoominButton->clearFocus(); // workaround for Android still showing selection
    QPointF zoom_centre = this->getCenter();
    this->zoom(zoom_centre, true);
}

void MainGraphicsView::centreOnPlayer() {
    playing_gamestate->centreButton->clearFocus(); // workaround for Android still showing selection
    Character *character = playing_gamestate->getPlayer();
    AnimatedObject *object = static_cast<AnimatedObject *>(character->getListenerData());
    this->centerOn(object);
    this->has_new_center_on = false;
}

QPointF MainGraphicsView::getCenter() const {
    if( has_new_center_on )
        return new_center_on;
    QPointF center = this->mapToScene( this->rect() ).boundingRect().center();
    return center;
}

void MainGraphicsView::zoom(QPointF zoom_centre, bool in) {
    qDebug("MainGraphicsView::zoom(%d)", in);
    const float factor_c = 1.1f;
    if( in ) {
        float n_scale = c_scale * factor_c;
        this->setScale(zoom_centre, n_scale);
    }
    else {
        float n_scale = c_scale / factor_c;
        this->setScale(zoom_centre, n_scale);
    }
}

void MainGraphicsView::mousePress(int m_x, int m_y) {
    this->single_left_mouse_down = true;
    this->mouse_down_x = m_x;
    this->mouse_down_y = m_y;
    this->has_kinetic_scroll = false;

    this->has_last_mouse = true;
    this->last_mouse_x = m_x;
    this->last_mouse_y = m_y;
}

// On a touchscreen phone, it's very hard to press and release without causing a drag, so need to allow some tolerance!
// Needs to be higher on Symbian, at least for Nokia 5800, as touching the display seems to cause it to move so much more easily.
#if defined(Q_OS_SYMBIAN)
const int drag_tol_c = 24;
#else
const int drag_tol_c = 16;
#endif

void MainGraphicsView::mouseRelease(int m_x, int m_y) {
    this->single_left_mouse_down = false;
    this->has_last_mouse = false;
    int xdist = abs(this->mouse_down_x - m_x);
    int ydist = abs(this->mouse_down_y - m_y);
    //if( m_x == this->mouse_down_x && m_y == this->mouse_down_y ) {
    if( xdist <= drag_tol_c && ydist <= drag_tol_c ) {
        this->has_kinetic_scroll = false;
        QPointF m_scene = this->mapToScene(m_x, m_y);
        LOG("clicked: %f, %f\n", m_scene.x(), m_scene.y());
        playing_gamestate->clickedMainView(m_scene.x(), m_scene.y());
    }
    // else, this was a drag operation
}

void MainGraphicsView::mouseMove(int m_x, int m_y) {
    if( this->single_left_mouse_down ) {
        // only if the left mouse is down, and this isn't a multitouch event (otherwise single_left_mouse_down should be set to false)
        //qDebug("    lmb");
        if( this->has_last_mouse ) {
            //qDebug("    has last mouse");
            /*QPointF old_pos = this->mapToScene(this->last_mouse_x, this->last_mouse_y);
            QPointF new_pos = this->mapToScene(event->x(), event->y());*/
            //qDebug("%d, %d -> %d, %d", last_mouse_x, last_mouse_y, m_x, m_y);
            QPointF old_pos(this->last_mouse_x, this->last_mouse_y);
            QPointF new_pos(m_x, m_y);
            QPointF diff = old_pos - new_pos; // n.b., scene scrolls in opposite direction to mouse movement

            // drag - we do this ourselves rather than using drag mode QGraphicsView::ScrollHandDrag, to avoid conflict with multitouch
            QPointF centre = this->getCenter();
            QPoint centre_pixels = this->mapFromScene(centre);
            centre_pixels.setX( centre_pixels.x() + diff.x() );
            centre_pixels.setY( centre_pixels.y() + diff.y() );
            centre = this->mapToScene(centre_pixels);
            //this->centerOn(centre);
            // have problems on Android with Qt 5.3 if we try to call centerOn() directly from an event callback (the screen doesn't update during the touch operation)
            this->has_new_center_on = true;
            this->new_center_on = centre;
            //qDebug("        post centre on: %f, %f", new_center_on.x(), new_center_on.y());

            // need to check against drag_tol_c, otherwise simply clicking can cause us to move with kinetic motion (at least on Android)
            if( fabs(diff.x()) > drag_tol_c || fabs(diff.y()) > drag_tol_c ) {
                int time_ms = game_g->getInputTimeFrameMS();
                if( time_ms > 0 ) {
                    diff /= (float)time_ms;
                    this->has_kinetic_scroll = true;
                    this->kinetic_scroll_dir.set(diff.x(), diff.y());
                    this->kinetic_scroll_speed = this->kinetic_scroll_dir.magnitude();
                    this->kinetic_scroll_dir /= this->kinetic_scroll_speed;
                    this->kinetic_scroll_speed = std::min(this->kinetic_scroll_speed, 1.0f);
                    //qDebug("    speed: %f", this->kinetic_scroll_speed);
                }
            }
            else {
                this->has_kinetic_scroll = false;
            }
            //qDebug("    has last mouse done");
        }
        else {
            this->has_kinetic_scroll = false;
        }
        this->has_last_mouse = true;
        this->last_mouse_x = m_x;
        this->last_mouse_y = m_y;
    }
}

void MainGraphicsView::processTouchEvent(QTouchEvent *touchEvent) {
    QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
    //qDebug("touch event %d count %d", event->type(), touchPoints.count());
    if( touchPoints.count() > 1 ) {
        this->has_kinetic_scroll = false;
        this->single_left_mouse_down = false;
        // can't trust last mouse position if more that one touch!
        this->has_last_mouse = false;
    }
#if QT_VERSION >= 0x050000
#ifdef Q_OS_ANDROID
    // on Android with Qt 5, mouse events are never received, instead it's done purely via touch events
    if( touchPoints.count() == 1 ) {
        QTouchEvent::TouchPoint touchPoint = touchPoints.at(0);
        int m_x = touchPoint.pos().x();
        int m_y = touchPoint.pos().y();
        if( touchEvent->type() == QEvent::TouchBegin ) {
            this->mousePress(m_x, m_y);
        }
        else if( touchEvent->type() == QEvent::TouchEnd ) {
            this->mouseRelease(m_x, m_y);
        }
        else if( touchEvent->type() == QEvent::TouchUpdate ) {
            this->mouseMove(m_x, m_y);
        }
    }
#endif
#endif
    if( touchPoints.count() == 2 ) {
        // determine scale factor
        const QTouchEvent::TouchPoint &touchPoint0 = touchPoints.first();
        const QTouchEvent::TouchPoint &touchPoint1 = touchPoints.last();
        /*float scale_factor =
                QLineF(touchPoint0.pos(), touchPoint1.pos()).length()
                / QLineF(touchPoint0.startPos(), touchPoint1.startPos()).length();*/
        QPointF touch_centre = (touchPoint0.pos() + touchPoint1.pos())*0.5;
        QPointF zoom_centre = this->mapToScene(QPoint(touch_centre.x(), touch_centre.y()));
        float scale_factor =
                QLineF(touchPoint0.pos(), touchPoint1.pos()).length()
                / QLineF(touchPoint0.lastPos(), touchPoint1.lastPos()).length();
        /*if (touchEvent->touchPointStates() & Qt::TouchPointReleased) {
            // if one of the fingers is released, remember the current scale
            // factor so that adding another finger later will continue zooming
            // by adding new scale factor to the existing remembered value.
            totalScaleFactor *= currentScaleFactor;
            currentScaleFactor = 1;
        }*/
        /*setTransform(QTransform().scale(totalScaleFactor * currentScaleFactor,
                                        totalScaleFactor * currentScaleFactor));*/
        float n_scale = c_scale *scale_factor;
        LOG("multitouch scale: %f : %f\n", scale_factor, n_scale);
        this->setScale(zoom_centre, n_scale);
    }
}

bool MainGraphicsView::viewportEvent(QEvent *event) {
    //qDebug("MainGraphicsView::viewportEvent() type %d\n", event->type());
    // multitouch done by touch events manually - gestures don't seem to work properly on my Android phone?
    if( event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd ) {
        QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
        processTouchEvent(touchEvent);
        return true;
    }
    /*else if( event->type() == QEvent::Gesture ) {
        LOG("MainGraphicsView received gesture\n");
        //throw string("received gesture");
        QGestureEvent *gesture = static_cast<QGestureEvent*>(event);
        QPinchGesture *pinch = static_cast<QPinchGesture *>(gesture->gesture(Qt::PinchGesture));
        if( pinch != NULL ) {
            LOG("    zoom\n");
            LOG("    scale factor %f\n", pinch->scaleFactor());
            float n_scale = c_scale * pinch->scaleFactor();
            this->setScale(n_scale);
            return true;
        }
    }*/
    return QGraphicsView::viewportEvent(event);
}

void MainGraphicsView::mousePressEvent(QMouseEvent *event) {
    //qDebug("MainGraphicsView::mousePressEvent");
    if( event->button() & Qt::LeftButton ) {
        this->mousePress(event->x(), event->y());
    }

    QGraphicsView::mousePressEvent(event);
}

void MainGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    //qDebug("MainGraphicsView::mouseReleaseEvent");
    if( event->button() & Qt::LeftButton ) {
        mouseRelease(event->x(), event->y());
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void MainGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    //qDebug("MainGraphicsView::mouseMoveEvent");
#if QT_VERSION >= 0x050000
    QPointF new_pos = event->localPos();
#else
    QPointF new_pos = event->posF();
#endif
    this->mouseMove(new_pos.x(), new_pos.y());

    QGraphicsView::mouseMoveEvent(event);
}

void MainGraphicsView::wheelEvent(QWheelEvent *event) {
    if( !touchscreen_c ) {
        // mobile UI needs to be done via multitouch instead
        QPointF zoom_centre = this->mapToScene( this->mapFromGlobal( QCursor::pos() ) );
        if( event->delta() > 0 ) {
            this->zoom(zoom_centre, true);
        }
        else if( event->delta() < 0 ) {
            this->zoom(zoom_centre, false);
        }
    }
}

bool MainGraphicsView::handleKey(const QKeyEvent *event, bool down) {
    bool done = false;

    switch( event->key() ) {
    case Qt::Key_4:
    case Qt::Key_Left:
    case Qt::Key_A:
        this->key_down[KEY_L] = down;
        done = true;
        break;
    case Qt::Key_6:
    case Qt::Key_Right:
    case Qt::Key_D:
        this->key_down[KEY_R] = down;
        done = true;
        break;
    case Qt::Key_8:
    case Qt::Key_Up:
    case Qt::Key_W:
        this->key_down[KEY_U] = down;
        done = true;
        break;
    case Qt::Key_2:
    case Qt::Key_Down:
    case Qt::Key_S:
        this->key_down[KEY_D] = down;
        done = true;
        break;
    case Qt::Key_7:
        this->key_down[KEY_LU] = down;
        done = true;
        break;
    case Qt::Key_9:
        this->key_down[KEY_RU] = down;
        done = true;
        break;
    case Qt::Key_1:
        this->key_down[KEY_LD] = down;
        done = true;
        break;
    case Qt::Key_3:
        this->key_down[KEY_RD] = down;
        done = true;
        break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        done = true;
        if( down ) {
            this->playing_gamestate->actionCommand(false);
        }
        break;
    case Qt::Key_U:
        done = true;
        if( down ) {
            this->playing_gamestate->actionCommand(true);
        }
        break;
    case Qt::Key_PageDown:
    case Qt::Key_PageUp:
        // we don't need to record these keys, but we set done to true, so that it isn't processed by the QGraphicsView
        done = true;
        break;
    }

    return done;
}

void MainGraphicsView::keyPressEvent(QKeyEvent *event) {
    //qDebug("MainGraphicsView::keyPressEvent: %d", event->key());
    if( game_g->isPaused() && event->key() != Qt::Key_Control && event->key() != Qt::Key_Alt ) {
        // Qt::Key_Control, Qt::Key_Alt produced when doing multitouch zoom in/out for some reason?!
        //qDebug("unpause");
        game_g->setPaused(false, false);
    }

    bool done = handleKey(event, true);
    if( !done ) {
        QGraphicsView::keyPressEvent(event);
    }
}

void MainGraphicsView::keyReleaseEvent(QKeyEvent *event) {
    //qDebug("MainGraphicsView::keyReleaseEvent: %d", event->key());
    bool done = handleKey(event, false);
    if( !done ) {
        QGraphicsView::keyReleaseEvent(event);
    }
}

void MainGraphicsView::resizeEvent(QResizeEvent *event) {
    LOG("MainGraphicsView resized to: %d, %d\n", event->size().width(), event->size().height());
    if( this->gui_overlay != NULL ) {
        //this->gui_overlay->setFixedSize(event->size());
        this->gui_overlay->resize(event->size());
        // needed as resizing moves the position, for some reason!
        //qDebug("### %d, %d", scene()->sceneRect().x(), scene()->sceneRect().y());
        //this->gui_overlay_item->setPos( this->scene()->sceneRect().topLeft() );
    }
    this->resetCachedContent();
}

void MainGraphicsView::paintEvent(QPaintEvent *event) {
    //LOG("paint\n");
    if( !smallscreen_c )
    {
        if( fps_timer.isValid() ) {
            fps_frame_count++;
            int time_ms = fps_timer.elapsed();
            if( time_ms > 1000 ) {
                float fps = (fps_frame_count*1000.0f)/(float)time_ms;
                if( gui_overlay != NULL )
                    this->gui_overlay->setFPS(fps);
                fps_timer.start();
                fps_frame_count = 0;
            }
        }
        else {
            fps_timer.start();
            fps_frame_count = 0;
        }
    }

    QGraphicsView::paintEvent(event);

    if( game_g->isLightingEnabled() && this->calculated_lighting_pixmap ) {
        QPainter painter(this->viewport());
        const float size_c = 8.0f;
        Character *player = this->playing_gamestate->getPlayer();
        if( player == NULL ) {
            return;
        }
        QPoint point = this->mapFromScene(player->getX(), player->getY());
        QPoint point_x = this->mapFromScene(player->getX() + size_c, player->getY());
        QPoint point_y = this->mapFromScene(player->getX(), player->getY() + size_c);
        int size_x = point_x.x() - point.x();
        int size_y = point_y.y() - point.y();
        int radius = std::max(size_x, size_y);

        //qDebug("### %f, %f", player->getX(), player->getY());
        //qDebug("### radius = %d", radius);
        //this->calculated_lighting_pixmap_scaled = false;
        // using the scaled lighting pixmap is slightly faster than dynamically drawing at a different size
        // but we have a delay on how often to rescale the lighting pixmap, to avoid the performance being very slow when zooming!
#ifndef Q_OS_SYMBIAN
        // Symbian (at least Nokia 5800) fails to create large pixmap, when zoomed in (larger than the screen).
        // This is only a minor performance improvement anyway.
        if( !this->calculated_lighting_pixmap_scaled && game_g->getGameTimeTotalMS() > lasttime_calculated_lighting_pixmap_scaled_ms + 1000 ) {
            this->lasttime_calculated_lighting_pixmap_scaled_ms = game_g->getGameTimeTotalMS();
            //qDebug("scale pixmap from %d to %d", lighting_pixmap.width(), 2*radius);
            this->lighting_pixmap_scaled = lighting_pixmap.scaledToWidth(2*radius);
            //qDebug("    done");
            this->calculated_lighting_pixmap_scaled = true;
        }
#endif
        //qDebug("%d, %d, %d", point.x(), point.y(), radius);
        //qDebug("darkness_alpha = %d", darkness_alpha);
        // note, sometimes the radius value may fluctuate even if we haven't zoomed in or out (due to rounding issues), which is why we should use the lighting_pixmap_scaled width, rather than radius, when doing the drawing
        int pixmap_width = this->calculated_lighting_pixmap_scaled ? lighting_pixmap_scaled.width() : 2*radius;
        int sx = point.x() - pixmap_width/2;
        int sy = point.y() - pixmap_width/2;
        if( this->calculated_lighting_pixmap_scaled ) {
            //qDebug("draw scaled lighting pixmap");
            painter.drawPixmap(sx, sy, lighting_pixmap_scaled);
        }
        else {
            //qDebug("draw unscaled lighting pixmap");
            painter.drawPixmap(sx, sy, pixmap_width, pixmap_width, lighting_pixmap);
        }
        QBrush brush(QColor(0, 0, 0, darkness_alpha));
        if( sx > 0 ) {
            painter.fillRect(0, 0, sx, this->height(), brush);
        }
        if( sx + pixmap_width < this->width() ) {
            painter.fillRect(sx + pixmap_width, 0, this->width() - sx - pixmap_width, this->height(), brush);
        }
        if( sy > 0 ) {
            painter.fillRect(sx, 0, pixmap_width, sy, brush);
        }
        if( sy + pixmap_width < this->height() ) {
            painter.fillRect(sx, sy + pixmap_width, pixmap_width, this->height() - sy - pixmap_width, brush);
        }
    }
}

void MainGraphicsView::createLightingMap(unsigned char lighting_min) {
    LOG("MainGraphicsView::createLightingMap(): lighting_min = %d\n", lighting_min);
    this->darkness_alpha = (unsigned char)(255 - (int)lighting_min);
    if( game_g->isLightingEnabled() )
    {
        const int res_c = 128;
        QPixmap pixmap(res_c, res_c);
        pixmap.fill(Qt::transparent);
        QRadialGradient radialGrad((res_c-1)/2, (res_c-1)/2, (res_c-1)/2);
        radialGrad.setColorAt(0.0, QColor(0, 0, 0, 0));
        radialGrad.setColorAt(1.0, QColor(0, 0, 0, darkness_alpha));
        QPainter painter2(&pixmap);
        painter2.setPen(Qt::NoPen);
        painter2.fillRect(0, 0, res_c, res_c, radialGrad);
        painter2.end();

        this->calculated_lighting_pixmap = true;
        this->lighting_pixmap = pixmap;
    }
}

void MainGraphicsView::updateInput() {
    if( (qApp->mouseButtons() & Qt::LeftButton) == 0 ) {
        /*int time_ms = game_g->getScreen()->getGameTimeFrameMS();
        float speed = (4.0f * time_ms)/1000.0f;*/
        //if( fabs(this->kinetic_scroll_x) >= 0.0f && fabs(this->kinetic_scroll_y) >= 0.0f ) {
        if( this->has_kinetic_scroll && !has_new_center_on ) {
            int time_ms = game_g->getInputTimeFrameMS();
            //qDebug("centre was: %f, %f", centre.x(), centre.y());
            float step = time_ms*this->kinetic_scroll_speed;
            float move_x = step * this->kinetic_scroll_dir.x;
            float move_y = step * this->kinetic_scroll_dir.y;
            //qDebug("    move: %f, %f", move_x, move_y);

            QPointF centre = this->getCenter();
            QPoint centre_pixels = this->mapFromScene(centre);

            //centre.setX( centre.x() + move_x );
            //centre.setY( centre.y() + move_y );
            centre_pixels.setX( centre_pixels.x() + move_x );
            centre_pixels.setY( centre_pixels.y() + move_y );

            centre = this->mapToScene(centre_pixels);
            //qDebug("    kinetic scroll to: %f, %f", centre.x(), centre.y());
            this->centerOn(centre);
            this->has_new_center_on = false; // should already be false (see above), but just to be safe
            float decel = 0.001f * (float)time_ms;
            this->kinetic_scroll_speed -= decel;
            if( this->kinetic_scroll_speed <= 0.0f ) {
                this->has_kinetic_scroll = false;
                this->kinetic_scroll_speed = 0.0f;
            }
            //qDebug("    kinetic is now %f, %f", this->kinetic_scroll_x, this->kinetic_scroll_y);
        }
    }

    if( has_new_center_on ) {
        //qDebug("centre on: %f, %f", new_center_on.x(), new_center_on.y());
        this->centerOn(new_center_on);
        has_new_center_on = false;
    }
}

void MainGraphicsView::setScale(float c_scale) {
    LOG("MainGraphicsView::setScale(%f)\n", c_scale);
    this->calculated_lighting_pixmap_scaled = false;
    this->lasttime_calculated_lighting_pixmap_scaled_ms = game_g->getGameTimeTotalMS(); // although we haven't calculated it here, we want to postpone the time when we next recalculate it
    this->c_scale = c_scale;
    this->c_scale = std::min(this->c_scale, max_zoom_c);
    this->c_scale = std::max(this->c_scale, min_zoom_c);
    this->resetTransform();
    if( this->playing_gamestate->isTransform3D() ) {
        this->scale(this->c_scale, 0.5f*this->c_scale);
    }
    else {
        this->scale(this->c_scale, this->c_scale);
    }
}

void MainGraphicsView::setScale(QPointF centre, float c_scale) {
    LOG("MainGraphicsView::setScale((%f, %f), %f)\n", centre.x(), centre.y(), c_scale);
    float old_scale = this->c_scale;
    QPointF view_centre = this->getCenter();

    /*this->calculated_lighting_pixmap_scaled = false;
    this->lasttime_calculated_lighting_pixmap_scaled_ms = game_g->getScreen()->getGameTimeTotalMS(); // although we haven't calculated it here, we want to postpone the time when we next recalculate it
    this->c_scale = c_scale;
    this->c_scale = std::min(this->c_scale, max_zoom_c);
    this->c_scale = std::max(this->c_scale, min_zoom_c);*/

    this->setScale(c_scale);
    c_scale = this->c_scale; // setScale may modify this->c_scale - so this line is just to keep things consistent

    float scale_factor = this->c_scale / old_scale;
    QPointF diff = ( view_centre - centre );
    qDebug("view_centre: %f, %f", view_centre.x(), view_centre.y());
    qDebug("diff: %f, %f", diff.x(), diff.y());
    qDebug("scale_factor: %f", scale_factor);

    /*this->resetTransform();
    this->scale(this->c_scale, this->c_scale);*/

    QPointF new_view_centre = centre + diff / scale_factor;
    qDebug("new_view_centre: %f, %f", new_view_centre.x(), new_view_centre.y());
    //this->centerOn(new_view_centre);
    // have problems on Android with Qt 5.3 if we try to call centerOn() directly from an event callback (the screen doesn't update during the touch operation)
    this->has_new_center_on = true;
    this->new_center_on = new_view_centre;
    //qDebug("        scale: post centre on: %f, %f", new_center_on.x(), new_center_on.y());

#ifndef Q_OS_ANDROID
    // needed so that the view is updated, when doing zoom either with multitouch on Windows touchscreen, or two-finger mouse wheel gesture - otherwise we seem to get stuck processing events until the gesture ends
    // not needed for Android, and indeed seems to make things more jerky there
    qApp->processEvents();
#endif
}

void MainGraphicsView::addTextEffect(TextEffect *text_effect) {
    // check to modify text position
    float font_scale = 1.0f / this->c_scale;
    float text_effect_w = text_effect->boundingRect().width() * font_scale;
    float text_effect_h = text_effect->boundingRect().height() * font_scale;
    /*qDebug("add text effect");
    qDebug("    at %f, %f", text_effect->x(), text_effect->y());
    qDebug("    rect %f, %f : %f, %f", text_effect->boundingRect().left(), text_effect->boundingRect().top(), text_effect->boundingRect().right(), text_effect->boundingRect().bottom());
    qDebug("    size %f x %f", text_effect_w, text_effect_h);*/
    bool set_new_y = false;
    float new_y = text_effect->y();
    bool done = false;
    while( !done ) {
        done = true;
        for(set<TextEffect *>::const_iterator iter = text_effects.begin(); iter != text_effects.end(); ++iter) {
            const TextEffect *te = *iter;
            float te_w = te->boundingRect().width() * font_scale;
            float te_h = te->boundingRect().height() * font_scale;
            if( this->playing_gamestate->isTransform3D() ) {
                te_h *= 2.0f;
            }
            if( text_effect->x() + text_effect_w > te->x() &&
                text_effect->x() < te->x() + te_w &&
                new_y + text_effect_h > te->y() &&
                new_y < te->y() + te_h ) {
                //qDebug("    shift text effect by %f", te->boundingRect().bottom() - text_effect->y() );
                /*qDebug("    shift text effect by %f", te->y() + te_h - text_effect->y() );
                qDebug("    te at %f, %f", te->x(), te->y());
                qDebug("    rect %f, %f : %f, %f", te->boundingRect().left(), te->boundingRect().top(), te->boundingRect().right(), te->boundingRect().bottom());
                qDebug("    size %f x %f", te_w, te_h);*/
                //text_effect->setPos( text_effect->x(), te->boundingRect().bottom() );
                //text_effect->setPos( text_effect->x(), te->y() + te_h );
                float this_new_y = te->y() + te_h + E_TOL_LINEAR;
                //if( !set_new_y || this_new_y > new_y )
                {
                    set_new_y = true;
                    done = false;
                    new_y = this_new_y;
                }
            }
        }
    }
    if( set_new_y ) {
        text_effect->setPos( text_effect->x(), new_y );
    }
    this->scene()->addItem(text_effect);
    text_effects.insert(text_effect);
}

void MainGraphicsView::removeTextEffect(TextEffect *text_effect) {
    this->scene()->removeItem(text_effect);
    text_effects.erase(text_effect);
}

GUIOverlay::GUIOverlay(PlayingGamestate *playing_gamestate, MainGraphicsView *view) :
    QWidget(view), playing_gamestate(playing_gamestate),
    display_progress(false), progress_percent(0),
    fps(-1.0f),
    has_fade(false), fade_in(false), fade_time_start_ms(0)
{
#ifndef Q_OS_ANDROID
    // accept touch events so we can pass them through - as WA_TransparentForMouseEvents doesn't seem to pass through touch events
    // not needed for Android, as the MainGraphicsView viewport reviews receive touch events directly
    this->setAttribute(Qt::WA_AcceptTouchEvents);
#endif
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
}

void GUIOverlay::paintEvent(QPaintEvent *) {
    //qDebug("GUIOverlay::paintEvent()");

    QPainter painter(this);
    painter.setFont( game_g->getFontScene() );

    const float bar_x = 16.0f/640.0f;
    const float bar_y = 48.0f/360.0f;
    const float bar_h = 16.0f/360.0f;
    const float portrait_size = 160.0f/1080.f;
    float text_x = bar_x;
    const float text_y = bar_y - 4.0f/360.0f;
    if( playing_gamestate->getPlayer() != NULL ) {
        const Character *player = playing_gamestate->getPlayer();
        if( player->getPortrait().length() > 0 ) {
            QPixmap pixmap = game_g->getPortraitImage(player->getPortrait());
            painter.drawPixmap(bar_x*width(), (bar_y - portrait_size)*height(), portrait_size*height(), portrait_size*height(), pixmap);
        }
        else {
            painter.setPen(Qt::white);
            painter.drawText(text_x*width(), text_y*height(), player->getName().c_str());
        }
        float fraction = ((float)player->getHealthPercent()) / (float)100.0f;
        this->drawBar(painter, bar_x, bar_y, 100.0f/640.0f, 16.0f/360.0f, fraction, Qt::darkGreen);
        if( player->getTargetNPC() != NULL && player->getTargetNPC()->isHostile() ) {
            const Character *enemy = player->getTargetNPC();
            //qDebug("enemy: %d", enemy);
            //qDebug("name: %s", enemy->getName().c_str());
            const float bar_x2 = 132.0f/640.0f;
            painter.setPen(Qt::white);
            painter.drawText(bar_x2*width(), text_y*height(), enemy->getName().c_str());
            fraction = ((float)enemy->getHealthPercent()) / (float)100.0f;
            this->drawBar(painter, bar_x2, bar_y, 100.0f/640.0f, bar_h, fraction, Qt::darkRed);
        }
    }

    if( this->has_fade ) {
        const int duration_ms = 1000;
        int time_diff_ms = game_g->getGameTimeTotalMS() - this->fade_time_start_ms;
        if( time_diff_ms >= duration_ms ) {
            if( this->fade_in ) {
                this->has_fade = false;
            }
            else {
                painter.fillRect(0, 0, width(), height(), QColor(0, 0, 0, 255));
            }
        }
        else if( time_diff_ms < 0 ) {
            ASSERT_LOGGER(time_diff_ms >= 0);
            // shouldn't happen?
            this->has_fade = false;
        }
        else {
            float alpha = ((float)time_diff_ms)/(float)duration_ms;
            int alpha_i = 255*alpha;
            if( this->fade_in ) {
                alpha_i = 255 - alpha_i;
            }
            //qDebug("fade: time %d alpha_i %d", time_diff_ms, alpha_i);
            painter.fillRect(0, 0, width(), height(), QColor(0, 0, 0, alpha_i));
        }
    }
    // fade must be before things that we don't want affected by the fade, e.g., progress bar

    if( this->display_progress ) {
        /*const int x_off = 16;
        const int hgt = 64;
        this->drawBar(painter, x_off, this->height()/2 - hgt/2, this->width() - 2*x_off, hgt, ((float)this->progress_percent)/100.0f, Qt::darkRed);*/
        const float x_off = 16.0f/640.0f;
        const float hgt = 64.0f/360.0f;
        painter.setPen(Qt::white);
        painter.drawText(x_off*width(), (132.0f/360.0f)*height(), progress_message.c_str());
        this->drawBar(painter, x_off, 0.5f - 0.5f*hgt, 1.0f - 2.0f*x_off, hgt, ((float)this->progress_percent)/100.0f, Qt::darkRed);
        qDebug(">>> draw progress: %d", this->progress_percent);
    }

    if( this->fps >= 0.0f )
    {
        painter.setPen(Qt::red);
        //float fps = 1000.0f / game_g->getScreen()->getGameTimeFrameMS();
        painter.drawText(8, height() - 16, QString::number(fps));
    }

    if( playing_gamestate->getCLocation() != NULL && playing_gamestate->getCLocation()->isDisplayName() ) {
        painter.setPen(Qt::white);
        QFontMetrics fm(painter.font());
        int font_height = fm.height();
        painter.drawText(bar_x*width(), (bar_y + bar_h)*height() + font_height, playing_gamestate->getCLocation()->getName().c_str());
    }

}

bool GUIOverlay::event(QEvent *event) {
    //qDebug("GUIOverlay::event() type %d\n", event->type());
#ifndef Q_OS_ANDROID
    // hack as WA_TransparentForMouseEvents doesn't seem to pass through touch events
    // not needed for Android, as the MainGraphicsView viewport reviews receive touch events directly
    if( event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd ) {
        QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
        //QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
        //qDebug("touch event %d count %d", event->type(), touchPoints.count());
        playing_gamestate->processTouchEvent(touchEvent);
        return true;
    }
#endif
    return QWidget::event(event);
}

void GUIOverlay::drawBar(QPainter &painter, float fx, float fy, float fwidth, float fheight, float fraction, QColor color) {
    int x = fx * this->width();
    int y = fy * this->height();
    int width = fwidth * this->width();
    int height = fheight * this->height();
    int x2 = x+1;
    int y2 = y+1;
    int width2 = width-2;
    int height2 = height-2;
    QBrush brush_bg(Qt::black);
    QBrush brush_fg(color);
    painter.setPen(Qt::white);
    painter.setOpacity(0.6f);
    painter.drawRect(x, y, width-1, height-1);
    painter.fillRect(x2, y2, width2, height2, brush_bg);
    painter.fillRect(x2, y2, width2*fraction, height2, brush_fg);
    painter.setOpacity(1.0f);
}

void GUIOverlay::setProgress(int progress_percent) {
    this->setProgress(progress_percent, PlayingGamestate::tr("Please wait...").toStdString());
}

void GUIOverlay::setFadeIn() {
    this->has_fade = true;
    this->fade_in = true;
    this->fade_time_start_ms = game_g->getGameTimeTotalMS();
}

void GUIOverlay::setFadeOut() {
    this->has_fade = true;
    this->fade_in = false;
    this->fade_time_start_ms = game_g->getGameTimeTotalMS();
}

/*void GUIOverlayItem::advance(int phase) {
    //qDebug("GUIOverlayItem::advance() phase %d", phase);
}*/
