#include <QtWebKit/QWebView>

#include <ctime>

#ifdef _DEBUG
#include <cassert>
#endif

#include "rpg/item.h"

#include "playinggamestate.h"
#include "game.h"
#include "infodialog.h"
#include "qt_screen.h"
#include "qt_utils.h"
#include "logiface.h"

#ifdef _DEBUG
#define DEBUG_SHOW_PATH
#endif

const float MainGraphicsView::min_zoom_c = 10.0f;
const float MainGraphicsView::max_zoom_c = 200.0f;

PlayingGamestate *PlayingGamestate::playingGamestate = NULL;

TextEffect::TextEffect(MainGraphicsView *view, const QString &text, int duration_ms, const QColor &color) :
    QGraphicsTextItem(text), time_expire(0), view(view) {

    this->setDefaultTextColor(color);
    this->time_expire = game_g->getScreen()->getGameTimeTotalMS() + duration_ms;
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
}

void TextEffect::advance(int phase) {
    if( phase == 0 ) {
        if( game_g->getScreen()->getGameTimeTotalMS() >= time_expire ) {
            this->view->removeTextEffect(this);
            this->deleteLater();
        }
    }
}

MainGraphicsView::MainGraphicsView(PlayingGamestate *playing_gamestate, QGraphicsScene *scene, QWidget *parent) :
    QGraphicsView(scene, parent), playing_gamestate(playing_gamestate), mouse_down_x(0), mouse_down_y(0), single_left_mouse_down(false), has_last_mouse(false), last_mouse_x(0), last_mouse_y(0), has_kinetic_scroll(false), kinetic_scroll_speed(0.0f),
    /*gui_overlay_item(NULL),*/ gui_overlay(NULL), c_scale(1.0f), calculated_lighting_pixmap(false), calculated_lighting_pixmap_scaled(false), lasttime_calculated_lighting_pixmap_scaled_ms(0), darkness_alpha(0)
{
    this->fps_timer.invalidate();
    for(int i=0;i<N_KEYS;i++) {
        this->key_down[i] = false;
    }
    this->grabKeyboard();
}

void MainGraphicsView::zoomOut() {
    QPointF zoom_centre = this->mapToScene( this->rect() ).boundingRect().center();
    this->zoom(zoom_centre, false);
}

void MainGraphicsView::zoomIn() {
    QPointF zoom_centre = this->mapToScene( this->rect() ).boundingRect().center();
    this->zoom(zoom_centre, true);
}

void MainGraphicsView::centreOnPlayer() {
    Character *character = playing_gamestate->getPlayer();
    AnimatedObject *object = static_cast<AnimatedObject *>(character->getListenerData());
    this->centerOn(object);
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

bool MainGraphicsView::viewportEvent(QEvent *event) {
    //qDebug("MainGraphicsView::viewportEvent() type %d\n", event->type());
    // multitouch done by touch events manually - gestures don't seem to work properly on my Android phone?
    if( event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchUpdate ) {
        QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
        QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
        if( touchPoints.count() > 1 ) {
            this->has_kinetic_scroll = false;
            this->single_left_mouse_down = false;
            // can't trust last mouse position if more that one touch!
            this->has_last_mouse = false;
        }
    }
    if( event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd ) {
        QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
        QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
        //qDebug("touch event %d count %d", event->type(), touchPoints.count());
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
        this->single_left_mouse_down = true;
        this->mouse_down_x = event->x();
        this->mouse_down_y = event->y();
        this->has_kinetic_scroll = false;
    }

    QGraphicsView::mousePressEvent(event);
}

// On a touchscreen phone, it's very hard to press and release without causing a drag, so need to allow some tolerance!
// Needs to be higher on Symbian, at least for Nokia 5800, as touching the display seems to cause it to move so much more easily.
#if defined(Q_OS_SYMBIAN)
const int drag_tol_c = 24;
#else
const int drag_tol_c = 16;
#endif

void MainGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    //qDebug("MainGraphicsView::mouseReleaseEvent");
    if( event->button() & Qt::LeftButton ) {
        this->single_left_mouse_down = false;
        this->has_last_mouse = false;
        int m_x = event->x();
        int m_y = event->y();
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

    QGraphicsView::mouseReleaseEvent(event);
}

void MainGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    //qDebug("MainGraphicsView::mouseMoveEvent");
    if( this->single_left_mouse_down ) {
        // only if the left mouse is down, and this isn't a multitouch event (otherwise single_left_mouse_down should be set to false)
        //qDebug("    lmb");
        if( this->has_last_mouse ) {
            //qDebug("    has last mouse");
            /*QPointF old_pos = this->mapToScene(this->last_mouse_x, this->last_mouse_y);
            QPointF new_pos = this->mapToScene(event->x(), event->y());*/
            QPointF old_pos(this->last_mouse_x, this->last_mouse_y);
            QPointF new_pos = event->posF();
            QPointF diff = old_pos - new_pos; // n.b., scene scrolls in opposite direction to mouse movement

            // drag - we do this ourselves rather than using drag mode QGraphicsView::ScrollHandDrag, to avoid conflict with multitouch
            QPointF centre = this->mapToScene( this->rect() ).boundingRect().center();
            QPoint centre_pixels = this->mapFromScene(centre);
            centre_pixels.setX( centre_pixels.x() + diff.x() );
            centre_pixels.setY( centre_pixels.y() + diff.y() );
            centre = this->mapToScene(centre_pixels);
            this->centerOn(centre);

            // need to check against drag_tol_c, otherwise simply clicking can cause us to move with kinetic motion (at least on Android)
            if( fabs(diff.x()) > drag_tol_c || fabs(diff.y()) > drag_tol_c ) {
                int time_ms = game_g->getScreen()->getRealTimeFrameMS();
                diff /= (float)time_ms;
                this->has_kinetic_scroll = true;
                this->kinetic_scroll_dir.set(diff.x(), diff.y());
                this->kinetic_scroll_speed = this->kinetic_scroll_dir.magnitude();
                this->kinetic_scroll_dir /= this->kinetic_scroll_speed;
                this->kinetic_scroll_speed = std::min(this->kinetic_scroll_speed, 1.0f);
                //qDebug("    speed: %f", this->kinetic_scroll_speed);
            }
            else {
                this->has_kinetic_scroll = false;
            }
        }
        else {
            this->has_kinetic_scroll = false;
        }
        this->has_last_mouse = true;
        this->last_mouse_x = event->x();
        this->last_mouse_y = event->y();
    }

    QGraphicsView::mouseMoveEvent(event);
}

void MainGraphicsView::wheelEvent(QWheelEvent *event) {
    if( !mobile_c ) {
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
    qDebug("MainGraphicsView::keyPressEvent: %d", event->key());
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
}

void MainGraphicsView::paintEvent(QPaintEvent *event) {
    /*QElapsedTimer timer;
    timer.start();*/
    if( fps_timer.isValid() ) {
        int time_ms = fps_timer.elapsed();
        if( time_ms > 0 ) {
            float fps = 1000.0f/(float)time_ms;
            this->gui_overlay->setFPS(fps);
        }
    }
    fps_timer.start();

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
        if( !this->calculated_lighting_pixmap_scaled && game_g->getScreen()->getGameTimeTotalMS() > lasttime_calculated_lighting_pixmap_scaled_ms + 1000 ) {
            this->lasttime_calculated_lighting_pixmap_scaled_ms = game_g->getScreen()->getGameTimeTotalMS();
            //qDebug("scale pixmap");
            this->lighting_pixmap_scaled = lighting_pixmap.scaledToWidth(2*radius);
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
            painter.drawPixmap(sx, sy, lighting_pixmap_scaled);
        }
        else {
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

    /*int time_ms = timer.elapsed();
    if( time_ms > 0 ) {
        float fps = 1000.0f/(float)time_ms;
        this->gui_overlay->setFPS(fps);
    }*/
}

void MainGraphicsView::createLightingMap(unsigned char lighting_min) {
    LOG("MainGraphicsView::createLightingMap(): lighting_min = %d\n", lighting_min);
    this->darkness_alpha = (unsigned char)(255 - (int)lighting_min);
    if( game_g->isLightingEnabled() )
    {
        const int res_c = 127;
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

void MainGraphicsView::update() {
    if( (qApp->mouseButtons() & Qt::LeftButton) == 0 ) {
        /*int time_ms = game_g->getScreen()->getGameTimeFrameMS();
        float speed = (4.0f * time_ms)/1000.0f;*/
        //if( fabs(this->kinetic_scroll_x) >= 0.0f && fabs(this->kinetic_scroll_y) >= 0.0f ) {
        if( this->has_kinetic_scroll ) {
            int time_ms = game_g->getScreen()->getRealTimeFrameMS();
            //qDebug("centre was: %f, %f", centre.x(), centre.y());
            float step = time_ms*this->kinetic_scroll_speed;
            float move_x = step * this->kinetic_scroll_dir.x;
            float move_y = step * this->kinetic_scroll_dir.y;
            //qDebug("    move: %f, %f", move_x, move_y);

            QPointF centre = this->mapToScene( this->rect() ).boundingRect().center();
            QPoint centre_pixels = this->mapFromScene(centre);

            //centre.setX( centre.x() + move_x );
            //centre.setY( centre.y() + move_y );
            centre_pixels.setX( centre_pixels.x() + move_x );
            centre_pixels.setY( centre_pixels.y() + move_y );

            centre = this->mapToScene(centre_pixels);
            //qDebug("    now: %f, %f", centre.x(), centre.y());
            this->centerOn(centre);
            float decel = 0.001f * (float)time_ms;
            this->kinetic_scroll_speed -= decel;
            if( this->kinetic_scroll_speed <= 0.0f ) {
                this->has_kinetic_scroll = false;
                this->kinetic_scroll_speed = 0.0f;
            }
            //qDebug("    kinetic is now %f, %f", this->kinetic_scroll_x, this->kinetic_scroll_y);
        }
    }
}

void MainGraphicsView::setScale(float c_scale) {
    LOG("MainGraphicsView::setScale(%f)\n", c_scale);
    this->calculated_lighting_pixmap_scaled = false;
    this->lasttime_calculated_lighting_pixmap_scaled_ms = game_g->getScreen()->getGameTimeTotalMS(); // although we haven't calculated it here, we want to postpone the time when we next recalculate it
    this->c_scale = c_scale;
    this->c_scale = std::min(this->c_scale, max_zoom_c);
    this->c_scale = std::max(this->c_scale, min_zoom_c);
    this->resetTransform();
    this->scale(this->c_scale, this->c_scale);
}

void MainGraphicsView::setScale(QPointF centre, float c_scale) {
    LOG("MainGraphicsView::setScale((%f, %f), %f)\n", centre.x(), centre.y(), c_scale);
    this->calculated_lighting_pixmap_scaled = false;
    this->lasttime_calculated_lighting_pixmap_scaled_ms = game_g->getScreen()->getGameTimeTotalMS(); // although we haven't calculated it here, we want to postpone the time when we next recalculate it
    float old_scale = this->c_scale;
    this->c_scale = c_scale;
    this->c_scale = std::min(this->c_scale, max_zoom_c);
    this->c_scale = std::max(this->c_scale, min_zoom_c);

    float scale_factor = this->c_scale / old_scale;
    QPointF view_centre = this->mapToScene( this->rect() ).boundingRect().center();
    QPointF diff = ( view_centre - centre );
    qDebug("view_centre: %f, %f", view_centre.x(), view_centre.y());
    qDebug("diff: %f, %f", diff.x(), diff.y());
    qDebug("scale_factor: %f", scale_factor);

    this->resetTransform();
    this->scale(this->c_scale, this->c_scale);
    QPointF new_view_centre = centre + diff / scale_factor;
    qDebug("new_view_centre: %f, %f", new_view_centre.x(), new_view_centre.y());
    this->centerOn(new_view_centre);
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
    QWidget(view), playing_gamestate(playing_gamestate), fps(-1.0f)
{
    //this->setAttribute(Qt::WA_NoSystemBackground);
}

void GUIOverlay::paintEvent(QPaintEvent *event) {
    //qDebug("GUIOverlay::paintEvent()");

    QPainter painter(this);
    painter.setFont( game_g->getFontScene() );
    if( playing_gamestate->getPlayer() != NULL ) {
        float bar_x = 16.0f/640.0f;
        float bar_y = 32.0f/360.0f;
        float text_y = bar_y - 4.0f/360.0f;
        const Character *player = playing_gamestate->getPlayer();
        painter.setPen(Qt::white);
        painter.drawText(bar_x*width(), text_y*height(), player->getName().c_str());
        float fraction = ((float)player->getHealthPercent()) / (float)100.0f;
        this->drawBar(painter, bar_x, bar_y, 100.0f/640.0f, 16.0f/360.0f, fraction, Qt::darkGreen);
        if( player->getTargetNPC() != NULL && player->getTargetNPC()->isHostile() ) {
            const Character *enemy = player->getTargetNPC();
            //qDebug("enemy: %d", enemy);
            //qDebug("name: %s", enemy->getName().c_str());
            float bar_x2 = 132.0f/640.0f;
            painter.setPen(Qt::white);
            painter.drawText(bar_x2*width(), text_y*height(), enemy->getName().c_str());
            fraction = ((float)enemy->getHealthPercent()) / (float)100.0f;
            this->drawBar(painter, bar_x2, bar_y, 100.0f/640.0f, 16.0f/360.0f, fraction, Qt::darkRed);
        }
    }

    if( this->display_progress ) {
        /*const int x_off = 16;
        const int hgt = 64;
        this->drawBar(painter, x_off, this->height()/2 - hgt/2, this->width() - 2*x_off, hgt, ((float)this->progress_percent)/100.0f, Qt::darkRed);*/
        const float x_off = 16.0f/640.0f;
        const float hgt = 64.0f/360.0f;
        painter.setPen(Qt::white);
        painter.drawText(x_off*width(), (100.0f/360.0f)*height(), "Please wait...");
        this->drawBar(painter, x_off, 0.5f - 0.5f*hgt, 1.0f - 2.0f*x_off, hgt, ((float)this->progress_percent)/100.0f, Qt::darkRed);
        qDebug(">>> draw progress: %d", this->progress_percent);
    }

    if( this->fps >= 0.0f )
    {
        painter.setPen(Qt::red);
        //float fps = 1000.0f / game_g->getScreen()->getGameTimeFrameMS();
        painter.drawText(8, height() - 16, QString::number(fps));
    }
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

/*void GUIOverlayItem::advance(int phase) {
    //qDebug("GUIOverlayItem::advance() phase %d", phase);
}*/

/*void StatusBar::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    QBrush brush(color);
    painter.fillRect(QRectF(QPointF(0, 0), this->size()), brush);
}*/

StatsWindow::StatsWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate)
{
    playing_gamestate->addWidget(this);

    const Character *player = playing_gamestate->getPlayer();

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    QString html ="<html><body>";

    html += "<b>Name:</b> ";
    html += player->getName().c_str();
    html += "<br/>";

    html += "<b>Difficulty:</b> ";
    html += Game::getDifficultyString(this->playing_gamestate->getDifficulty()).c_str();
    html += "<br/>";

    /*html += "<b>Fighting Prowess:</b> " + QString::number(player->getFP()) + "<br/>";
    html += "<b>Bow Skill:</b> " + QString::number(player->getBS()) + "<br/>";
    html += "<b>Strength:</b> " + QString::number(player->getStrength()) + "<br/>";
    html += "<b>Attacks:</b> " + QString::number(player->getAttacks()) + "<br/>";
    html += "<b>Mind:</b> " + QString::number(player->getMind()) + "<br/>";
    html += "<b>Dexterity:</b> " + QString::number(player->getDexterity()) + "<br/>";
    html += "<b>Bravery:</b> " + QString::number(player->getBravery()) + "<br/>";
    html += "<b>Speed:</b> " + QString::number(player->getSpeed()) + "<br/>";*/

    /*html += "<b>Fighting Prowess:</b> " + QString::number(player->getProfileIntProperty(profile_key_FP_c)) + "<br/>";
    html += "<b>Bow Skill:</b> " + QString::number(player->getProfileIntProperty(profile_key_BS_c)) + "<br/>";
    html += "<b>Strength:</b> " + QString::number(player->getProfileIntProperty(profile_key_S_c)) + "<br/>";
    html += "<b>Attacks:</b> " + QString::number(player->getProfileIntProperty(profile_key_A_c)) + "<br/>";
    html += "<b>Mind:</b> " + QString::number(player->getProfileIntProperty(profile_key_M_c)) + "<br/>";
    html += "<b>Dexterity:</b> " + QString::number(player->getProfileIntProperty(profile_key_D_c)) + "<br/>";
    html += "<b>Bravery:</b> " + QString::number(player->getProfileIntProperty(profile_key_B_c)) + "<br/>";
    html += "<b>Speed:</b> " + QString::number(player->getProfileFloatProperty(profile_key_Sp_c)) + "<br/>";*/
    html += this->writeStat("Fighting Prowess", profile_key_FP_c, false);
    html += this->writeStat("Bow Skill", profile_key_BS_c, false);
    html += this->writeStat("Strength", profile_key_S_c, false);
    html += this->writeStat("Attacks", profile_key_A_c, false);
    html += this->writeStat("Mind", profile_key_M_c, false);
    html += this->writeStat("Dexterity", profile_key_D_c, false);
    html += this->writeStat("Bravery", profile_key_B_c, false);
    html += this->writeStat("Speed", profile_key_Sp_c, true);

    html += "<b>Health:</b> ";
    if( player->getHealth() < player->getMaxHealth() ) {
        html += "<font color=\"#ff0000\">";
        html += QString::number(player->getHealth());
        html += "</font>";
    }
    else {
        html += QString::number(player->getHealth());
    }
    html += " / ";
    html += QString::number(player->getMaxHealth());
    html += "<br/>";

    html += "<b>Level:</b> " + QString::number(player->getLevel()) + "<br/>";
    html += "<b>XP:</b> " + QString::number(player->getXP()) + " (" + QString::number(player->getXPForNextLevel()) + " required for next level.)<br/>";

    html += "</body></html>";

    QWebView *label = new QWebView();
    game_g->setWebView(label);
    label->setHtml(html);
    layout->addWidget(label);

    QPushButton *closeButton = new QPushButton("Continue");
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeSubWindow()));
}

QString StatsWindow::writeStat(const string &visual_name, const string &stat_key, bool is_float) const {
    const Character *player = playing_gamestate->getPlayer();
    QString html = "<b>";
    html += visual_name.c_str();
    html += ":</b> ";
    if( is_float ) {
        float stat = player->getProfileFloatProperty(stat_key);
        float base_stat = player->getBaseProfileFloatProperty(stat_key);
        if( stat > base_stat ) {
            html += "<font color=\"#00ff00\">";
        }
        else if( stat < base_stat ) {
            html += "<font color=\"#ff0000\">";
        }
        html += QString::number(stat);
        if( stat != base_stat ) {
            html += "</font>";
        }
    }
    else {
        int stat = player->getProfileIntProperty(stat_key);
        int base_stat = player->getBaseProfileIntProperty(stat_key);
        if( stat > base_stat ) {
            html += "<font color=\"#00ff00\">";
        }
        else if( stat < base_stat ) {
            html += "<font color=\"#ff0000\">";
        }
        html += QString::number(stat);
        if( stat != base_stat ) {
            html += "</font>";
        }
    }
    html += "<br/>";
    return html;
}

ItemsWindow::ItemsWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate), list(NULL),
    dropButton(NULL), armButton(NULL), wearButton(NULL), useButton(NULL),
    view_type(VIEWTYPE_ALL)
{
    playing_gamestate->addWidget(this);

    Character *player = playing_gamestate->getPlayer();

    QFont font = game_g->getFontStd();
    this->setFont(font);
    QFont font_small = game_g->getFontSmall();

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *viewAllButton = new QPushButton("All");
        game_g->initButton(viewAllButton);
        viewAllButton->setFont(font_small);
        viewAllButton->setToolTip("Display all items (F1)");
        viewAllButton->setShortcut(QKeySequence(Qt::Key_F1));
        h_layout->addWidget(viewAllButton);
        connect(viewAllButton, SIGNAL(clicked()), this, SLOT(clickedViewAll()));

        QPushButton *viewWeaponsButton = new QPushButton("Wpns");
        game_g->initButton(viewWeaponsButton);
        viewWeaponsButton->setFont(font_small);
        viewWeaponsButton->setToolTip("Display only weapons (F2)");
        viewWeaponsButton->setShortcut(QKeySequence(Qt::Key_F2));
        h_layout->addWidget(viewWeaponsButton);
        connect(viewWeaponsButton, SIGNAL(clicked()), this, SLOT(clickedViewWeapons()));

        QPushButton *viewAmmoButton = new QPushButton("Ammo");
        game_g->initButton(viewAmmoButton);
        viewAmmoButton->setFont(font_small);
        viewAmmoButton->setToolTip("Display only ammunition (F3)");
        viewAmmoButton->setShortcut(QKeySequence(Qt::Key_F3));
        h_layout->addWidget(viewAmmoButton);
        connect(viewAmmoButton, SIGNAL(clicked()), this, SLOT(clickedViewAmmo()));

        QPushButton *viewShieldsButton = new QPushButton("Shields");
        game_g->initButton(viewShieldsButton);
        viewShieldsButton->setFont(font_small);
        viewShieldsButton->setToolTip("Display only shields (F4)");
        viewShieldsButton->setShortcut(QKeySequence(Qt::Key_F4));
        h_layout->addWidget(viewShieldsButton);
        connect(viewShieldsButton, SIGNAL(clicked()), this, SLOT(clickedViewShields()));

        QPushButton *viewArmourButton = new QPushButton("Arm");
        game_g->initButton(viewArmourButton);
        viewArmourButton->setFont(font_small);
        viewArmourButton->setToolTip("Display only armour (F5)");
        viewArmourButton->setShortcut(QKeySequence(Qt::Key_F5));
        h_layout->addWidget(viewArmourButton);
        connect(viewArmourButton, SIGNAL(clicked()), this, SLOT(clickedViewArmour()));

        QPushButton *viewMagicButton = new QPushButton("Magic");
        game_g->initButton(viewMagicButton);
        viewMagicButton->setFont(font_small);
        viewMagicButton->setToolTip("Display only magical items (F6)");
        viewMagicButton->setShortcut(QKeySequence(Qt::Key_F6));
        h_layout->addWidget(viewMagicButton);
        connect(viewMagicButton, SIGNAL(clicked()), this, SLOT(clickedViewMagic()));

        QPushButton *viewMiscButton = new QPushButton("Misc");
        game_g->initButton(viewMiscButton);
        viewMiscButton->setFont(font_small);
        viewMiscButton->setToolTip("Display only miscellaneous items (F7)");
        viewMiscButton->setShortcut(QKeySequence(Qt::Key_F7));
        h_layout->addWidget(viewMiscButton);
        connect(viewMiscButton, SIGNAL(clicked()), this, SLOT(clickedViewMisc()));
    }

    list = new ScrollingListWidget();
    //list->setViewMode(QListView::IconMode);
    if( !mobile_c ) {
        QFont list_font = list->font();
        list_font.setPointSize( list_font.pointSize() + 8 );
        list->setFont(list_font);
    }
    {
        QFontMetrics fm(list->font());
        int icon_size = fm.height();
        //int icon_size = fm.width("LONGSWORD");
        list->setIconSize(QSize(icon_size, icon_size));
        LOG("icon size now %f, %f\n", list->iconSize().width(), list->iconSize().height());
    }
    layout->addWidget(list);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->grabKeyboard();

    this->refreshList();

    connect(list, SIGNAL(currentRowChanged(int)), this, SLOT(changedSelectedItem(int)));
    connect(list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(clickedInfo()));

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *goldLabel = new QLabel("Gold: " + QString::number( player->getGold() ));
        //goldLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // needed to fix problem of having too little vertical space (on Qt Smartphone Simulator at least)
        h_layout->addWidget(goldLabel);

        weightLabel = new QLabel(""); // label set in setWeightLabel()
        //weightLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(weightLabel);
        this->setWeightLabel();
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        armButton = new QPushButton(""); // text set in changedSelectedItem()
        game_g->initButton(armButton);
        armButton->setToolTip("Arm/disarm weapon or shield (Space)");
        h_layout->addWidget(armButton);
        connect(armButton, SIGNAL(clicked()), this, SLOT(clickedArmWeapon()));

        wearButton = new QPushButton(""); // text set in changedSelectedItem()
        game_g->initButton(wearButton);
        wearButton->setToolTip("Wear/take off armour (Space)");
        h_layout->addWidget(wearButton);
        connect(wearButton, SIGNAL(clicked()), this, SLOT(clickedWearArmour()));

        useButton = new QPushButton(""); // text set in changedSelectedItem()
        game_g->initButton(useButton);
        useButton->setToolTip("Use item (Space)");
        h_layout->addWidget(useButton);
        connect(useButton, SIGNAL(clicked()), this, SLOT(clickedUseItem()));

        dropButton = new QPushButton("Drop Item");
        game_g->initButton(dropButton);
        dropButton->setToolTip("Drop item onto the ground (D))");
        dropButton->setShortcut(QKeySequence(Qt::Key_D));
        h_layout->addWidget(dropButton);
        connect(dropButton, SIGNAL(clicked()), this, SLOT(clickedDropItem()));

        infoButton = new QPushButton("Info");
        game_g->initButton(infoButton);
        infoButton->setToolTip("More information on this item (I))");
        infoButton->setShortcut(QKeySequence(Qt::Key_I));
        h_layout->addWidget(infoButton);
        connect(infoButton, SIGNAL(clicked()), this, SLOT(clickedInfo()));
    }

    QPushButton *closeButton = new QPushButton("Continue");
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeSubWindow()));

    /*if( list_items.size() > 0 ) {
        list->setCurrentRow(0);
    }*/
    int index = list->currentRow();
    this->changedSelectedItem(index);
}

void ItemsWindow::refreshList() {
    qDebug("ItemsWindow::refreshList()");
    list_items.clear();
    list->clear();
    Character *player = playing_gamestate->getPlayer();
    for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
        Item *item = *iter;
        if( view_type == VIEWTYPE_WEAPONS && item->getType() != ITEMTYPE_WEAPON ) {
            continue;
        }
        else if( view_type == VIEWTYPE_SHIELDS && item->getType() != ITEMTYPE_SHIELD ) {
            continue;
        }
        else if( view_type == VIEWTYPE_ARMOUR && item->getType() != ITEMTYPE_ARMOUR ) {
            continue;
        }
        else if( view_type == VIEWTYPE_AMMO && item->getType() != ITEMTYPE_AMMO ) {
            continue;
        }
        else if( view_type == VIEWTYPE_MAGIC && !item->isMagical() ) {
            continue;
        }
        else if( view_type == VIEWTYPE_MISC && !( item->getType() == ITEMTYPE_GENERAL && !item->isMagical() ) ) {
            continue;
        }
        QString item_str = this->getItemString(item);
        //list->addItem( item_str );
        QListWidgetItem *list_item = new QListWidgetItem(item_str);
        QIcon icon( playing_gamestate->getItemImage( item->getImageName() ) );
        list_item->setIcon(icon);
        list->addItem(list_item);
        list_items.push_back(item);
    }
    if( list->count() > 0 ) {
        list->setCurrentRow(0);
    }
}

void ItemsWindow::changeView(ViewType view_type) {
    if( this->view_type != view_type ) {
        this->view_type = view_type;
        this->refreshList();
    }
}

void ItemsWindow::clickedViewAll() {
    this->changeView(VIEWTYPE_ALL);
}

void ItemsWindow::clickedViewWeapons() {
    this->changeView(VIEWTYPE_WEAPONS);
}

void ItemsWindow::clickedViewAmmo() {
    this->changeView(VIEWTYPE_AMMO);
}

void ItemsWindow::clickedViewShields() {
    this->changeView(VIEWTYPE_SHIELDS);
}

void ItemsWindow::clickedViewArmour() {
    this->changeView(VIEWTYPE_ARMOUR);
}

void ItemsWindow::clickedViewMagic() {
    this->changeView(VIEWTYPE_MAGIC);
}

void ItemsWindow::clickedViewMisc() {
    this->changeView(VIEWTYPE_MISC);
}

QString ItemsWindow::getItemString(const Item *item) const {
    QString item_str = item->getName().c_str();
    if( playing_gamestate->getPlayer()->getCurrentWeapon() == item ) {
        item_str += " [Current Weapon]";
    }
    else if( playing_gamestate->getPlayer()->getCurrentShield() == item ) {
        item_str += " [Current Shield]";
    }
    else if( playing_gamestate->getPlayer()->getCurrentArmour() == item ) {
        item_str += " [Current Armour]";
    }
    if( item->getWeight() > 0 ) {
        item_str += " (Weight " + QString::number(item->getWeight()) + ")";
    }
    return item_str;
}

void ItemsWindow::changedSelectedItem(int currentRow) {
    LOG("changedSelectedItem(%d)\n", currentRow);

    if( currentRow == -1 ) {
        dropButton->setVisible(false);
        armButton->setVisible(false);
        wearButton->setVisible(false);
        useButton->setVisible(false);
        infoButton->setVisible(false);
        return;
    }
    dropButton->setVisible(true);
    infoButton->setVisible(true);
    Item *item = list_items.at(currentRow);
    if( item->getType() == ITEMTYPE_WEAPON ) {
        armButton->setVisible(true);
        if( playing_gamestate->getPlayer()->getCurrentWeapon() == item ) {
            armButton->setText("Disarm Weapon");
        }
        else {
            armButton->setText("Arm Weapon");
        }
    }
    else if( item->getType() == ITEMTYPE_SHIELD ) {
        if( playing_gamestate->getPlayer()->getCurrentWeapon() != NULL && playing_gamestate->getPlayer()->getCurrentWeapon()->isTwoHanded() ) {
            armButton->setVisible(false);
        }
        else {
            armButton->setVisible(true);
            if( playing_gamestate->getPlayer()->getCurrentShield() == item ) {
                armButton->setText("Disarm Shield");
            }
            else {
                armButton->setText("Arm Shield");
            }
        }
    }
    else {
        armButton->setVisible(false);
    }
    if( item->getType() != ITEMTYPE_ARMOUR ) {
        wearButton->setVisible(false);
    }
    else {
        wearButton->setVisible(true);
        if( playing_gamestate->getPlayer()->getCurrentArmour() == item ) {
            wearButton->setText("Take Off Armour");
        }
        else {
            wearButton->setText("Wear Armour");
        }
    }

    if( !item->canUse() ) {
        useButton->setVisible(false);
    }
    else {
        useButton->setVisible(true);
        useButton->setText(item->getUseVerb().c_str());
    }

    armButton->setShortcut(QKeySequence(Qt::Key_Space));
    wearButton->setShortcut(QKeySequence(Qt::Key_Space));
    useButton->setShortcut(QKeySequence(Qt::Key_Space));
}

void ItemsWindow::setWeightLabel() {
    Character *player = this->playing_gamestate->getPlayer();
    string weight_str = player->getWeightString();
    this->weightLabel->setText(weight_str.c_str());
}

void ItemsWindow::clickedArmWeapon() {
    LOG("clickedArmWeapon()\n");
    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    Item *item = list_items.at(index);
    if( item->getType() == ITEMTYPE_WEAPON ) {
        Weapon *weapon = static_cast<Weapon *>(item);
        if( playing_gamestate->getPlayer()->getCurrentWeapon() == weapon ) {
            // disarm instead
            LOG("player disarmed weapon\n");
            playing_gamestate->getPlayer()->armWeapon(NULL);
        }
        else {
            LOG("player armed weapon: %s\n", item->getName().c_str());
            playing_gamestate->getPlayer()->armWeapon(weapon);
            playing_gamestate->playSound("weapon_unsheath");
        }
    }
    else if( item->getType() == ITEMTYPE_SHIELD ) {
        Shield *shield = static_cast<Shield *>(item);
        if( playing_gamestate->getPlayer()->getCurrentShield() == shield ) {
            // disarm instead
            LOG("player disarmed shield");
            playing_gamestate->getPlayer()->armShield(NULL);
        }
        else {
            LOG("player armed shield: %s\n", item->getName().c_str());
            playing_gamestate->getPlayer()->armShield(shield);
        }
    }
    else {
        LOG("not a weapon or shield?!\n");
    }

    /*QListWidgetItem *item_widget = list->item(index);
    item_widget->setText( this->getItemString(item) );*/
    for(size_t i=0;i<list_items.size();i++) {
        const Item *item = list_items.at(i);
        QListWidgetItem *item_widget = list->item(i);
        item_widget->setText( this->getItemString(item) );
    }
    this->changedSelectedItem(index);
}

void ItemsWindow::clickedWearArmour() {
    LOG("clickedWearArmour()\n");
    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    Item *item = list_items.at(index);
    if( item->getType() != ITEMTYPE_ARMOUR ) {
        LOG("not armour?!\n");
        return;
    }
    Armour *armour = static_cast<Armour *>(item);
    if( playing_gamestate->getPlayer()->getCurrentArmour() == armour ) {
        // take off instead
        LOG("player took off armour\n");
        playing_gamestate->getPlayer()->wearArmour(NULL);
    }
    else {
        LOG("player put on armour: %s\n", item->getName().c_str());
        playing_gamestate->getPlayer()->wearArmour(armour);
        playing_gamestate->playSound("wear_armour");
    }

    /*QListWidgetItem *item_widget = list->item(index);
    item_widget->setText( this->getItemString(item) );*/
    for(size_t i=0;i<list_items.size();i++) {
        const Item *item = list_items.at(i);
        QListWidgetItem *item_widget = list->item(i);
        item_widget->setText( this->getItemString(item) );
    }
    this->changedSelectedItem(index);
}

void ItemsWindow::clickedUseItem() {
    LOG("clickedUseItem()\n");
    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    Item *item = list_items.at(index);
    Character *player = this->playing_gamestate->getPlayer();
    if( item->useItem(this->playing_gamestate, player) ) {
        // item is deleted
        player->takeItem(item);
        delete item;
        item = NULL;
        this->itemIsDeleted(index);
    }
    if( player->isDead() ) {
        this->playing_gamestate->closeSubWindow();
    }
}

void ItemsWindow::clickedDropItem() {
    LOG("clickedDropItem()\n");
    /*QList<QListWidgetItem *> selected_items = list->selectedItems();
    if( selected_items.size() == 1 ) {
        QListWidgetItem *selected_item = selected_items.at(0);*/
    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    Item *item = list_items.at(index);
    Character *player = playing_gamestate->getPlayer();
    player->dropItem(item);

    /*list->clear();
    list_items.clear();
    for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
        Item *item = *iter;
        list->addItem( item->getName().c_str() );
        list_items.push_back(item);
    }*/
    this->itemIsDeleted(index);
}

void ItemsWindow::clickedInfo() {
    LOG("clickedInfo()\n");
    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    const Item *item = list_items.at(index);
    string info = item->getDetailedDescription();
    info = convertToHTML(info);
    playing_gamestate->showInfoWindow(info);
}

void ItemsWindow::itemIsDeleted(size_t index) {
    QListWidgetItem *list_item = list->takeItem(index);
    delete list_item;
    list_items.erase(list_items.begin() + index);
    if( list_items.size() > 0 ) {
        if( index > list_items.size()-1 )
            index = list_items.size()-1;
        list->setCurrentRow(index);
    }

    this->setWeightLabel();
}

TradeWindow::TradeWindow(PlayingGamestate *playing_gamestate, const vector<const Item *> &items, const vector<int> &costs) :
    playing_gamestate(playing_gamestate), goldLabel(NULL), weightLabel(NULL), list(NULL), items(items), costs(costs), player_list(NULL)
{
    playing_gamestate->addWidget(this);

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    Character *player = playing_gamestate->getPlayer();

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *label = new QLabel("What would you like to buy?");
        //label->setWordWrap(true);
        h_layout->addWidget(label);

        goldLabel = new QLabel("");
        h_layout->addWidget(goldLabel);
        this->updateGoldLabel();

        weightLabel = new QLabel("");
        h_layout->addWidget(weightLabel);
        this->setWeightLabel();
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *sellLabel = new QLabel("Your items:");
        h_layout->addWidget(sellLabel);

        QLabel *buyLabel = new QLabel("Items to buy:");
        h_layout->addWidget(buyLabel);
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        player_list = new ScrollingListWidget();
        if( !mobile_c ) {
            QFont list_font = player_list->font();
            list_font.setPointSize( list_font.pointSize() + 8 );
            player_list->setFont(list_font);
        }
        {
            QFontMetrics fm(player_list->font());
            int icon_size = fm.height();
            player_list->setIconSize(QSize(icon_size, icon_size));
        }
        h_layout->addWidget(player_list);
        for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
            Item *item = *iter;
            int cost = 0;
            for(size_t i=0;i<items.size();i++) {
                const Item *item2 = items.at(i);
                if( item->getKey() == item2->getKey() || item->getBaseTemplate() == item2->getKey() ) {
                    cost = costs.at(i);
                    cost += item->getWorthBonus();
                }
            }
            addPlayerItem(item, cost);
        }

        list = new ScrollingListWidget();
        if( !mobile_c ) {
            QFont list_font = list->font();
            list_font.setPointSize( list_font.pointSize() + 8 );
            list->setFont(list_font);
        }
        {
            QFontMetrics fm(list->font());
            int icon_size = fm.height();
            list->setIconSize(QSize(icon_size, icon_size));
        }
        h_layout->addWidget(list);
        for(size_t i=0;i<items.size();i++) {
            const Item *item = items.at(i);
            int cost = costs.at(i);
            QString item_str = QString(item->getName().c_str()) + QString(" (") + QString::number(cost) + QString(" gold)");
            QListWidgetItem *list_item = new QListWidgetItem(item_str);
            QIcon icon( playing_gamestate->getItemImage( item->getImageName() ) );
            list_item->setIcon(icon);
            list->addItem(list_item);
        }
        connect(list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(clickedInfo()));
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *sellButton = new QPushButton("Sell");
        game_g->initButton(sellButton);
        sellButton->setFocusPolicy(Qt::NoFocus);
        sellButton->setToolTip("Sell the item selected of yours (on the left) (S)");
        sellButton->setShortcut(QKeySequence(Qt::Key_S));
        h_layout->addWidget(sellButton);
        connect(sellButton, SIGNAL(clicked()), this, SLOT(clickedSell()));

        QPushButton *buyButton = new QPushButton("Buy");
        game_g->initButton(buyButton);
        buyButton->setFocusPolicy(Qt::NoFocus);
        buyButton->setToolTip("Buy the item selected (on the right) (B)");
        buyButton->setShortcut(QKeySequence(Qt::Key_B));
        h_layout->addWidget(buyButton);
        connect(buyButton, SIGNAL(clicked()), this, SLOT(clickedBuy()));

        QPushButton *infoButton = new QPushButton("Info");
        game_g->initButton(infoButton);
        infoButton->setFocusPolicy(Qt::NoFocus);
        infoButton->setToolTip("Display more information about the selected item for sale (I)");
        infoButton->setShortcut(QKeySequence(Qt::Key_I));
        h_layout->addWidget(infoButton);
        connect(infoButton, SIGNAL(clicked()), this, SLOT(clickedInfo()));
    }

    QPushButton *closeButton = new QPushButton("Finish Trading");
    game_g->initButton(closeButton);
    closeButton->setFocusPolicy(Qt::NoFocus);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeSubWindow()));

    list->setFocus();
}

void TradeWindow::updateGoldLabel() {
    Character *player = playing_gamestate->getPlayer();
    goldLabel->setText("Gold: " + QString::number( player->getGold() ));
}

void TradeWindow::setWeightLabel() {
    Character *player = this->playing_gamestate->getPlayer();
    string weight_str = player->getWeightString();
    this->weightLabel->setText(weight_str.c_str());
}

void TradeWindow::addPlayerItem(Item *item, int buy_cost) {
    buy_cost *= 0.5f;
    QString item_str = QString(item->getName().c_str()) + QString(" (") + QString::number(buy_cost) + QString(" gold)");
    QListWidgetItem *list_item = new QListWidgetItem(item_str);
    QIcon icon( playing_gamestate->getItemImage( item->getImageName() ) );
    list_item->setIcon(icon);
    player_list->addItem(list_item);
    player_items.push_back(item);
    player_costs.push_back(buy_cost);
}

void TradeWindow::clickedBuy() {
    LOG("TradeWindow::clickedBuy()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < items.size() );

    const Item *selected_item = items.at(index);
    int cost = costs.at(index);
    Character *player = playing_gamestate->getPlayer();
    if( player->getGold() >= cost ) {
        LOG("player buys: %s\n", selected_item->getName().c_str());
        player->addGold(-cost);
        this->updateGoldLabel();
        Item *item = playing_gamestate->cloneStandardItem(selected_item->getKey());
        player->addItem(item);
        this->setWeightLabel();
        this->addPlayerItem(item, cost);
        list->setCurrentRow(-1);
    }
    else {
        LOG("not enough money\n");
        //game_g->showInfoDialog("Trade", "You do not have enough money to purchase this item.");
        playing_gamestate->showInfoDialog("You do not have enough money to purchase this item.");
    }
}

void TradeWindow::clickedSell() {
    LOG("TradeWindow::clickedSell()\n");

    int index = player_list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < player_items.size() );

    Item *selected_item = player_items.at(index);
    int cost = player_costs.at(index);
    if( cost > 0 ) {
        LOG("player sells: %s\n", selected_item->getName().c_str());
        Character *player = playing_gamestate->getPlayer();
        player->addGold(cost);
        this->updateGoldLabel();
        player->takeItem(selected_item);
        delete selected_item;
        this->setWeightLabel();
        QListWidgetItem *list_item = player_list->takeItem(index);
        delete list_item;
        player_items.erase(player_items.begin() + index);
        player_costs.erase(player_costs.begin() + index);
    }
    else {
        //game_g->showInfoDialog("Trade", "This shop doesn't buy that item.");
        playing_gamestate->showInfoDialog("This shop doesn't buy that item.");
    }
}

void TradeWindow::clickedInfo() {
    LOG("TradeWindow::clickedInfo()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < items.size() );

    const Item *selected_item = items.at(index);
    string info = selected_item->getDetailedDescription();
    info = convertToHTML(info);
    playing_gamestate->showInfoWindow(info);
}

ItemsPickerWindow::ItemsPickerWindow(PlayingGamestate *playing_gamestate, vector<Item *> items) :
    playing_gamestate(playing_gamestate), list(NULL), items(items), player_list(NULL), weightLabel(NULL)
{
    playing_gamestate->addWidget(this);

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    /*{
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *label = new QLabel("What would you like to buy?");
        label->setWordWrap(true);
        h_layout->addWidget(label);

        goldLabel = new QLabel("");
        h_layout->addWidget(goldLabel);
        this->updateGoldLabel();

        weightLabel = new QLabel("");
        h_layout->addWidget(weightLabel);
        this->setWeightLabel();
    }*/

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *groundLabel = new QLabel("Items to pick up:");
        h_layout->addWidget(groundLabel);

        QLabel *playerLabel = new QLabel("Your items:");
        h_layout->addWidget(playerLabel);
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        list = new ScrollingListWidget();
        list->grabKeyboard();
        if( !mobile_c ) {
            QFont list_font = list->font();
            list_font.setPointSize( list_font.pointSize() + 8 );
            list->setFont(list_font);
        }
        {
            QFontMetrics fm(list->font());
            int icon_size = fm.height();
            list->setIconSize(QSize(icon_size, icon_size));
        }
        h_layout->addWidget(list);
        for(size_t i=0;i<items.size();i++) {
            const Item *item = items.at(i);
            this->addGroundItem(item);
        }
        if( items.size() > 0 ) {
            list->setCurrentRow(0);
        }
        connect(list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(clickedPickUp()));

        player_list = new ScrollingListWidget();
        if( !mobile_c ) {
            QFont list_font = player_list->font();
            list_font.setPointSize( list_font.pointSize() + 8 );
            player_list->setFont(list_font);
        }
        {
            QFontMetrics fm(player_list->font());
            int icon_size = fm.height();
            player_list->setIconSize(QSize(icon_size, icon_size));
        }
        h_layout->addWidget(player_list);
        this->refreshPlayerItems();
    }

    weightLabel = new QLabel("");
    layout->addWidget(weightLabel);
    this->setWeightLabel();

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *pickUpButton = new QPushButton("Pick Up");
        game_g->initButton(pickUpButton);
        pickUpButton->setToolTip("Pick up the selected item (Space)");
        pickUpButton->setShortcut(QKeySequence(Qt::Key_Space));
        h_layout->addWidget(pickUpButton);
        connect(pickUpButton, SIGNAL(clicked()), this, SLOT(clickedPickUp()));

        QPushButton *dropButton = new QPushButton("Drop");
        game_g->initButton(dropButton);
        dropButton->setToolTip("Drop the selected item of yours onto the ground");
        h_layout->addWidget(dropButton);
        connect(dropButton, SIGNAL(clicked()), this, SLOT(clickedDrop()));

        QPushButton *infoButton = new QPushButton("Info");
        game_g->initButton(infoButton);
        infoButton->setToolTip("Display more information about the selected item (I)");
        infoButton->setShortcut(QKeySequence(Qt::Key_I));
        h_layout->addWidget(infoButton);
        connect(infoButton, SIGNAL(clicked()), this, SLOT(clickedInfo()));
    }

    QPushButton *closeButton = new QPushButton("Close");
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeSubWindow()));
}

void ItemsPickerWindow::addGroundItem(const Item *item) {
    QString item_str = QString(item->getName().c_str());
    QListWidgetItem *list_item = new QListWidgetItem(item_str);
    QIcon icon( playing_gamestate->getItemImage( item->getImageName() ) );
    list_item->setIcon(icon);
    list->addItem(list_item);
    // n.b., don't add to items list - should already be there, or otherwise caller should add it manually
}

void ItemsPickerWindow::addPlayerItem(Item *item) {
    QString item_str = QString(item->getName().c_str());
    QListWidgetItem *list_item = new QListWidgetItem(item_str);
    QIcon icon( playing_gamestate->getItemImage( item->getImageName() ) );
    list_item->setIcon(icon);
    player_list->addItem(list_item);
    player_items.push_back(item);
}

void ItemsPickerWindow::refreshPlayerItems() {
    player_list->clear();
    player_items.clear();
    Character *player = playing_gamestate->getPlayer();
    for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
        Item *item = *iter;
        addPlayerItem(item);
    }
}

void ItemsPickerWindow::clickedPickUp() {
    LOG("ItemsPickerWindow::clickedPickUp()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < items.size() );

    Item *selected_item = items.at(index);
    Character *player = playing_gamestate->getPlayer();
    LOG("player picks up: %s\n", selected_item->getName().c_str());
    if( selected_item->getType() == ITEMTYPE_CURRENCY ) {
        this->playing_gamestate->playSound("coin");
    }
    player->pickupItem(selected_item);
    this->setWeightLabel();
    this->refreshPlayerItems();

    items.erase(items.begin() + index);
    delete list->takeItem(index);

    playing_gamestate->checkQuestComplete();
}

void ItemsPickerWindow::clickedDrop() {
    LOG("ItemsPickerWindow::clickedDrop()\n");

    int index = player_list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < player_items.size() );

    Item *selected_item = player_items.at(index);
    LOG("player drops: %s\n", selected_item->getName().c_str());
    Character *player = playing_gamestate->getPlayer();
    player->dropItem(selected_item);
    this->setWeightLabel();
    player_items.erase(player_items.begin() + index);
    delete player_list->takeItem(index);

    items.push_back(selected_item);
    this->addGroundItem(selected_item);
}

void ItemsPickerWindow::clickedInfo() {
    LOG("ItemsPickerWindow::clickedInfo()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < items.size() );

    const Item *selected_item = items.at(index);
    string info = selected_item->getDetailedDescription();
    info = convertToHTML(info);
    playing_gamestate->showInfoWindow(info);
}

void ItemsPickerWindow::setWeightLabel() {
    Character *player = this->playing_gamestate->getPlayer();
    string weight_str = player->getWeightString();
    this->weightLabel->setText(weight_str.c_str());
}

CampaignWindow::CampaignWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate)
{
    playing_gamestate->addWidget(this);

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    QString close_text;
    if( playing_gamestate->getQuest()->isCompleted() && playing_gamestate->isLastQuest() ) {
        QLabel *label = new QLabel("You have completed all the quests!");
        label->setFont(game_g->getFontSmall());
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(label);

        close_text = "Finish game";
    }
    else {
        //QLabel *label = new QLabel("You have left the dungeon, and returned to your village to rest. You may also take the time to visit the local shops to buy supplies, sell any wares you have, as well as conducting training to improve your skills.");
        QLabel *label = new QLabel("You have left the dungeon, and returned to your village to rest. You may also take the time to visit the local shops to buy supplies, and sell any wares that you have.");
        label->setFont(game_g->getFontSmall());
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(label);

        for(vector<Shop *>::const_iterator iter = playing_gamestate->shopsBegin(); iter != playing_gamestate->shopsEnd(); ++iter) {
            const Shop *shop = *iter;
            QPushButton *shopButton = new QPushButton(shop->getName().c_str());
            game_g->initButton(shopButton);
            shopButton->setShortcut(QKeySequence(shop->getName().at(0)));
            QVariant variant = qVariantFromValue((void *)shop);
            shopButton->setProperty("shop", variant);
            //shopButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            layout->addWidget(shopButton);
            connect(shopButton, SIGNAL(clicked()), this, SLOT(clickedShop()));
        }

        /*QPushButton *trainingButton = new QPushButton("Training");
        game_g->initButton(trainingButton);
        //trainingButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(trainingButton);
        connect(trainingButton, SIGNAL(clicked()), this, SLOT(clickedTraining()));*/

        close_text = playing_gamestate->getQuest()->isCompleted() ? "Start next Quest" : "Continue your Quest";
    }

    QPushButton *closeButton = new QPushButton(close_text);
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(clickedClose()));
}

void CampaignWindow::clickedClose() {
    LOG("CampaignWindow::clickedClose()\n");
    this->playing_gamestate->closeSubWindow();
    if( playing_gamestate->getQuest()->isCompleted() ) {
        LOG("quest is completed\n");
        if( playing_gamestate->isLastQuest() ) {
            LOG("last quest, so game is complete\n");
            GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
            game_g->pushMessage(game_message);
        }
        else {
            LOG("move onto next quest\n");
            playing_gamestate->advanceQuest();
            const QuestInfo &c_quest_info = playing_gamestate->getCQuestInfo();
            string qt_filename = DEPLOYMENT_PATH + c_quest_info.getFilename();
            LOG("move onto next quest: %s\n", c_quest_info.getFilename().c_str());
            playing_gamestate->loadQuest(qt_filename, false);
        }
    }
    /*else {
        this->playing_gamestate->closeSubWindow();
    }*/
}

void CampaignWindow::clickedShop() {
    LOG("CampaignWindow::clickedShop()\n");
    QObject *sender = this->sender();
    ASSERT_LOGGER( sender != NULL );
    const Shop *shop = static_cast<const Shop *>(sender->property("shop").value<void *>());
    ASSERT_LOGGER( shop != NULL );
    LOG("visit shop: %s\n", shop->getName().c_str());
    new TradeWindow(playing_gamestate, shop->getItems(), shop->getCosts());
}

/*void CampaignWindow::clickedArmourer() {
    LOG("CampaignWindow::clickedArmourer()\n");
}

void CampaignWindow::clickedGeneralStores() {
    LOG("CampaignWindow::clickedGeneralStores()\n");
}

void CampaignWindow::clickedMagicShop() {
    LOG("CampaignWindow::clickedMagicShop()\n");
}*/

/*void CampaignWindow::clickedTraining() {
    LOG("CampaignWindow::clickedTraining()\n");
    //game_g->showInfoDialog("Training", "This feature is not yet implemented");
    playing_gamestate->showInfoDialog("This feature is not yet implemented");
}*/

SaveGameWindow::SaveGameWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate), list(NULL), edit(NULL)
{
    playing_gamestate->addWidget(this);

    QFont font = game_g->getFontBig();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    list = new ScrollingListWidget();
    list->grabKeyboard();
    if( !mobile_c ) {
        QFont list_font = list->font();
        list_font.setPointSize( list_font.pointSize() + 8 );
        list->setFont(list_font);
    }
    layout->addWidget(list);
    list->setSelectionMode(QAbstractItemView::SingleSelection);

    list->addItem("New Save Game File");

    QDir dir( QString(game_g->getApplicationFilename(savegame_folder).c_str()) );
    QStringList filter;
    filter << "*" + QString( savegame_ext.c_str() );
    QStringList files = dir.entryList(filter);
    for(int i=0;i<files.size();i++) {
        const QString &file = files.at(i);
        list->addItem(file);
    }

    list->setCurrentRow(0);

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *saveButton = new QPushButton("Save");
        game_g->initButton(saveButton);
        saveButton->setShortcut(QKeySequence(Qt::Key_Return));
        //saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(saveButton);
        connect(saveButton, SIGNAL(clicked()), this, SLOT(clickedSave()));

        QPushButton *deleteButton = new QPushButton("Delete File");
        game_g->initButton(deleteButton);
        deleteButton->setToolTip("Delete the selected save game file (D)");
        deleteButton->setShortcut(QKeySequence(Qt::Key_D));
        //deleteButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(deleteButton);
        connect(deleteButton, SIGNAL(clicked()), this, SLOT(clickedDelete()));
    }

    QPushButton *closeButton = new QPushButton("Cancel");
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Escape));
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeAllSubWindows()));
}

void SaveGameWindow::clickedSave() {
    LOG("SaveGameWindow::clickedSave()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < list->count());
    if( index == 0 ) {
        QString filename = QString(savegame_root.c_str());
        QDateTime date_time = QDateTime::currentDateTime();
        QDate date = date_time.date();
        filename += QString("%1").arg(QString::number(date.year()), 2, QChar('0'));
        filename += "-";
        filename += QString("%1").arg(QString::number(date.month()), 2, QChar('0'));
        filename += "-";
        filename += QString("%1").arg(QString::number(date.day()), 2, QChar('0'));
        filename += "_";
        QTime time = date_time.time();
        filename += QString("%1").arg(QString::number(time.hour()), 2, QChar('0'));
        filename += "-";
        filename += QString("%1").arg(QString::number(time.minute()), 2, QChar('0'));
        filename += "-";
        filename += QString("%1").arg(QString::number(time.second()), 2, QChar('0'));
        //filename += date_time.toString();

        QWidget *subwindow = new QWidget();
        playing_gamestate->addWidget(subwindow);

        QVBoxLayout *layout = new QVBoxLayout();
        subwindow->setLayout(layout);

        QLabel *label = new QLabel("Choose a filename:");
        layout->addWidget(label);

        this->edit = new QLineEdit(filename);
        edit->grabKeyboard(); // needed, due to previously having set the savegame list to grab the keyboard
        // disallow: \ / : * ? " < > |
        QRegExp rx("[^\\\\/:*?\"<>|]*");
        QValidator *validator = new QRegExpValidator(rx, this);
        this->edit->setValidator(validator);
        layout->addWidget(edit);
        this->edit->setFocus();
        connect(this->edit, SIGNAL(returnPressed()), this, SLOT(clickedSaveNew()));

        QPushButton *saveButton = new QPushButton("Save game");
        game_g->initButton(saveButton);
        saveButton->setShortcut(QKeySequence(Qt::Key_Return));
        saveButton->setFont(game_g->getFontBig());
        saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(saveButton);
        connect(saveButton, SIGNAL(clicked()), this, SLOT(clickedSaveNew()));

        QPushButton *closeButton = new QPushButton("Cancel");
        game_g->initButton(closeButton);
        closeButton->setShortcut(QKeySequence(Qt::Key_Escape));
        closeButton->setFont(game_g->getFontBig());
        closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(closeButton);
        connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeAllSubWindows()));
    }
    else {
        QString filename = list->item(index)->text();
        LOG("save as existing file: %s\n", filename.toStdString().c_str());
        if( playing_gamestate->saveGame(filename.toStdString()) ) {
            //game_g->showInfoDialog("Saved Game", "The game has been successfully saved.");
            playing_gamestate->showInfoDialog("The game has been successfully saved.");
        }
        else {
            game_g->showErrorDialog("Failed to save game!");
        }
        playing_gamestate->closeAllSubWindows();
    }
}

void SaveGameWindow::clickedDelete() {
    LOG("SaveGameWindow::clickedDelete()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < list->count());
    if( index >= 1 ) {
        QString filename = list->item(index)->text();
        //if( game_g->askQuestionDialog("Delete Save Game", "Are you sure you wish to delete save game: " + filename.toStdString() + "?") ) {
        if( playing_gamestate->askQuestionDialog("Are you sure you wish to delete save game: " + filename.toStdString() + "?") ) {
            LOG("delete existing file: %s\n", filename.toStdString().c_str());
            string full_path = game_g->getApplicationFilename(savegame_folder + filename.toStdString());
            LOG("full path: %s\n", full_path.c_str());
            remove(full_path.c_str());
            QListWidgetItem *list_item = list->takeItem(index);
            delete list_item;
        }
    }
}

void SaveGameWindow::clickedSaveNew() {
    LOG("SaveGameWindow::clickedSaveNew()\n");
    ASSERT_LOGGER(this->edit != NULL);
    QString filename = this->edit->text();
    filename += QString(savegame_ext.c_str());
    LOG("save as new file: %s\n", filename.toStdString().c_str());

    string full_path = game_g->getApplicationFilename(savegame_folder + filename.toStdString());
    QFile qfile( QString(full_path.c_str()) );
    bool ok = true;
    if( qfile.exists() ) {
        LOG("file exists!\n");
        //if( !game_g->askQuestionDialog("Save Game", "Are you sure you wish to overwrite an existing save game file?") ) {
        if( !playing_gamestate->askQuestionDialog("Are you sure you wish to overwrite an existing save game file?") ) {
            LOG("user says to not save\n");
            ok = false;
        }
    }
    if( ok ) {
        if( playing_gamestate->saveGame(filename.toStdString()) ) {
            //game_g->showInfoDialog("Saved Game", "The game has been successfully saved.");
            playing_gamestate->showInfoDialog("The game has been successfully saved.");
        }
        else {
            game_g->showErrorDialog("Failed to save game!");
        }
    }
    playing_gamestate->closeAllSubWindows();
}

PlayingGamestate::PlayingGamestate(bool is_savegame, size_t player_type) :
    scene(NULL), view(NULL), gui_overlay(NULL), main_stacked_widget(NULL),
    difficulty(DIFFICULTY_MEDIUM), player(NULL), c_quest_indx(0), c_location(NULL), quest(NULL)
{
    LOG("PlayingGamestate::PlayingGamestate()\n");
    playingGamestate = this;

    srand( clock() );

    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->setEnabled(false);
    game_g->getScreen()->setPaused(true);

    {
        const int res_c = 15;
        QPixmap pixmap(res_c, res_c);
        pixmap.fill(Qt::transparent);
        QRadialGradient radialGrad((res_c-1)/2, (res_c-1)/2, (res_c-1)/2);
        radialGrad.setColorAt(0.0, QColor(255, 255, 255, 63));
        radialGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
        QPainter painter(&pixmap);
        painter.setPen(Qt::NoPen);
        painter.fillRect(0, 0, res_c, res_c, radialGrad);
        painter.end();
        this->smoke_pixmap = pixmap;
    }

    // create UI
    LOG("create UI\n");
    QFont font = game_g->getFontStd();
    window->setFont(font);

    scene = new QGraphicsScene(window);
    //scene->setSceneRect(0, 0, scene_w_c, scene_h_c);
    //scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    //view = new QGraphicsView(scene, window);
    view = new MainGraphicsView(this, scene, window);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setBackgroundBrush(QBrush(Qt::black));
    view->setFrameStyle(QFrame::NoFrame);
    view->setFocusPolicy(Qt::NoFocus); // so clicking doesn't take focus away from the main window
    view->setCursor(Qt::OpenHandCursor);
    //view->grabGesture(Qt::PinchGesture);
    view->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    view->setCacheMode(QGraphicsView::CacheBackground);

    /*QWidget *centralWidget = new QWidget(window);
    this->mainwindow = centralWidget;
    LOG("mainwindow: %d\n", mainwindow);
    centralWidget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    window->setCentralWidget(centralWidget);*/
    this->main_stacked_widget = new QStackedWidget();
    main_stacked_widget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    window->setCentralWidget(main_stacked_widget);

    QWidget *centralWidget = new QWidget();
    main_stacked_widget->addWidget(centralWidget);

    QHBoxLayout *layout = new QHBoxLayout();
    centralWidget->setLayout(layout);

    {
        QVBoxLayout *v_layout = new QVBoxLayout();
        layout->addLayout(v_layout);

        QPushButton *statsButton = new QPushButton("Stats");
        game_g->initButton(statsButton);
        statsButton->setShortcut(QKeySequence(Qt::Key_F1));
        statsButton->setToolTip("Display statistics of your character (F1)");
        statsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //statsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(statsButton, SIGNAL(clicked()), this, SLOT(clickedStats()));
        v_layout->addWidget(statsButton);

        QPushButton *itemsButton = new QPushButton("Items");
        game_g->initButton(itemsButton);
        itemsButton->setShortcut(QKeySequence(Qt::Key_F2));
        itemsButton->setToolTip("Display the items that you are carrying (F2)");
        itemsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //itemsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(itemsButton, SIGNAL(clicked()), this, SLOT(clickedItems()));
        v_layout->addWidget(itemsButton);

        /*QPushButton *spellsButton = new QPushButton("Spells");
        game_g->initButton(spellsButton);
        spellsButton->setToolTip("Not supported yet");
        spellsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //spellsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        v_layout->addWidget(spellsButton);*/

        QPushButton *journalButton = new QPushButton("Journal");
        game_g->initButton(journalButton);
        journalButton->setShortcut(QKeySequence(Qt::Key_F3));
        journalButton->setToolTip("Displays information about your quests (F3)");
        journalButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //journalButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(journalButton, SIGNAL(clicked()), this, SLOT(clickedJournal()));
        v_layout->addWidget(journalButton);

        /*QPushButton *quitButton = new QPushButton("Quit");
        game_g->initButton(quitButton);
        quitButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        v_layout->addWidget(quitButton);
        connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));*/

        QPushButton *pauseButton = new QPushButton("Pause");
        game_g->initButton(pauseButton);
        pauseButton->setShortcut(QKeySequence(Qt::Key_P));
        pauseButton->setToolTip("Pause the game (P)");
        pauseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        connect(pauseButton, SIGNAL(clicked()), game_g->getScreen(), SLOT(togglePaused()));
        v_layout->addWidget(pauseButton);

        QPushButton *restButton = new QPushButton("Rest");
        game_g->initButton(restButton);
        restButton->setShortcut(QKeySequence(Qt::Key_R));
        restButton->setToolTip("Rest until you are healed (R)");
        restButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        connect(restButton, SIGNAL(clicked()), this, SLOT(clickedRest()));
        v_layout->addWidget(restButton);

        QPushButton *optionsButton = new QPushButton("Options");
        game_g->initButton(optionsButton);
        optionsButton->setShortcut(QKeySequence(Qt::Key_Escape));
        optionsButton->setToolTip("Options to save game or quit");
        optionsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //optionsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(optionsButton, SIGNAL(clicked()), this, SLOT(clickedOptions()));
        v_layout->addWidget(optionsButton);
    }

    //layout->addWidget(view);
    {
        QVBoxLayout *v_layout = new QVBoxLayout();
        layout->addLayout(v_layout);

        v_layout->addWidget(view);

        {
            QHBoxLayout *h_layout = new QHBoxLayout();
            v_layout->addLayout(h_layout);

            QPushButton *zoomoutButton = new QPushButton("-");
            game_g->initButton(zoomoutButton);
            zoomoutButton->setShortcut(QKeySequence(Qt::Key_Less));
            zoomoutButton->setToolTip("Zoom out");
            connect(zoomoutButton, SIGNAL(clicked()), view, SLOT(zoomOut()));
            h_layout->addWidget(zoomoutButton);

            QPushButton *zoominButton = new QPushButton("+");
            game_g->initButton(zoominButton);
            zoominButton->setShortcut(QKeySequence(Qt::Key_Greater));
            zoominButton->setToolTip("Zoom in");
            connect(zoominButton, SIGNAL(clicked()), view, SLOT(zoomIn()));
            h_layout->addWidget(zoominButton);

            QPushButton *centreButton = new QPushButton("O");
            game_g->initButton(centreButton);
            centreButton->setShortcut(QKeySequence(Qt::Key_C));
            centreButton->setToolTip("Centre view on your player's location");
            centreButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            connect(centreButton, SIGNAL(clicked()), view, SLOT(centreOnPlayer()));
            h_layout->addWidget(centreButton);
        }
    }

    view->showFullScreen();

    gui_overlay = new GUIOverlay(this, view);
    gui_overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    view->setGUIOverlay(gui_overlay);

    LOG("display UI\n");
    gui_overlay->setProgress(0);
    qApp->processEvents();
    qApp->processEvents(); // need to call this twice to get window to update, on Windows at least, for some reason?

    // create RPG data
    LOG("create RPG data\n");

    if( !is_savegame ) {
        LOG("create player\n");
        /*this->player = new Character("Warrior", "", false);
        //this->player->setProfile(7, 7, 7, 1, 6, 7, 7, 2.75f);
        this->player->setProfile(7, 7, 7, 1, 6, 7, 7, 2.0f);
        player->initialiseHealth(60);
        //player->addGold( rollDice(2, 6, 10) );
        player->addGold( 333 ); // CHEAT, simulate start of quest 2
        player->setXP(68); // CHEAT, simulate start of quest 2
        */
        this->player = game_g->createPlayer(player_type);
        //this->player->initialiseHealth(600); // CHEAT
        //player->addGold( 1000 ); // CHEAT
    }

    LOG("load images\n");
    {
        QString filename = QString(DEPLOYMENT_PATH) + "data/images.xml";
        QFile file(filename);
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            qDebug("failed to open: %s", filename.toStdString().c_str());
            throw string("Failed to open images xml file");
        }
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                if( reader.name() == "image" ) {
                    QStringRef type_s = reader.attributes().value("type");
                    if( type_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("image element has no type attribute or is zero length");
                    }
                    QString type = type_s.toString();
                    QStringRef name_s = reader.attributes().value("name");
                    if( name_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("image element has no name attribute or is zero length");
                    }
                    QString name = name_s.toString(); // need to take copy of string reference
                    QStringRef imagetype_s = reader.attributes().value("imagetype");
                    QString imagetype = imagetype_s.toString();
                    qDebug("image element type: %s name: %s imagetype: %s", type_s.toString().toStdString().c_str(), name_s.toString().toStdString().c_str(), imagetype_s.toString().toStdString().c_str());
                    QString filename;
                    QPixmap pixmap;
                    bool clip = false;
                    int xpos = 0, ypos = 0, width = 0, height = 0, expected_width = 0;
                    int stride_x = 0;
                    if( imagetype_s.length() == 0 ) {
                        // load file
                        QStringRef filename_s = reader.attributes().value("filename");
                        if( filename_s.length() == 0 ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("image element has no filename attribute or is zero length");
                        }
                        filename = filename_s.toString();
                        qDebug("    filename: %s", filename.toStdString().c_str());
                        QStringRef xpos_s = reader.attributes().value("xpos");
                        QStringRef ypos_s = reader.attributes().value("ypos");
                        QStringRef width_s = reader.attributes().value("width");
                        QStringRef height_s = reader.attributes().value("height");
                        QStringRef expected_width_s = reader.attributes().value("expected_width");
                        if( xpos_s.length() > 0 || ypos_s.length() > 0 || width_s.length() > 0 || height_s.length() > 0 ) {
                            clip = true;
                            xpos = parseInt(xpos_s.toString(), true);
                            ypos = parseInt(ypos_s.toString(), true);
                            width = parseInt(width_s.toString());
                            height = parseInt(height_s.toString());
                            expected_width = parseInt(expected_width_s.toString());
                            qDebug("    clip to: %d, %d, %d, %d (expected width %d)", xpos, ypos, width, height, expected_width);
                        }
                        else {
                            if( expected_width_s.length() > 0 ) {
                                expected_width = parseInt(expected_width_s.toString());
                            }
                            else {
                                expected_width = 128;
                            }
                        }

                        QStringRef stride_x_s = reader.attributes().value("stride_x");
                        if( stride_x_s.length() > 0 ) {
                            stride_x = parseInt(stride_x_s.toString());
                        }
                        else {
                            stride_x = 128;
                        }
                        // image loaded later
                        /*QString filename_qt = ":/" + filename;
                        if( type_s == "npc" ) {
                            // clipping is done on a per-frame basis, for animation
                            pixmap = game_g->loadImage(filename_qt.toStdString().c_str());
                        }
                        else {
                            pixmap = game_g->loadImage(filename_qt.toStdString().c_str(), clip, xpos, ypos, width, height, expected_width);
                        }*/
                    }
                    else if( imagetype_s == "solid" ) {
                        QStringRef red_s = reader.attributes().value("red");
                        QStringRef green_s = reader.attributes().value("green");
                        QStringRef blue_s = reader.attributes().value("blue");
                        int red = parseInt(red_s.toString());
                        int green = parseInt(green_s.toString());
                        int blue = parseInt(blue_s.toString());
                        pixmap = QPixmap(16, 16);
                        pixmap.fill(QColor(red, green, blue));
                    }
                    else if( imagetype_s == "perlin" ) {
                        QStringRef max_red_s = reader.attributes().value("max_red");
                        QStringRef max_green_s = reader.attributes().value("max_green");
                        QStringRef max_blue_s = reader.attributes().value("max_blue");
                        QStringRef min_red_s = reader.attributes().value("min_red");
                        QStringRef min_green_s = reader.attributes().value("min_green");
                        QStringRef min_blue_s = reader.attributes().value("min_blue");
                        unsigned char filter_max[3] = {255, 255, 255};
                        unsigned char filter_min[3] = {255, 255, 255};
                        filter_max[0] = parseInt(max_red_s.toString());
                        filter_max[1] = parseInt(max_green_s.toString());
                        filter_max[2] = parseInt(max_blue_s.toString());
                        filter_min[0] = parseInt(min_red_s.toString());
                        filter_min[1] = parseInt(min_green_s.toString());
                        filter_min[2] = parseInt(min_blue_s.toString());
                        pixmap = createNoise(64, 64, 4.0f, 4.0f, filter_max, filter_min, NOISEMODE_PERLIN, 4);
                    }
                    else {
                        LOG("error at line %d\n", reader.lineNumber());
                        LOG("image element has unknown imagetype: %s\n", imagetype_s.string()->toStdString().c_str());
                        throw string("image element has unknown imagetype");
                    }
                    // now read any animations (only relevant for some types)
                    vector<AnimationLayerDefinition> animation_layer_definition;
                    while( !reader.atEnd() && !reader.hasError() && !(reader.isEndElement() && reader.name() == "image") ) {
                        reader.readNext();
                        if( reader.isStartElement() )
                        {
                            if( reader.name() == "animation" ) {
                                QStringRef sub_name_s = reader.attributes().value("name");
                                QStringRef sub_start_s = reader.attributes().value("start");
                                QStringRef sub_length_s = reader.attributes().value("length");
                                QStringRef sub_type_s = reader.attributes().value("type");
                                int sub_start = parseInt(sub_start_s.toString());
                                int sub_length = parseInt(sub_length_s.toString());
                                AnimationSet::AnimationType animation_type = AnimationSet::ANIMATIONTYPE_SINGLE;
                                if( sub_type_s == "single" ) {
                                    animation_type = AnimationSet::ANIMATIONTYPE_SINGLE;
                                }
                                else if( sub_type_s == "loop" ) {
                                    animation_type = AnimationSet::ANIMATIONTYPE_LOOP;
                                }
                                else if( sub_type_s == "bounce" ) {
                                    animation_type = AnimationSet::ANIMATIONTYPE_BOUNCE;
                                }
                                else {
                                    LOG("error at line %d\n", reader.lineNumber());
                                    throw string("image has unknown animation type");
                                }
                                animation_layer_definition.push_back( AnimationLayerDefinition(sub_name_s.toString().toStdString(), sub_start, sub_length, animation_type) );
                            }
                            else {
                                LOG("error at line %d\n", reader.lineNumber());
                                throw string("unknown xml tag within image section");
                            }
                        }
                    }

                    if( filename.length() > 0 ) {
                        //QString filename_qt = ":/" + filename;
                        QString qt_filename = DEPLOYMENT_PATH + filename;
                        if( animation_layer_definition.size() > 0 ) {
                            qDebug("load with animations");
                            // clipping is done on a per-frame basis, for animation
                            pixmap = game_g->loadImage(qt_filename.toStdString());
                        }
                        else {
                            qDebug("load image");
                            pixmap = game_g->loadImage(qt_filename.toStdString(), clip, xpos, ypos, width, height, expected_width);
                        }
                    }
                    if( !clip && animation_layer_definition.size() > 0 ) {
                        // set the default stride values
                        width = 128;
                        height = 128;
                    }

                    if( type == "generic") {
                        if( animation_layer_definition.size() > 0 ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("animations not supported for this animation type");
                        }
                        this->builtin_images[name.toStdString()] = pixmap;
                    }
                    else if( type == "item") {
                        if( animation_layer_definition.size() > 0 ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("animations not supported for this animation type");
                        }
                        this->item_images[name.toStdString()] = pixmap;
                    }
                    else if( type == "scenery" ) {
                        if( animation_layer_definition.size() > 0 ) {
                            //qDebug("create scenery_animation_layers: %s", name.toStdString().c_str());
                            //this->scenery_animation_layers[name.toStdString()] = AnimationLayer::create(pixmap, animation_layer_definition, xpos, ypos, 64, 72, 64, 72, 1);
                            this->scenery_animation_layers[name.toStdString()] = AnimationLayer::create(pixmap, animation_layer_definition, xpos, ypos, width, height, stride_x, 128, expected_width, 1);
                        }
                        else {
                            this->scenery_images[name.toStdString()] = pixmap;
                        }
                    }
                    /*else if( type == "npc_static" ) {
                        this->npc_static_images[name.toStdString()] = pixmap;
                    }*/
                    else if( type == "npc" ) {
                        if( animation_layer_definition.size() > 0 ) {
                            //this->animation_layers[name.toStdString()] = AnimationLayer::create(pixmap, animation_layer_definition, xpos, ypos, width, height, expected_width, expected_width, N_DIRECTIONS);
                            this->animation_layers[name.toStdString()] = AnimationLayer::create(pixmap, animation_layer_definition, xpos, ypos, width, height, stride_x, 128, expected_width, N_DIRECTIONS);
                        }
                        else {
                            this->npc_static_images[name.toStdString()] = pixmap;
                        }
                    }
                    else {
                        LOG("error at line %d\n", reader.lineNumber());
                        LOG("unknown type attribute: %s\n", type.toStdString().c_str());
                        throw string("image element has unknown type attribute");
                    }
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error at line %d\n", reader.lineNumber());
            LOG("error reading images.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading images xml file");
        }
    }

    gui_overlay->setProgress(20);
    qApp->processEvents();

    LOG("load items\n");
    {
        QFile file(QString(DEPLOYMENT_PATH) + "data/items.xml");
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open items xml file");
        }
        enum ItemsXMLType {
            ITEMS_XML_TYPE_NONE = 0,
            ITEMS_XML_TYPE_SHOP = 1,
            ITEMS_XML_TYPE_PLAYER_DEFAULT = 2
        };
        ItemsXMLType itemsXMLType = ITEMS_XML_TYPE_NONE;
        Shop *shop = NULL;
        string player_default_type;
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                /*qDebug("read xml: %s", reader.name().toString().toStdString().c_str());
                qDebug("    type: %d", reader.tokenType());
                qDebug("    n attributes: %d", reader.attributes().size());*/
                if( reader.name() == "shop" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected items xml: shop element wasn't expected here");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    shop = new Shop(name_s.toString().toStdString());
                    shops.push_back(shop);
                    itemsXMLType = ITEMS_XML_TYPE_SHOP;
                }
                else if( reader.name() == "purchase" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_SHOP ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected items xml: purchase element wasn't expected here");
                    }
                    QStringRef template_s = reader.attributes().value("template");
                    QStringRef cost_s = reader.attributes().value("cost");
                    if( template_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("purchase element has no template attribute");
                    }
                    int cost = parseInt(cost_s.toString());
                    Item *item = this->cloneStandardItem(template_s.toString().toStdString());
                    shop->addItem(item, cost);
                }
                else if( reader.name() == "player_default_items" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected items xml: player_default_items element wasn't expected here");
                    }
                    itemsXMLType = ITEMS_XML_TYPE_PLAYER_DEFAULT;
                    QStringRef type_s = reader.attributes().value("type");
                    player_default_type = type_s.toString().toStdString();
                }
                else if( reader.name() == "player_default_item" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_PLAYER_DEFAULT ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected items xml: player_default_item element wasn't expected here");
                    }
                    if( !is_savegame && player_default_type == player->getName() ) {
                        QStringRef template_s = reader.attributes().value("template");
                        if( template_s.length() == 0 ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("player_default_item element has no template attribute");
                        }
                        Item *item = this->cloneStandardItem(template_s.toString().toStdString());
                        player->addItem(item);
                    }
                }
                else {
                    if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected items xml: element wasn't expected here");
                    }
                    Item *item = parseXMLItem( reader );
                    if( item != NULL ) {
                        this->addStandardItem( item );
                    }
                    // else ignore unknown element
                }
            }
            else if( reader.isEndElement() ) {
                if( reader.name() == "shop" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_SHOP ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected items xml: shop end element wasn't expected here");
                    }
                    shop = NULL;
                    itemsXMLType = ITEMS_XML_TYPE_NONE;
                }
                else if( reader.name() == "player_default_items" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_PLAYER_DEFAULT ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected items xml: player_default_items end element wasn't expected here");
                    }
                    itemsXMLType = ITEMS_XML_TYPE_NONE;
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error at line %d\n", reader.lineNumber());
            LOG("error reading items.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading items xml file");
        }
    }

    gui_overlay->setProgress(30);
    qApp->processEvents();

    LOG("load NPCs\n");
    {
        QFile file(QString(DEPLOYMENT_PATH) + "data/npcs.xml");
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open npcs xml file");
        }
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                if( reader.name() == "npc" ) {
                    QStringRef name_s = reader.attributes().value("name");
                    qDebug("found npc template: %s", name_s.toString().toStdString().c_str());
                    QStringRef animation_name_s = reader.attributes().value("animation_name");
                    QStringRef static_image_s = reader.attributes().value("static_image");
                    bool static_image = parseBool(static_image_s.toString(), true);
                    QStringRef bounce_s = reader.attributes().value("bounce");
                    bool bounce = parseBool(bounce_s.toString(), true);
                    QStringRef FP_s = reader.attributes().value("FP");
                    int FP = parseInt(FP_s.toString());
                    QStringRef BS_s = reader.attributes().value("BS");
                    int BS = parseInt(BS_s.toString());
                    QStringRef S_s = reader.attributes().value("S");
                    int S = parseInt(S_s.toString());
                    QStringRef A_s = reader.attributes().value("A");
                    int A = parseInt(A_s.toString());
                    QStringRef M_s = reader.attributes().value("M");
                    int M = parseInt(M_s.toString());
                    QStringRef D_s = reader.attributes().value("D");
                    int D = parseInt(D_s.toString());
                    QStringRef B_s = reader.attributes().value("B");
                    int B = parseInt(B_s.toString());
                    QStringRef Sp_s = reader.attributes().value("Sp");
                    float Sp = parseFloat(Sp_s.toString());
                    QStringRef health_min_s = reader.attributes().value("health_min");
                    int health_min = parseInt(health_min_s.toString());
                    QStringRef health_max_s = reader.attributes().value("health_max");
                    int health_max = parseInt(health_max_s.toString());
                    QStringRef gold_min_s = reader.attributes().value("gold_min");
                    int gold_min = parseInt(gold_min_s.toString());
                    QStringRef gold_max_s = reader.attributes().value("gold_max");
                    int gold_max = parseInt(gold_max_s.toString());
                    QStringRef xp_worth_s = reader.attributes().value("xp_worth");
                    int xp_worth = parseInt(xp_worth_s.toString());
                    QStringRef causes_terror_s = reader.attributes().value("causes_terror");
                    bool causes_terror = parseBool(causes_terror_s.toString(), true);
                    QStringRef terror_effect_s = reader.attributes().value("terror_effect");
                    int terror_effect = parseInt(terror_effect_s.toString(), true);
                    QStringRef requires_magical_s = reader.attributes().value("requires_magical");
                    bool requires_magical = parseBool(requires_magical_s.toString(), true);
                    CharacterTemplate *character_template = new CharacterTemplate(animation_name_s.toString().toStdString(), FP, BS, S, A, M, D, B, Sp, health_min, health_max, gold_min, gold_max, xp_worth, causes_terror, terror_effect);
                    character_template->setStaticImage(static_image);
                    character_template->setBounce(bounce);
                    character_template->setRequiresMagical(requires_magical);
                    QStringRef natural_damageX_s = reader.attributes().value("natural_damageX");
                    QStringRef natural_damageY_s = reader.attributes().value("natural_damageY");
                    QStringRef natural_damageZ_s = reader.attributes().value("natural_damageZ");
                    if( natural_damageX_s.length() > 0 || natural_damageY_s.length() > 0 || natural_damageZ_s.length() > 0 ) {
                        int natural_damageX = parseInt(natural_damageX_s.toString());
                        int natural_damageY = parseInt(natural_damageY_s.toString());
                        int natural_damageZ = parseInt(natural_damageZ_s.toString());
                        character_template->setNaturalDamage(natural_damageX, natural_damageY, natural_damageZ);
                    }
                    QStringRef can_fly_s = reader.attributes().value("can_fly");
                    bool can_fly = parseBool(can_fly_s.toString(), true);
                    character_template->setCanFly(can_fly);
                    this->character_templates[ name_s.toString().toStdString() ] = character_template;
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error at line %d\n", reader.lineNumber());
            LOG("error reading npcs.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading npcs xml file");
        }
    }

    gui_overlay->setProgress(40);
    qApp->processEvents();

    LOG("load Spells");
    {
        QFile file(QString(DEPLOYMENT_PATH) + "data/spells.xml");
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open spells xml file");
        }
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                if( reader.name() == "spell" ) {
                    QStringRef name_s = reader.attributes().value("name");
                    qDebug("found spell template: %s", name_s.toString().toStdString().c_str());
                    QStringRef type_s = reader.attributes().value("type");
                    QStringRef rollX_s = reader.attributes().value("rollX");
                    QStringRef rollY_s = reader.attributes().value("rollY");
                    QStringRef rollZ_s = reader.attributes().value("rollZ");
                    int rollX = parseInt(rollX_s.toString(), true);
                    int rollY = parseInt(rollY_s.toString(), true);
                    int rollZ = parseInt(rollZ_s.toString(), true);
                    Spell *spell = new Spell(name_s.toString().toStdString(), type_s.toString().toStdString());
                    spell->setRoll(rollX, rollY, rollZ);
                    this->spells[ name_s.toString().toStdString() ] = spell;
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error at line %d\n", reader.lineNumber());
            LOG("error reading npcs.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading npcs xml file");
        }
    }

    gui_overlay->setProgress(60);
    qApp->processEvents();

    LOG("create animation frames\n");
    LOG("load player image\n");
    vector<AnimationLayerDefinition> player_animation_layer_definition;
    const int off_x = 32, off_y = 40, width = 64, height = 64;
    //int off_x = 0, off_y = 0, width = 128, height = 128;
    const int expected_stride_x = 128, expected_stride_y = 128, expected_total_width = 4096;
    //float off_x = 32.0f/128.0f, off_y = 40.0f/128.0f, width = 64.0f/128.0f, height = 64.0f/128.0f;
    //float off_x = 0.0f/128.0f, off_y = 0.0f/128.0f, width = 128.0f/128.0f, height = 128.0f/128.0f;
    player_animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 4, AnimationSet::ANIMATIONTYPE_BOUNCE) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("run", 4, 8, AnimationSet::ANIMATIONTYPE_LOOP) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("attack", 12, 4, AnimationSet::ANIMATIONTYPE_SINGLE) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("ranged", 28, 4, AnimationSet::ANIMATIONTYPE_SINGLE) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("death", 18, 6, AnimationSet::ANIMATIONTYPE_SINGLE) );
    //LOG("clothes layer\n");
    //int time_s = clock();
    //this->animation_layers["clothes"] = AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/clothes.png", player_animation_layer_definition, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS);
    //this->animation_layers["head"] = AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/male_head1.png", player_animation_layer_definition, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS);
    this->animation_layers["player"] = AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/player.png", player_animation_layer_definition, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS);
    //LOG("time to load: %d\n", clock() - time_s);
    //LOG("longsword layer\n");
    this->animation_layers["longsword"] = AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/longsword.png", player_animation_layer_definition, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS);
    //LOG("longbow layer\n");
    this->animation_layers["longbow"] = AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/longbow.png", player_animation_layer_definition, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS);
    //LOG("shield layer\n");
    this->animation_layers["shield"] = AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/shield.png", player_animation_layer_definition, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS);

    gui_overlay->setProgress(70);
    qApp->processEvents();

    LOG("load sound effects\n");
    //if( !mobile_c )
    if( game_g->isSoundEnabled() )
    {
        game_g->loadSound("click", string(DEPLOYMENT_PATH) + "sound/click_short.wav");
        game_g->loadSound("coin", string(DEPLOYMENT_PATH) + "sound/coin.wav");
        game_g->loadSound("container", string(DEPLOYMENT_PATH) + "sound/container.wav");
        game_g->loadSound("door", string(DEPLOYMENT_PATH) + "sound/door.wav");
        game_g->loadSound("drink", string(DEPLOYMENT_PATH) + "sound/bubble2.wav");
        game_g->loadSound("lock", string(DEPLOYMENT_PATH) + "sound/lock.wav");
#if !defined(Q_OS_SYMBIAN) && !defined(Q_WS_SIMULATOR)
        game_g->loadSound("swing", string(DEPLOYMENT_PATH) + "sound/swing2.wav");  // playing this sample causes strange pauses on Symbian?? (Nokia 5800)
#endif
        game_g->loadSound("turn_page", string(DEPLOYMENT_PATH) + "sound/turn_page.wav");
        game_g->loadSound("weapon_unsheath", string(DEPLOYMENT_PATH) + "sound/sword-unsheathe5.wav");
        game_g->loadSound("wear_armour", string(DEPLOYMENT_PATH) + "sound/chainmail1.wav");
        // remember to call freeSound in the PlayingGamestate destructor!
    }

    gui_overlay->setProgress(90);
    qApp->processEvents();

    LOG("load quests\n");

    {
        QFile file(QString(DEPLOYMENT_PATH) + "data/quests.xml");
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open quests xml file");
        }
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                if( reader.name() == "quest" ) {
                    QStringRef filename_s = reader.attributes().value("filename");
                    qDebug("found quest: %s", filename_s.toString().toStdString().c_str());
                    if( filename_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("quest doesn't have filename info");
                    }
                    QuestInfo quest_info(filename_s.toString().toStdString());
                    this->quest_list.push_back(quest_info);
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error at line %d\n", reader.lineNumber());
            LOG("error reading quests.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading quests xml file");
        }
    }
    if( this->quest_list.size() == 0 ) {
        throw string("failed to find any quests");
    }
    gui_overlay->setProgress(100);
    qApp->processEvents();

    window->setEnabled(true);
    game_g->getScreen()->setPaused(false);

}

PlayingGamestate::~PlayingGamestate() {
    LOG("PlayingGamestate::~PlayingGamestate()\n");
    //this->closeSubWindow();
    this->closeAllSubWindows();
    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->centralWidget()->deleteLater();
    window->setCentralWidget(NULL);

    playingGamestate = NULL;

    delete quest;
    quest = NULL;
    c_location = NULL;

    for(map<string, AnimationLayer *>::iterator iter = this->animation_layers.begin(); iter != this->animation_layers.end(); ++iter) {
        AnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }
    for(map<string, AnimationLayer *>::iterator iter = this->scenery_animation_layers.begin(); iter != this->scenery_animation_layers.end(); ++iter) {
        AnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }
    for(map<string, Item *>::iterator iter = this->standard_items.begin(); iter != this->standard_items.end(); ++iter) {
        Item *item = iter->second;
        qDebug("about to delete standard item: %d", item);
        qDebug("    name: %s", item->getName().c_str());
        delete item;
    }
    for(map<string, CharacterTemplate *>::iterator iter = this->character_templates.begin(); iter != this->character_templates.end(); ++iter) {
        CharacterTemplate *character_template = iter->second;
        qDebug("about to delete character template: %d", character_template);
        delete character_template;
    }
    for(vector<Shop *>::iterator iter = shops.begin(); iter != shops.end(); ++iter) {
        Shop *shop = *iter;
        delete shop;
    }
    if( game_g->isSoundEnabled() )
    {
        game_g->freeSound("click");
        game_g->freeSound("coin");
        game_g->freeSound("container");
        game_g->freeSound("door");
        game_g->freeSound("drink");
        game_g->freeSound("lock");
        game_g->freeSound("swing");
        game_g->freeSound("turn_page");
        game_g->freeSound("weapon_unsheath");
        game_g->freeSound("wear_armour");
    }
    LOG("done\n");
}

float PlayingGamestate::getDifficultyModifier() const {
    float factor = 1.0;
    switch( difficulty ) {
    case DIFFICULTY_EASY:
        factor = 1.2f;
        break;
    case DIFFICULTY_MEDIUM:
        factor = 1.0f;
        break;
    case DIFFICULTY_HARD:
        factor = 0.8f;
        break;
    default:
        ASSERT_LOGGER(false);
        break;
    }
    return factor;
}

void PlayingGamestate::playBackgroundMusic() {
    // needed for looping
    qDebug("PlayingGamestate::playBackgroundMusic()");
/*#ifndef Q_OS_ANDROID
    if( game_g->isSoundEnabled() ) {
        this->sound_effects["background"]->stop();
        this->sound_effects["background"]->play();
    }
#endif*/
}

Item *PlayingGamestate::parseXMLItem(QXmlStreamReader &reader) {
    Item *item = NULL;

    QStringRef base_template_sr = reader.attributes().value("base_template");
    QStringRef arg1_sr = reader.attributes().value("arg1");
    QStringRef arg2_sr = reader.attributes().value("arg2");
    QStringRef arg1_s_sr = reader.attributes().value("arg1_s");
    QStringRef weight_sr = reader.attributes().value("weight");
    QStringRef rating_sr = reader.attributes().value("rating");
    QStringRef magical_sr = reader.attributes().value("magical");
    QStringRef worth_bonus_sr = reader.attributes().value("worth_bonus");
    QString base_template = base_template_sr.toString();
    int weight = parseInt(weight_sr.toString(), true);
    int arg1 = parseInt(arg1_sr.toString(), true);
    int arg2 = parseInt(arg2_sr.toString(), true);
    QString arg1_s = arg1_s_sr.toString();
    int rating = parseInt(rating_sr.toString(), true);
    if( rating == 0 )
        rating = 1; // so the default of 0 defaults instead to 1
    bool magical = parseBool(magical_sr.toString(), true);
    int worth_bonus = parseInt(worth_bonus_sr.toString(), true);

    if( reader.name() == "item" ) {
        QStringRef name_s = reader.attributes().value("name");
        QStringRef image_name_s = reader.attributes().value("image_name");
        QStringRef icon_width_s = reader.attributes().value("icon_width");
        QStringRef use_s = reader.attributes().value("use");
        QStringRef use_verb_s = reader.attributes().value("use_verb");
        item = new Item(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight);
        if( icon_width_s.length() > 0 ) {
            float icon_width = parseFloat(icon_width_s.toString());
            //LOG("icon_width: %f\n", icon_width);
            item->setIconWidth(icon_width);
        }
        if( use_s.length() > 0 ) {
            item->setUse(use_s.toString().toStdString(), use_verb_s.toString().toStdString());
        }
        // must be done last
        QString description = reader.readElementText(QXmlStreamReader::IncludeChildElements);
        item->setDescription(description.toStdString());
    }
    else if( reader.name() == "weapon" ) {
        //qDebug("    weapon:");
        QStringRef name_s = reader.attributes().value("name");
        QStringRef image_name_s = reader.attributes().value("image_name");
        QStringRef animation_name_s = reader.attributes().value("animation_name");
        QStringRef two_handed_s = reader.attributes().value("two_handed");
        QStringRef ranged_s = reader.attributes().value("ranged");
        QStringRef ammo_s = reader.attributes().value("ammo");
        QStringRef damageX_s = reader.attributes().value("damageX");
        QStringRef damageY_s = reader.attributes().value("damageY");
        QStringRef damageZ_s = reader.attributes().value("damageZ");
        QStringRef min_strength_s = reader.attributes().value("min_strength");
        /*qDebug("    name: %s", name_s.toString().toStdString().c_str());
        qDebug("    image_name: %s", image_name_s.toString().toStdString().c_str());
        qDebug("    animation_name: %s", animation_name_s.toString().toStdString().c_str());
        qDebug("    weight: %s", weight_s.toString().toStdString().c_str());
        qDebug("    two_handed_s: %s", two_handed_s.toString().toStdString().c_str());
        qDebug("    ranged_s: %s", ranged_s.toString().toStdString().c_str());
        qDebug("    ammo_s: %s", ammo_s.toString().toStdString().c_str());*/
        bool two_handed = parseBool(two_handed_s.toString(), true);
        bool ranged = parseBool(ranged_s.toString(), true);
        int damageX = parseInt(damageX_s.toString());
        int damageY = parseInt(damageY_s.toString());
        int damageZ = parseInt(damageZ_s.toString());
        int min_strength = parseInt(min_strength_s.toString(), true);
        Weapon *weapon = new Weapon(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, animation_name_s.toString().toStdString(), damageX, damageY, damageZ);
        item = weapon;
        weapon->setTwoHanded(two_handed);
        weapon->setRanged(ranged);
        if( ammo_s.length() > 0 ) {
            weapon->setRequiresAmmo(true, ammo_s.toString().toStdString());
        }
        weapon->setMinStrength(min_strength);
        // must be done last
        QString description = reader.readElementText(QXmlStreamReader::IncludeChildElements);
        weapon->setDescription(description.toStdString());
    }
    else if( reader.name() == "shield" ) {
        QStringRef name_s = reader.attributes().value("name");
        QStringRef image_name_s = reader.attributes().value("image_name");
        QStringRef animation_name_s = reader.attributes().value("animation_name");
        Shield *shield = new Shield(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, animation_name_s.toString().toStdString());
        item = shield;
        // must be done last
        QString description = reader.readElementText(QXmlStreamReader::IncludeChildElements);
        shield->setDescription(description.toStdString());
    }
    else if( reader.name() == "armour" ) {
        //qDebug("    armour:");
        QStringRef name_s = reader.attributes().value("name");
        qDebug("    name: %s", name_s.toString().toStdString().c_str());
        QStringRef image_name_s = reader.attributes().value("image_name");
        QStringRef min_strength_s = reader.attributes().value("min_strength");
        int min_strength = parseInt(min_strength_s.toString(), true);
        Armour *armour = new Armour(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, rating);
        item = armour;
        armour->setMinStrength(min_strength);
        // must be done last
        QString description = reader.readElementText(QXmlStreamReader::IncludeChildElements);
        armour->setDescription(description.toStdString());
    }
    else if( reader.name() == "ammo" ) {
        QStringRef name_s = reader.attributes().value("name");
        QStringRef image_name_s = reader.attributes().value("image_name");
        QStringRef projectile_image_name_s = reader.attributes().value("projectile_image_name");
        QStringRef amount_s = reader.attributes().value("amount");
        int amount = parseInt(amount_s.toString());
        Ammo *ammo = new Ammo(name_s.toString().toStdString(), image_name_s.toString().toStdString(), projectile_image_name_s.toString().toStdString(), weight, amount);
        item = ammo;
        // must be done last
        QString description = reader.readElementText(QXmlStreamReader::IncludeChildElements);
        ammo->setDescription(description.toStdString());
    }
    else if( reader.name() == "currency" ) {
        QStringRef name_s = reader.attributes().value("name");
        QStringRef image_name_s = reader.attributes().value("image_name");
        QStringRef value_s = reader.attributes().value("value");
        Currency *currency = new Currency(name_s.toString().toStdString(), image_name_s.toString().toStdString());
        if( value_s.length() > 0 ) {
            // n.b., when currency is defined as a template, the value is left as 0, and overridden when we instantiate it by the special "gold" element
            int value = parseInt(value_s.toString());
            currency->setValue(value);
        }
        item = currency;
    }
    // else ignore unknown element - leave item as NULL
    if( item != NULL ) {
        // remember not to use the string regs here, as no longer value!
        item->setArg1(arg1);
        item->setArg2(arg2);
        item->setArg1s(arg1_s.toStdString());
        item->setRating(rating);
        item->setMagical(magical);
        item->setBaseTemplate(base_template.toStdString());
        item->setWorthBonus(worth_bonus);
    }
    return item;
}

void PlayingGamestate::setDifficulty(Difficulty difficulty) {
    LOG("set difficulty to: %d\n", difficulty);
    this->difficulty = difficulty;
}

void PlayingGamestate::moveToLocation(Location *location, Vector2D pos) {
    LOG("PlayingGamestate::moveToLocation(%s, %f, %f)\n", location->getName().c_str(), pos.x, pos.y);
    if( this->player->getLocation() != NULL ) {
        LOG("remove player from location\n");
        this->player->getLocation()->removeCharacter(this->player);
        this->player->setListener(NULL, NULL);
    }
    view->clear();
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        character->setListener(NULL, NULL);
    }
    this->c_location->setListener(NULL, NULL);

    this->c_location = location;
    this->c_location->addCharacter(player, pos.x, pos.y);
    this->c_location->setListener(this, NULL); // must do after creating the location and its contents, so it doesn't try to add items to the scene, etc

    this->setupView();
}

void PlayingGamestate::setupView() {
    qDebug("PlayingGamestate::setupView()");
    // set up the view on the RPG world
    MainWindow *window = game_g->getScreen()->getMainWindow();

    view->createLightingMap(this->c_location->getLightingMin());
    //view->centerOn(player->getPos().x, player->getPos().y);

    const float offset_y = 0.5f;
    float location_width = 0.0f, location_height = 0.0f;
    c_location->calculateSize(&location_width, &location_height);
    const float extra_offset_c = 5.0f; // so we can still scroll slightly past the boundary, and also that multitouch works beyond the boundary
    //scene->setSceneRect(0, -offset_y, location_width, location_height + 2*offset_y);
    scene->setSceneRect(-extra_offset_c, -offset_y-extra_offset_c, location_width+2*extra_offset_c, location_height + 2*offset_y + 2*extra_offset_c);
    {
        const float desired_width_c = mobile_c ? 10.0f : 20.0f;
        float initial_scale = window->width() / desired_width_c;
        LOG("width: %d\n", window->width());
        LOG("initial_scale: %f\n", initial_scale);
        view->setScale(initial_scale);
    }

    QBrush floor_brush(builtin_images[c_location->getFloorImageName()]);
    QBrush wall_brush;
    {
        /*int pixels_per_unit = 128;
        float scale = 4.0f/(float)pixels_per_unit;*/
        float scale = 4.0f/(float)builtin_images[c_location->getFloorImageName()].width();
        floor_brush.setTransform(QTransform::fromScale(scale, scale));
        if( c_location->getWallImageName().length() > 0 ) {
            wall_brush.setTexture(builtin_images[c_location->getWallImageName()]);
            wall_brush.setTransform(QTransform::fromScale(scale, scale));
        }

        float background_scale = 1.0f/32.0f;
        QBrush background_brush(builtin_images[c_location->getBackgroundImageName()]);
        background_brush.setTransform(QTransform::fromScale(background_scale, background_scale));
        view->setBackgroundBrush(background_brush);
        //view->setBackgroundBrush(QBrush(Qt::white));
    }

    for(size_t i=0;i<c_location->getNFloorRegions();i++) {
        FloorRegion *floor_region = c_location->getFloorRegion(i);
        QPolygonF polygon;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D point = floor_region->getPoint(j);
            //QPointF qpoint(point.x, point.y + offset_y);
            QPointF qpoint(point.x, point.y);
            polygon.push_back(qpoint);
        }
        QGraphicsPolygonItem *item = scene->addPolygon(polygon, Qt::NoPen, floor_brush);
        floor_region->setUserGfxData(item);
        item->setVisible(false); // default to false, visibility is checked afterwards
        if( c_location->getWallImageName().length() > 0 ) {
            for(size_t j=0;j<floor_region->getNPoints();j++) {
                if( floor_region->getEdgeType(j) == FloorRegion::EDGETYPE_INTERNAL ) {
                    continue;
                }
                size_t n_j = j==floor_region->getNPoints()-1 ? 0 : j+1;
                Vector2D p0 = floor_region->getPoint(j);
                Vector2D p1 = floor_region->getPoint(n_j);
                Vector2D dp = p1 - p0;
                float dp_length = dp.magnitude();
                if( dp_length == 0.0f ) {
                    continue;
                }
                dp /= dp_length;
                Vector2D normal_into_wall = - dp.perpendicularYToX();
                QPolygonF wall_polygon;
                const float wall_dist = 0.1f;
                wall_polygon.push_back(QPointF(p0.x, p0.y));
                wall_polygon.push_back(QPointF(p0.x + wall_dist * normal_into_wall.x, p0.y + wall_dist * normal_into_wall.y));
                wall_polygon.push_back(QPointF(p1.x + wall_dist * normal_into_wall.x, p1.y + wall_dist * normal_into_wall.y));
                wall_polygon.push_back(QPointF(p1.x, p1.y));
                QGraphicsPolygonItem *wall_item = new QGraphicsPolygonItem(wall_polygon, item);
                wall_item->setPen(Qt::NoPen);
                wall_item->setBrush(wall_brush);

                /*if( fabs(normal_into_wall.y) > E_TOL_LINEAR ) {
                    const float wall_height = 0.5f;
                    QPolygonF wall_polygon_3d;
                    if( normal_into_wall.y < 0.0f ) {
                        wall_polygon_3d.push_back(QPointF(p0.x, p0.y));
                        wall_polygon_3d.push_back(QPointF(p0.x, p0.y - wall_height));
                        wall_polygon_3d.push_back(QPointF(p1.x, p1.y - wall_height));
                        wall_polygon_3d.push_back(QPointF(p1.x, p1.y));
                        QGraphicsPolygonItem *wall_item_3d = new QGraphicsPolygonItem(wall_polygon_3d, item);
                        wall_item_3d->setPen(Qt::NoPen);
                        wall_item_3d->setBrush(wall_brush);
                    }
                    else {
                        //wall_polygon_3d.push_back(QPointF(p0.x, p0.y));
                        //wall_polygon_3d.push_back(QPointF(p0.x, p0.y + wall_height));
                        //wall_polygon_3d.push_back(QPointF(p1.x, p1.y + wall_height));
                        //wall_polygon_3d.push_back(QPointF(p1.x, p1.y));
                    }
                }*/
            }
        }
    }
#ifdef DEBUG_SHOW_PATH
    {
        // DEBUG ONLY
        QPen wall_pen(Qt::red);
        for(size_t i=0;i<c_location->getNBoundaries();i++) {
            const Polygon2D *boundary = c_location->getBoundary(i);
            //qDebug("boundary %d:", i);
            for(size_t j=0;j<boundary->getNPoints();j++) {
                Vector2D p0 = boundary->getPoint(j);
                Vector2D p1 = boundary->getPoint((j+1) % boundary->getNPoints());
                //scene->addLine(p0.x, p0.y + offset_y, p1.x, p1.y + offset_y, wall_pen);
                //qDebug("    %f, %f to %f, %f", p0.x, p0.y, p1.x, p1.y);
                scene->addLine(p0.x, p0.y, p1.x, p1.y, wall_pen);
            }
        }
    }
    {
        // DEBUG ONLY
        QPen wall_pen(Qt::red);
        const Graph *distance_graph = c_location->getDistanceGraph();
        for(size_t i=0;i<distance_graph->getNVertices();i++) {
            const GraphVertex *vertex = distance_graph->getVertex(i);
            Vector2D path_way_point = vertex->getPos();
            const float radius = 0.05f;
            scene->addEllipse(path_way_point.x - radius, path_way_point.y - radius, 2.0f*radius, 2.0f*radius, wall_pen);
            // n.b., draws edges twice, but doesn't matter for debug purposes...
            for(size_t j=0;j<vertex->getNNeighbours();j++) {
                const GraphVertex *o_vertex = vertex->getNeighbour(distance_graph, j);
                Vector2D o_path_way_point = o_vertex->getPos();
                float x1 = path_way_point.x;
                float y1 = path_way_point.y;
                float x2 = o_path_way_point.x;
                float y2 = o_path_way_point.y;
                scene->addLine(x1, y1, x2, y2, wall_pen);
            }
        }
    }
#endif

    LOG("add graphics for items\n");
    for(set<Item *>::iterator iter = c_location->itemsBegin(); iter != c_location->itemsEnd(); ++iter) {
        Item *item = *iter;
        this->locationAddItem(c_location, item, false);
    }
    LOG("add graphics for scenery\n");
    for(set<Scenery *>::iterator iter = c_location->scenerysBegin(); iter != c_location->scenerysEnd(); ++iter) {
        Scenery *scenery = *iter;
        this->locationAddScenery(c_location, scenery);
    }

    LOG("add graphics for characters\n");
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        this->locationAddCharacter(c_location, character);
    }
    view->centreOnPlayer(); // must be done after player character graphic object is created

    /*const int size_c = 256;
    const int alpha = 255;
    QRadialGradient radialGrad(size_c/2, size_c/2, size_c/2);
    radialGrad.setColorAt(0.0, QColor(255, 255, 255, alpha));
    radialGrad.setColorAt(1.0, Qt::transparent);
    //radialGrad.setColorAt(0.0, QColor(0, 0, 0, 0));
    //radialGrad.setColorAt(1.0, QColor(0, 0, 0, alpha));
    QPixmap pixmap(size_c, size_c);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setPen(Qt::NoPen);
    painter.setBrush(radialGrad);
    painter.drawEllipse(0, 0, size_c, size_c);
    painter.end();

    AnimatedObject *object = static_cast<AnimatedObject *>(player->getListenerData());
    //QGraphicsPixmapItem *light_source = scene->addPixmap(pixmap);
    QGraphicsPixmapItem *light_source = new QGraphicsPixmapItem(pixmap);
    light_source->setParentItem(object);
    const float scale_c = 8.0f;
    light_source->setPos(object->getWidth()/2-size_c*scale_c/2, object->getHeight()/2-size_c*scale_c/2);
    light_source->setScale(scale_c);
    //light_source->setOpacity(0.5f);
    light_source->setZValue(10000.0f);*/

    /*{
        TextEffect *text_effect = new TextEffect("Welcome to Erebus", 1000);
        text_effect->setPos( player->getPos().x, player->getPos().y );
        scene->addItem(text_effect);
    }*/
    //this->addTextEffect("Welcome to Erebus", player->getPos(), 2000);

    /*SmokeParticleSystem *ps = new SmokeParticleSystem(smoke_pixmap);
    ps->setBirthRate(1.0f);
    ps->setPos(player->getX(), player->getY());
    ps->setScale(0.5f / smoke_pixmap.width());
    ps->setZValue(1000.0f);
    scene->addItem(ps);*/

    LOG("init visibility\n");
    //c_location->initVisibility(player->getPos());
    c_location->updateVisibility(player->getPos());
    // n.b., we look through all the regions rather than just those updated, so that this works when loading a game (as then, other floor regions may have been previously set to be visible)
    for(size_t i=0;i<c_location->getNFloorRegions();i++) {
        FloorRegion *floor_region = c_location->getFloorRegion(i);
        if( floor_region->isVisible() ) {
            this->updateVisibilityForFloorRegion(floor_region);
        }
    }
    LOG("done\n");
}

void PlayingGamestate::loadQuest(string filename, bool is_savegame) {
    LOG("PlayingGamestate::loadQuest(%s)\n", filename.c_str());
    // filename should be full path

    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->setEnabled(false);
    game_g->getScreen()->setPaused(true);

    gui_overlay->setProgress(0);
    qApp->processEvents();

    if( this->player != NULL && this->player->getLocation() != NULL ) {
        qDebug("remove player from location\n");
        this->player->getLocation()->removeCharacter(this->player);
        this->player->setListener(NULL, NULL);
    }
    if( this->quest != NULL ) {
        qDebug("delete previous quest...\n");
        delete this->quest;
    }
    // delete any items from previous quests
    view->clear();
    //this->closeAllSubWindows(); // just to be safe - e.g., when moving onto next quest from campaign window

    qDebug("create new quest\n");
    this->quest = new Quest();
    //this->quest->setCompleted(true); // test

    /*Location *location = new Location();
    this->quest->addLocation(location);*/
    Location *location = NULL;

    gui_overlay->setProgress(10);
    qApp->processEvents();

    {
        int progress_lo = 10, progress_hi = 50;
        bool done_player_start = false;
        enum QuestXMLType {
            QUEST_XML_TYPE_NONE = 0,
            //QUEST_XML_TYPE_BOUNDARY = 1,
            QUEST_XML_TYPE_SCENERY = 2,
            QUEST_XML_TYPE_NPC = 3,
            QUEST_XML_TYPE_FLOORREGION = 4
        };
        QuestXMLType questXMLType = QUEST_XML_TYPE_NONE;
        Polygon2D boundary;
        Scenery *scenery = NULL;
        Character *npc = NULL;
        FloorRegion *floor_region = NULL;
        QFile file(filename.c_str());
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open quest xml file");
        }
        qint64 size = file.size();
        int progress_count = 0;
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                progress_count++;
                if( progress_count % 20 == 0 ) {
                    float progress = ((float)file.pos()) / ((float)size);
                    gui_overlay->setProgress((1.0f - progress) * progress_lo + progress * progress_hi);
                    qApp->processEvents();
                }

                qDebug("read start element: %s (questXMLType=%d)", reader.name().toString().toStdString().c_str(), questXMLType);
                if( reader.name() == "info" ) {
                    if( is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: info element not expected in save games");
                    }
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: info element wasn't expected here");
                    }
                    QString info = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    qDebug("quest info: %s\n", info.toStdString().c_str());
                    quest->setInfo(info.toStdString());
                }
                else if( reader.name() == "completed_text" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: completed_text element wasn't expected here");
                    }
                    QString completed_text = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    qDebug("quest completed_text: %s\n", completed_text.toStdString().c_str());
                    quest->setCompletedText(completed_text.toStdString());
                }
                else if( reader.name() == "journal" ) {
                    if( !is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: journal element only allowed in save games");
                    }
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: journal element wasn't expected here");
                    }
                    QString encoded = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    QByteArray journal = QByteArray::fromPercentEncoding(encoded.toLatin1());
                    this->journal_ss.clear();
                    this->journal_ss << journal.data();
                }
                else if( reader.name() == "game" ) {
                    if( !is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: game element only allowed in save games");
                    }
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: game element wasn't expected here");
                    }
                    QStringRef difficulty_s = reader.attributes().value("difficulty");
                    qDebug("read difficulty: %s\n", difficulty_s.toString().toStdString().c_str());
                    for(int i=0;i<N_DIFFICULTIES;i++) {
                        Difficulty test_difficulty = (Difficulty)i;
                        if( difficulty_s.toString().toStdString() == Game::getDifficultyString(test_difficulty) ) {
                            this->difficulty = test_difficulty;
                            qDebug("    set difficulty to: %d\n", difficulty);
                            break;
                        }
                    }
                    // if not defined, we keep to the default
                }
                else if( reader.name() == "location" ) {
                    if( location != NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: location element wasn't expected here");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    qDebug("read location: %s\n", name_s.toString().toStdString().c_str());
                    if( quest->findLocation(name_s.toString().toStdString()) != NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: duplicate location name");
                    }
                    QStringRef type_s = reader.attributes().value("type");
                    QStringRef lighting_min_s = reader.attributes().value("lighting_min");
                    location = new Location(name_s.toString().toStdString());
                    if( type_s.length() > 0 ) {
                        if( type_s.toString() == "indoors" ) {
                            location->setType(Location::TYPE_INDOORS);
                        }
                        else if( type_s.toString() == "outdoors" ) {
                            location->setType(Location::TYPE_OUTDOORS);
                        }
                        else {
                            LOG("error at line %d\n", reader.lineNumber());
                            LOG("unknown type: %s\n", type_s.toString().toStdString().c_str());
                            throw string("unexpected quest xml: location has unknown type");
                        }
                    }
                    if( lighting_min_s.length() > 0 ) {
                        int lighting_min = parseInt(lighting_min_s.toString());
                        if( lighting_min < 0 || lighting_min > 255 ) {
                            LOG("unknown type: %s\n", type_s.toString().toStdString().c_str());
                            LOG("invalid lighting_min: %f\n", lighting_min);
                            throw string("unexpected quest xml: location has invalid lighting_min");
                        }
                        location->setLightingMin(static_cast<unsigned char>(lighting_min));
                    }
                    quest->addLocation(location);
                }
                else if( reader.name() == "floor" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: floor element wasn't expected here");
                    }
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    if( image_name_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: floor element has no image_name attribute");
                    }
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: floor element outside of location");
                    }
                    location->setFloorImageName(image_name_s.toString().toStdString());
                }
                else if( reader.name() == "wall" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: wall element wasn't expected here");
                    }
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: wall element outside of location");
                    }
                    if( image_name_s.length() > 0 ) {
                        location->setWallImageName(image_name_s.toString().toStdString());
                    }
                }
                else if( reader.name() == "background" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: background element wasn't expected here");
                    }
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    if( image_name_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: background element has no image_name attribute");
                    }
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: background element outside of location");
                    }
                    location->setBackgroundImageName(image_name_s.toString().toStdString());
                }
                else if( reader.name() == "floorregion" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: floorregion element wasn't expected here");
                    }
                    QStringRef shape_s = reader.attributes().value("shape");
                    if( shape_s == "rect" ) {
                        QStringRef rect_x_s = reader.attributes().value("rect_x");
                        float rect_x = parseFloat(rect_x_s.toString());
                        QStringRef rect_y_s = reader.attributes().value("rect_y");
                        float rect_y = parseFloat(rect_y_s.toString());
                        QStringRef rect_w_s = reader.attributes().value("rect_w");
                        float rect_w = parseFloat(rect_w_s.toString());
                        QStringRef rect_h_s = reader.attributes().value("rect_h");
                        float rect_h = parseFloat(rect_h_s.toString());
                        //qDebug("found floor region: %f, %f, %f, %f\n", rect_x, rect_y, rect_w, rect_h);
                        questXMLType = QUEST_XML_TYPE_FLOORREGION;
                        floor_region = FloorRegion::createRectangle(rect_x, rect_y, rect_w, rect_h);
                        // will be added to location when doing the end floorregion element
                    }
                    else if( shape_s == "polygon" ) {
                        questXMLType = QUEST_XML_TYPE_FLOORREGION;
                        floor_region = new FloorRegion();
                    }
                    else {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("floorregion has unknown shape");
                    }
                    QStringRef visible_s = reader.attributes().value("visible");
                    if( visible_s.length() > 0 ) {
                        bool visible = parseBool(visible_s.toString());
                        floor_region->setVisible(visible);
                    }
                }
                else if( reader.name() == "floorregion_point" ) {
                    if( questXMLType != QUEST_XML_TYPE_FLOORREGION ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: floorregion_point element wasn't expected here");
                    }
                    QStringRef x_s = reader.attributes().value("x");
                    float x = parseFloat(x_s.toString());
                    QStringRef y_s = reader.attributes().value("y");
                    float y = parseFloat(y_s.toString());
                    floor_region->addPoint(Vector2D(x, y));
                }
                /*else if( reader.name() == "boundary" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        throw string("unexpected quest xml");
                    }
                    questXMLType = QUEST_XML_TYPE_BOUNDARY;
                    boundary = Polygon2D(); // reset
                }
                else if( reader.name() == "point" ) {
                    if( questXMLType != QUEST_XML_TYPE_BOUNDARY ) {
                        throw string("unexpected quest xml");
                    }
                    QStringRef point_x_s = reader.attributes().value("x");
                    float point_x = parseFloat(point_x_s.toString());
                    QStringRef point_y_s = reader.attributes().value("y");
                    float point_y = parseFloat(point_y_s.toString());
                    qDebug("found boundary point: %f, %f\n", point_x, point_y);
                    boundary.addPoint(Vector2D(point_x, point_y));
                }*/
                else if( reader.name() == "player_start" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: player_start element wasn't expected here");
                    }
                    if( done_player_start ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: duplicate player_start element");
                    }
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    if( player == NULL ) {
                        throw string("encountered player_start element, but player not yet defined");
                    }
                    qDebug("player starts at %f, %f", pos_x, pos_y);
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: player_start element outside of location");
                    }
                    this->c_location = location;
                    this->c_location->addCharacter(player, pos_x, pos_y);
                    done_player_start = true;
                }
                else if( reader.name() == "quest_objective" ) {
                    QStringRef type_s = reader.attributes().value("type");
                    QStringRef arg1_s = reader.attributes().value("arg1");
                    QStringRef gold_s = reader.attributes().value("gold");
                    int gold = parseInt(gold_s.toString());
                    QuestObjective *quest_objective = new QuestObjective(type_s.toString().toStdString(), arg1_s.toString().toStdString(), gold);
                    this->quest->setQuestObjective(quest_objective);
                }
                else if( reader.name() == "quest_info" ) {
                    QStringRef complete_s = reader.attributes().value("complete");
                    bool complete = parseBool(complete_s.toString(), true);
                    this->quest->setCompleted(complete);
                }
                else if( reader.name() == "npc" || reader.name() == "player" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: npc/player element wasn't expected here");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef template_s = reader.attributes().value("template");
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    QStringRef default_pos_x_s = reader.attributes().value("default_x");
                    QStringRef default_pos_y_s = reader.attributes().value("default_y");
                    if( template_s.length() > 0 ) {
                        if( reader.name() == "player" ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("didn't expect player element to load by template");
                        }
                        // load from template
                        /*CharacterTemplate *character_template = this->character_templates[name_s.toString().toStdString()];
                        if( character_template == NULL ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            LOG("can't find character template: %s\n", template_s.toString().toStdString().c_str());
                            throw string("can't find character template");
                        }
                        npc = new Character(name_s.toString().toStdString(), true, *character_template);*/
                        npc = this->createCharacter(template_s.toString().toStdString());
                        QStringRef is_hostile_s = reader.attributes().value("is_hostile");
                        bool is_hostile = is_hostile_s.length() == 0 ? true : parseBool(is_hostile_s.toString());
                        npc->setHostile(is_hostile);
                    }
                    else {
                        if( reader.name() == "player" ) {
                            qDebug("player: %s", name_s.toString().toStdString().c_str());
                            this->player = npc = new Character(name_s.toString().toStdString(), "", false);
                        }
                        else {
                            qDebug("npc: %s", name_s.toString().toStdString().c_str());
                            QStringRef animation_name_s = reader.attributes().value("animation_name");
                            if( animation_name_s.length() == 0 ) {
                                throw string("npc has no animation_name");
                            }
                            QStringRef static_image_s = reader.attributes().value("static_image");
                            bool static_image = parseBool(static_image_s.toString(), true);
                            QStringRef bounce_s = reader.attributes().value("bounce");
                            bool bounce = parseBool(bounce_s.toString(), true);
                            QStringRef is_hostile_s = reader.attributes().value("is_hostile");
                            bool is_hostile = is_hostile_s.length() == 0 ? true : parseBool(is_hostile_s.toString());

                            npc = new Character(name_s.toString().toStdString(), animation_name_s.toString().toStdString(), true);
                            npc->setStaticImage(static_image);
                            npc->setBounce(bounce);
                            npc->setHostile(is_hostile);
                        }
                        QStringRef is_dead_s = reader.attributes().value("is_dead");
                        npc->setDead( parseBool(is_dead_s.toString(), true) );
                        QStringRef health_s = reader.attributes().value("health");
                        QStringRef max_health_s = reader.attributes().value("max_health");
                        npc->initialiseHealth( parseInt( max_health_s.toString()) );
                        npc->setHealth( parseInt( health_s.toString()) );
                        QStringRef FP_s = reader.attributes().value("FP");
                        int FP = parseInt(FP_s.toString());
                        QStringRef BS_s = reader.attributes().value("BS");
                        int BS = parseInt(BS_s.toString());
                        QStringRef S_s = reader.attributes().value("S");
                        int S = parseInt(S_s.toString());
                        QStringRef A_s = reader.attributes().value("A");
                        int A = parseInt(A_s.toString());
                        QStringRef M_s = reader.attributes().value("M");
                        int M = parseInt(M_s.toString());
                        QStringRef D_s = reader.attributes().value("D");
                        int D = parseInt(D_s.toString());
                        QStringRef B_s = reader.attributes().value("B");
                        int B = parseInt(B_s.toString());
                        QStringRef Sp_s = reader.attributes().value("Sp");
                        float Sp = parseFloat(Sp_s.toString());
                        npc->setProfile(FP, BS, S, A, M, D, B, Sp);
                        QStringRef natural_damageX_s = reader.attributes().value("natural_damageX");
                        QStringRef natural_damageY_s = reader.attributes().value("natural_damageY");
                        QStringRef natural_damageZ_s = reader.attributes().value("natural_damageZ");
                        if( natural_damageX_s.length() > 0 || natural_damageY_s.length() > 0 || natural_damageZ_s.length() > 0 ) {
                            int natural_damageX = parseInt(natural_damageX_s.toString());
                            int natural_damageY = parseInt(natural_damageY_s.toString());
                            int natural_damageZ = parseInt(natural_damageZ_s.toString());
                            npc->setNaturalDamage(natural_damageX, natural_damageY, natural_damageZ);
                        }
                        QStringRef can_fly_s = reader.attributes().value("can_fly");
                        bool can_fly = parseBool(can_fly_s.toString(), true);
                        npc->setCanFly(can_fly);
                        QStringRef is_paralysed_s = reader.attributes().value("is_paralysed");
                        bool is_paralysed = parseBool(is_paralysed_s.toString(), true);
                        if( is_paralysed ) {
                            QStringRef paralysed_time_s = reader.attributes().value("paralysed_time");
                            int paralysed_time = parseInt(paralysed_time_s.toString());
                            npc->paralyse(paralysed_time);
                        }
                        QStringRef level_s = reader.attributes().value("level");
                        int level = parseInt(level_s.toString(), true);
                        npc->setLevel(level);
                        QStringRef xp_s = reader.attributes().value("xp");
                        int xp = parseInt(xp_s.toString(), true);
                        npc->setXP(xp);
                        QStringRef xp_worth_s = reader.attributes().value("xp_worth");
                        int xp_worth = parseInt(xp_worth_s.toString());
                        npc->setXPWorth(xp_worth);
                        QStringRef causes_terror_s = reader.attributes().value("causes_terror");
                        bool causes_terror = parseBool(causes_terror_s.toString(), true);
                        if( causes_terror ) {
                            QStringRef terror_effect_s = reader.attributes().value("terror_effect");
                            int terror_effect = parseInt(terror_effect_s.toString());
                            npc->setCausesTerror(terror_effect);
                        }
                        QStringRef done_terror_s = reader.attributes().value("done_terror");
                        bool done_terror = parseBool(done_terror_s.toString(), true);
                        npc->setDoneTerror(done_terror);
                        QStringRef requires_magical_s = reader.attributes().value("requires_magical");
                        bool requires_magical = parseBool(requires_magical_s.toString(), true);
                        npc->setRequiresMagical(requires_magical);
                        QStringRef gold_s = reader.attributes().value("gold");
                        npc->addGold( parseInt( gold_s.toString(), true) );
                    }

                    QStringRef can_talk_s = reader.attributes().value("can_talk");
                    if( can_talk_s.length() > 0 ) {
                        bool can_talk = parseBool(can_talk_s.toString());
                        npc->setCanTalk(can_talk);
                    }
                    QStringRef has_talked_s = reader.attributes().value("has_talked");
                    if( has_talked_s.length() > 0 ) {
                        bool has_talked = parseBool(has_talked_s.toString());
                        npc->setHasTalked(has_talked);
                    }
                    QStringRef interaction_completed_s = reader.attributes().value("interaction_completed");
                    if( interaction_completed_s.length() > 0 ) {
                        bool interaction_completed = parseBool(interaction_completed_s.toString());
                        npc->setInteractionCompleted(interaction_completed);
                    }
                    QStringRef interaction_type_s = reader.attributes().value("interaction_type");
                    npc->setInteractionType(interaction_type_s.toString().toStdString());
                    QStringRef interaction_data_s = reader.attributes().value("interaction_data");
                    npc->setInteractionData(interaction_data_s.toString().toStdString());
                    QStringRef interaction_xp_s = reader.attributes().value("interaction_xp");
                    int interaction_xp = parseInt(interaction_xp_s.toString(), true);
                    npc->setInteractionXP(interaction_xp);
                    QStringRef shop_s = reader.attributes().value("shop");
                    if( shop_s.length() > 0 ) {
                        npc->setShop(shop_s.toString().toStdString());
                    }

                    // if an NPC doesn't have a default position defined in the file, we set it to the position
                    if( default_pos_x_s.length() > 0 && default_pos_y_s.length() > 0 ) {
                        float default_pos_x = parseFloat(pos_x_s.toString());
                        float default_pos_y = parseFloat(pos_y_s.toString());
                        npc->setDefaultPosition(default_pos_x, default_pos_y);
                    }
                    else {
                        npc->setDefaultPosition(pos_x, pos_y);
                    }
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: npc/player element outside of location");
                    }
                    location->addCharacter(npc, pos_x, pos_y);
                    questXMLType = QUEST_XML_TYPE_NPC;
                }
                else if( reader.name() == "opening_initial") {
                    if( questXMLType != QUEST_XML_TYPE_NPC ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: opening_initial element wasn't expected here");
                    }
                    QString text = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    npc->setTalkOpeningInitial(text.toStdString());
                }
                else if( reader.name() == "opening_later") {
                    if( questXMLType != QUEST_XML_TYPE_NPC ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: opening_later element wasn't expected here");
                    }
                    QString text = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    npc->setTalkOpeningLater(text.toStdString());
                }
                else if( reader.name() == "opening_interaction_complete") {
                    if( questXMLType != QUEST_XML_TYPE_NPC ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: opening_interaction_complete element wasn't expected here");
                    }
                    QString text = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    npc->setTalkOpeningInteractionComplete(text.toStdString());
                }
                else if( reader.name() == "talk") {
                    if( questXMLType != QUEST_XML_TYPE_NPC ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: talk element wasn't expected here");
                    }
                    QStringRef question_s = reader.attributes().value("question");
                    QString question = question_s.toString();
                    QStringRef action_s = reader.attributes().value("action");
                    QString action = action_s.toString();
                    QStringRef while_not_done_s = reader.attributes().value("while_not_done");
                    bool while_not_done = parseBool(while_not_done_s.toString(), true);
                    QStringRef objective_s = reader.attributes().value("objective");
                    bool objective = parseBool(objective_s.toString(), true);
                    QString answer = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    npc->addTalkItem(question.toStdString(), answer.toStdString(), action.toStdString(), while_not_done, objective);
                }
                else if( reader.name() == "spell") {
                    if( questXMLType != QUEST_XML_TYPE_NPC ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: spell element wasn't expected here");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef count_s = reader.attributes().value("count");
                    int count = parseInt(count_s.toString());
                    npc->addSpell(name_s.toString().toStdString(), count);
                }
                else if( reader.name() == "item" || reader.name() == "weapon" || reader.name() == "shield" || reader.name() == "armour" || reader.name() == "ammo" || reader.name() == "currency" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE && questXMLType != QUEST_XML_TYPE_SCENERY && questXMLType != QUEST_XML_TYPE_NPC ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: item element wasn't expected here");
                    }
                    QStringRef template_s = reader.attributes().value("template");
                    Item *item = NULL;
                    // need to read everything we want from the reader, before calling parseXMLItem.
                    bool current_weapon = false, current_shield = false, current_armour = false;
                    float pos_x = 0.0f, pos_y = 0.0f;
                    if( questXMLType == QUEST_XML_TYPE_SCENERY ) {
                    }
                    else if( questXMLType == QUEST_XML_TYPE_NPC ) {
                        QStringRef current_weapon_s = reader.attributes().value("current_weapon");
                        current_weapon = parseBool(current_weapon_s.toString(), true);
                        QStringRef current_shield_s = reader.attributes().value("current_shield");
                        current_shield = parseBool(current_shield_s.toString(), true);
                        QStringRef current_armour_s = reader.attributes().value("current_armour");
                        current_armour = parseBool(current_armour_s.toString(), true);
                    }
                    else {
                        QStringRef pos_x_s = reader.attributes().value("x");
                        pos_x = parseFloat(pos_x_s.toString());
                        QStringRef pos_y_s = reader.attributes().value("y");
                        pos_y = parseFloat(pos_y_s.toString());
                    }

                    if( template_s.length() > 0 ) {
                        // load from template
                        qDebug("load item from template");
                        if( reader.name() != "item" ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("only allowed item element type when loading as template");
                        }
                        item = this->cloneStandardItem(template_s.toString().toStdString());
                        if( item == NULL ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            LOG("can't find item: %s\n", template_s.toString().toStdString().c_str());
                            throw string("can't find item");
                        }
                    }
                    else {
                        item = parseXMLItem(reader); // n.b., reader will advance to end element!
                        if( item == NULL ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unknown item element");
                        }
                    }

                    if( questXMLType == QUEST_XML_TYPE_SCENERY ) {
                        scenery->addItem(item);
                    }
                    else if( questXMLType == QUEST_XML_TYPE_NPC ) {
                        npc->addItem(item, npc != this->player);
                        if( current_weapon ) {
                            if( item->getType() != ITEMTYPE_WEAPON ) {
                                LOG("error at line %d\n", reader.lineNumber());
                                throw string("current_weapon is not a weapon");
                            }
                            Weapon *weapon = static_cast<Weapon *>(item);
                            npc->armWeapon(weapon);
                        }
                        else if( current_shield ) {
                            if( item->getType() != ITEMTYPE_SHIELD ) {
                                LOG("error at line %d\n", reader.lineNumber());
                                throw string("current_shield is not a shield");
                            }
                            Shield *shield = static_cast<Shield *>(item);
                            npc->armShield(shield);
                        }
                        else if( current_armour ) {
                            if( item->getType() != ITEMTYPE_ARMOUR ) {
                                LOG("error at line %d\n", reader.lineNumber());
                                throw string("current_armour is not a armour");
                            }
                            Armour *armour = static_cast<Armour *>(item);
                            npc->wearArmour(armour);
                        }
                    }
                    else {
                        if( location == NULL ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected quest xml: item element outside of location");
                        }
                        location->addItem(item, pos_x, pos_y);
                    }
                }
                else if( reader.name() == "gold" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE && questXMLType != QUEST_XML_TYPE_SCENERY && questXMLType != QUEST_XML_TYPE_NPC ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: gold element wasn't expected here");
                    }
                    QStringRef amount_s = reader.attributes().value("amount");
                    int amount = parseInt(amount_s.toString());
                    Item *item = this->cloneGoldItem(amount);
                    if( item == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        LOG("can't find gold item\n");
                        throw string("can't find gold item");
                    }
                    if( questXMLType == QUEST_XML_TYPE_SCENERY ) {
                        scenery->addItem(item);
                    }
                    else if( questXMLType == QUEST_XML_TYPE_NPC ) {
                        npc->addItem(item);
                    }
                    else {
                        QStringRef pos_x_s = reader.attributes().value("x");
                        float pos_x = parseFloat(pos_x_s.toString());
                        QStringRef pos_y_s = reader.attributes().value("y");
                        float pos_y = parseFloat(pos_y_s.toString());
                        if( location == NULL ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected quest xml: gold element outside of location");
                        }
                        location->addItem(item, pos_x, pos_y);
                    }
                }
                else if( reader.name() == "scenery" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: scenery element wasn't expected here");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    QStringRef opacity_s = reader.attributes().value("opacity");
                    QStringRef has_smoke_s = reader.attributes().value("has_smoke");
                    QStringRef draw_type_s = reader.attributes().value("draw_type");
                    QStringRef action_last_time_s = reader.attributes().value("action_last_time");
                    QStringRef action_delay_s = reader.attributes().value("action_delay");
                    QStringRef action_type_s = reader.attributes().value("action_type");
                    QStringRef action_value_s = reader.attributes().value("action_value");
                    QStringRef interact_type_s = reader.attributes().value("interact_type");
                    QStringRef interact_state_s = reader.attributes().value("interact_state");
                    QStringRef blocking_s = reader.attributes().value("blocking");
                    bool blocking = parseBool(blocking_s.toString(), true);
                    QStringRef block_visibility_s = reader.attributes().value("block_visibility");
                    bool block_visibility = parseBool(block_visibility_s.toString(), true);
                    QStringRef is_opened_s = reader.attributes().value("is_opened");
                    bool is_opened = parseBool(is_opened_s.toString(), true);
                    QStringRef door_s = reader.attributes().value("door");
                    bool door = parseBool(door_s.toString(), true);
                    QStringRef exit_s = reader.attributes().value("exit");
                    bool exit = parseBool(exit_s.toString(), true);
                    QStringRef exit_location_s = reader.attributes().value("exit_location");
                    QStringRef locked_s = reader.attributes().value("locked");
                    bool locked = parseBool(locked_s.toString(), true);
                    QStringRef locked_silent_s = reader.attributes().value("locked_silent");
                    bool locked_silent = parseBool(locked_silent_s.toString(), true);
                    QStringRef locked_text_s = reader.attributes().value("locked_text");
                    QStringRef locked_used_up_s = reader.attributes().value("locked_used_up");
                    bool locked_used_up = parseBool(locked_used_up_s.toString(), true);
                    QStringRef unlock_item_name_s = reader.attributes().value("unlocked_by_template");
                    QStringRef unlock_text_s = reader.attributes().value("unlock_text");
                    QStringRef unlock_xp_s = reader.attributes().value("unlock_xp");
                    QStringRef confirm_text_s = reader.attributes().value("confirm_text");
                    float size_w = 0.0f, size_h = 0.0f;
                    QStringRef size_s = reader.attributes().value("size");
                    bool is_animation = false;
                    map<string, QPixmap>::iterator image_iter = this->scenery_images.find(image_name_s.toString().toStdString());
                    map<string, AnimationLayer *>::const_iterator animation_iter = this->scenery_animation_layers.find(image_name_s.toString().toStdString());
                    if( image_iter == this->scenery_images.end() ) {
                        if( animation_iter == this->scenery_animation_layers.end() ) {
                            LOG("failed to find image for scenery: %s\n", name_s.toString().toStdString().c_str());
                            LOG("    image name: %s\n", image_name_s.toString().toStdString().c_str());
                            throw string("Failed to find scenery's image");
                        }
                        else {
                            is_animation = true;
                        }
                    }
                    else {
                        is_animation = false;
                    }

                    if( size_s.length() > 0 ) {
                        float size = parseFloat(size_s.toString());
                        QPixmap image;
                        if( is_animation ) {
                            const AnimationLayer *animation_layer = animation_iter->second;
                            image = animation_layer->getAnimationSet("")->getFrame(0, 0);
                        }
                        else {
                            image = image_iter->second;
                        }
                        //QPixmap image = this->scenery_images[image_name_s.toString().toStdString()];
                        int image_w = image.width();
                        int image_h = image.height();
                        if( image_w > image_h ) {
                            size_w = size;
                            size_h = (size*image_h)/(float)image_w;
                        }
                        else {
                            size_h = size;
                            size_w = (size*image_w)/(float)image_h;
                        }
                        qDebug("size: %f size_w: %f size_h: %f", size, size_w, size_h);
                    }
                    else {
                        QStringRef size_w_s = reader.attributes().value("w");
                        QStringRef size_h_s = reader.attributes().value("h");
                        size_w = parseFloat(size_w_s.toString());
                        size_h = parseFloat(size_h_s.toString());
                    }

                    scenery = new Scenery(name_s.toString().toStdString(), image_name_s.toString().toStdString(), is_animation, size_w, size_h);
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: scenery element outside of location");
                    }
                    location->addScenery(scenery, pos_x, pos_y);
                    if( door && exit ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("scenery can't be both a door and an exit");
                    }
                    else if( exit && exit_location_s.length() > 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("scenery can't be both an exit and an exit_location");
                    }
                    else if( exit_location_s.length() > 0 && door ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("scenery can't be both an exit_location and a door");
                    }

                    if( is_animation ) {
                        map<string, AnimationLayer *>::const_iterator animation_iter = this->scenery_animation_layers.find(scenery->getImageName());
                        if( animation_iter != this->scenery_animation_layers.end() ) {
                            const AnimationLayer *animation_layer = animation_iter->second;
                            if( animation_layer->getAnimationSet("opened") != NULL ) {
                                scenery->setCanBeOpened(true);
                            }
                        }
                    }

                    scenery->setBlocking(blocking, block_visibility);
                    if( is_opened && !scenery->canBeOpened() ) {
                        qDebug("trying to set is_opened on scenery that can't be opened: %s at %f, %f", scenery->getName().c_str(), scenery->getX(), scenery->getY());
                    }
                    scenery->setOpened(is_opened);
                    if( opacity_s.length() > 0 ) {
                        float opacity = parseFloat(opacity_s.toString());
                        scenery->setOpacity(opacity);
                    }
                    if( has_smoke_s.length() > 0 ) {
                        bool has_smoke = parseBool(has_smoke_s.toString());
                        scenery->setHasSmoke(has_smoke);
                    }
                    if( draw_type_s.length() > 0 ) {
                        if( draw_type_s == "floating" ) {
                            scenery->setDrawType(Scenery::DRAWTYPE_FLOATING);
                        }
                        else if( draw_type_s == "background" ) {
                            scenery->setDrawType(Scenery::DRAWTYPE_BACKGROUND);
                        }
                        else {
                            LOG("unrecognised draw_type: %s\n", draw_type_s.toString().toStdString().c_str());
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unrecognised draw_type for scenery");
                        }
                    }
                    if( action_last_time_s.length() > 0 ) {
                        int action_last_time = parseInt(action_last_time_s.toString());
                        scenery->setActionLastTime(action_last_time);
                    }
                    if( action_delay_s.length() > 0 ) {
                        int action_delay = parseInt(action_delay_s.toString());
                        scenery->setActionDelay(action_delay);
                    }
                    if( action_type_s.length() > 0 ) {
                        scenery->setActionType(action_type_s.toString().toStdString());
                    }
                    if( action_value_s.length() > 0 ) {
                        int action_value = parseInt(action_value_s.toString());
                        scenery->setActionValue(action_value);
                    }
                    if( interact_type_s.length() > 0 ) {
                        scenery->setInteractType(interact_type_s.toString().toStdString());
                    }
                    if( interact_state_s.length() > 0 ) {
                        int interact_state = parseInt(interact_state_s.toString());
                        scenery->setInteractState(interact_state);
                    }
                    scenery->setDoor(door);
                    scenery->setExit(exit);
                    if( exit_location_s.length() > 0 ) {
                        QStringRef exit_location_x_s = reader.attributes().value("exit_location_x");
                        float exit_location_x = parseFloat(exit_location_x_s.toString());
                        QStringRef exit_location_y_s = reader.attributes().value("exit_location_y");
                        float exit_location_y = parseFloat(exit_location_y_s.toString());
                        scenery->setExitLocation(exit_location_s.toString().toStdString(), Vector2D(exit_location_x, exit_location_y));
                    }
                    scenery->setLocked(locked);
                    scenery->setLockedSilent(locked_silent);
                    if( locked_text_s.length() > 0 ) {
                        scenery->setLockedText(locked_text_s.toString().toStdString());
                    }
                    scenery->setLockedUsedUp(locked_used_up);
                    if( unlock_item_name_s.length() > 0 ) {
                        scenery->setUnlockItemName(unlock_item_name_s.toString().toStdString());
                    }
                    if( unlock_text_s.length() > 0 ) {
                        scenery->setUnlockText(unlock_text_s.toString().toStdString());
                    }

                    if( unlock_xp_s.length() > 0 ) {
                        int unlock_xp = parseInt(unlock_xp_s.toString());
                        scenery->setUnlockXP(unlock_xp);
                    }
                    if( confirm_text_s.length() > 0 ) {
                        scenery->setConfirmText(confirm_text_s.toString().toStdString());
                    }
                    questXMLType = QUEST_XML_TYPE_SCENERY;
                }
                else if( reader.name() == "trap" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE && questXMLType != QUEST_XML_TYPE_SCENERY ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: trap element wasn't expected here");
                    }
                    QStringRef type_s = reader.attributes().value("type");
                    QStringRef rating_s = reader.attributes().value("rating");
                    int rating = parseInt(rating_s.toString(), true);
                    QStringRef difficulty_s = reader.attributes().value("difficulty");
                    int difficulty = parseInt(difficulty_s.toString(), true);
                    if( questXMLType == QUEST_XML_TYPE_SCENERY ) {
                        Trap *trap = new Trap(type_s.toString().toStdString());
                        trap->setRating(rating);
                        trap->setDifficulty(difficulty);
                        scenery->setTrap(trap);
                    }
                    else {
                        QStringRef pos_x_s = reader.attributes().value("x");
                        float pos_x = parseFloat(pos_x_s.toString());
                        QStringRef pos_y_s = reader.attributes().value("y");
                        float pos_y = parseFloat(pos_y_s.toString());
                        QStringRef size_w_s = reader.attributes().value("w");
                        float size_w = parseFloat(size_w_s.toString());
                        QStringRef size_h_s = reader.attributes().value("h");
                        float size_h = parseFloat(size_h_s.toString());
                        Trap *trap = new Trap(type_s.toString().toStdString(), size_w, size_h);
                        trap->setRating(rating);
                        trap->setDifficulty(difficulty);
                        if( location == NULL ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected quest xml: trap element outside of location");
                        }
                        location->addTrap(trap, pos_x, pos_y);
                    }
                }
            }
            else if( reader.isEndElement() ) {
                if( reader.name() == "location" ) {
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: location end element wasn't expected here");
                    }
                    location = NULL;
                }
                else if( reader.name() == "npc" || reader.name() == "player" ) {
                    if( questXMLType != QUEST_XML_TYPE_NPC ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: npc/player end element wasn't expected here");
                    }
                    npc = NULL;
                    questXMLType = QUEST_XML_TYPE_NONE;
                }
                else if( reader.name() == "scenery" ) {
                    if( questXMLType != QUEST_XML_TYPE_SCENERY ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: scenery end element wasn't expected here");
                    }
                    scenery = NULL;
                    questXMLType = QUEST_XML_TYPE_NONE;
                }
                else if( reader.name() == "floorregion" ) {
                    if( questXMLType != QUEST_XML_TYPE_FLOORREGION ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: floorregion end element wasn't expected here");
                    }
                    if( floor_region->getNPoints() < 3 ) {
                        LOG("floorregion only has %d points\n", floor_region->getNPoints());
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("floorregion has insufficient points");
                    }
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: floorregion end element outside of location");
                    }
                    location->addFloorRegion(floor_region);
                    floor_region = NULL;
                    questXMLType = QUEST_XML_TYPE_NONE;
                }
            }
            else {
                if( questXMLType == QUEST_XML_TYPE_SCENERY ) {
                    QStringRef text = reader.text();
                    if( stringAnyNonWhitespace(text.toString().toStdString()) ) {
                        //qDebug("### : %s", text.toString().toStdString().c_str());
                        scenery->setDescription(text.toString().toStdString());
                    }
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error at line %d\n", reader.lineNumber());
            LOG("error reading quest xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading quest xml file");
        }
        else if( !done_player_start ) {
            LOG("quest xml didn't define player_start\n");
            throw string("quest xml didn't define player_start");
        }
    }

    gui_overlay->setProgress(50);
    qApp->processEvents();

    int progress_lo = 50, progress_hi = 100;
    int progress_count = 0;
    for(vector<Location *>::iterator iter = quest->locationsBegin(); iter != quest->locationsEnd(); ++iter) {
        Location *loc = *iter;
        qDebug("process location: %s", loc->getName().c_str());
        if( loc->getBackgroundImageName().length() == 0 ) {
            throw string("Location doesn't define background image name");
        }
        else if( loc->getFloorImageName().length() == 0 ) {
            throw string("Location doesn't define floor image name");
        }

        loc->createBoundariesForRegions();
        loc->createBoundariesForScenery();
        loc->calculateDistanceGraph();

        progress_count++;
        if( progress_count % 4 == 0 ) {
            float progress = ((float)progress_count) / ((float)quest->getNLocations());
            gui_overlay->setProgress((1.0f - progress) * progress_lo + progress * progress_hi);
            qApp->processEvents();
        }
    }

    this->c_location->setListener(this, NULL); // must do after creating the location and its contents, so it doesn't try to add items to the scene, etc
    this->setupView();

    gui_overlay->unsetProgress();
    qApp->processEvents();

    window->setEnabled(true);
    game_g->getScreen()->setPaused(false);
    game_g->getScreen()->restartElapsedTimer();

    //qApp->processEvents();

    if( !is_savegame && quest->getInfo().length() > 0 ) {
        string quest_info = quest->getInfo();
        quest_info = convertToHTML(quest_info);
        stringstream str;
        str << "<html><body>";
        str << "<h1>Quest</h1>";
        str << "<p>" << quest_info << "</p>";
        str << "</body></html>";
        this->showInfoWindow(str.str());

        this->journal_ss << "<p><b>Quest Details</b></p>";
        this->journal_ss << "<p>" << quest_info << "</p>";
    }

    qDebug("View is transformed? %d", view->isTransformed());
    LOG("done\n");
}

void PlayingGamestate::locationAddItem(const Location *location, Item *item, bool visible) {
    if( this->c_location == location ) {
        QGraphicsPixmapItem *object = new QGraphicsPixmapItem();
        item->setUserGfxData(object);
        object->setPixmap( this->getItemImage( item->getImageName() ) );
        scene->addItem(object);
        /*{
            // DEBUG
            QPen pen(Qt::red);
            scene->addEllipse(item->getX() - 0.5f*item_width, item->getY() - 0.5f*item_width, item_width, item_width, pen);
            //scene->addEllipse(item->getX() - 0.05f, item->getY() - 0.05f, 0.1f, 0.1f, pen);
        }*/
        float icon_width = item->getIconWidth();
        object->setPos(item->getX(), item->getY());
        float item_scale = icon_width / object->pixmap().width();
        object->setTransformOriginPoint(-0.5f*object->pixmap().width()*item_scale, -0.5f*object->pixmap().height()*item_scale);
        object->setScale(item_scale);
        object->setZValue(2.0f*E_TOL_LINEAR); // so items appear above DRAWTYPE_BACKGROUND Scenery
        object->setVisible(visible);
    }
}

void PlayingGamestate::locationRemoveItem(const Location *location, Item *item) {
    if( this->c_location == location ) {
        QGraphicsPixmapItem *object = static_cast<QGraphicsPixmapItem *>(item->getUserGfxData());
        item->setUserGfxData(NULL);
        scene->removeItem(object);
        delete object;
    }
}

void PlayingGamestate::locationAddScenery(const Location *location, Scenery *scenery) {
    //qDebug("PlayingGamestate::locationAddScenery(%d, %d)", location, scenery);
    if( this->c_location == location ) {
        QGraphicsItem *object = NULL;
        if( scenery->isAnimation() ) {
            object = new AnimatedObject();
        }
        else {
            object = new QGraphicsPixmapItem();
        }
        //qDebug("set gfx data %d", object);
        scenery->setUserGfxData(object);
        this->locationUpdateScenery(scenery);
        object->setOpacity(scenery->getOpacity());
        object->setVisible(false); // default to false, visibility is checked afterwards
        scene->addItem(object);
        object->setPos(scenery->getX(), scenery->getY());
        float z_value = object->pos().y();
        if( scenery->getDrawType() == Scenery::DRAWTYPE_FLOATING ) {
            z_value += 1000.0f;
        }
        else if( scenery->getDrawType() == Scenery::DRAWTYPE_BACKGROUND ) {
            z_value = E_TOL_LINEAR;
        }
        object->setZValue(z_value);
        /*
        //float scenery_scale = scenery_width / object->pixmap().width();
        // n.b., aspect-ratio of scenery should match that of the corresponding image for this scenery!
        float scenery_scale = scenery->getWidth() / object->pixmap().width();
        //object->setTransformOriginPoint(-0.5f*object->pixmap().width()*scenery_scale, -0.5f*object->pixmap().height()*scenery_scale);
        //object->setScale(scenery_scale);
        //QTransform transform = QTransform::fromScale(scenery_scale, scenery_scale);
        float centre_x = 0.5f*object->pixmap().width();
        float centre_y = 0.5f*object->pixmap().height();
        QTransform transform;
        transform = transform.scale(scenery_scale, scenery_scale);
        transform = transform.translate(-centre_x, -centre_y);
        object->setTransform(transform);
        */
        /*float scenery_scale_w = scenery->getWidth() / object->pixmap().width();
        float scenery_scale_h = scenery->getHeight() / object->pixmap().height();
        float centre_x = 0.5f*object->pixmap().width();
        float centre_y = 0.5f*object->pixmap().height();*/
        float scenery_scale_w = scenery->getWidth() / object->boundingRect().width();
        float scenery_scale_h = scenery->getHeight() / object->boundingRect().height();
        float centre_x = 0.5f*object->boundingRect().width();
        float centre_y = 0.5f*object->boundingRect().height();
        QTransform transform;
        transform = transform.scale(scenery_scale_w, scenery_scale_h);
        transform = transform.translate(-centre_x, -centre_y);
        object->setTransform(transform);
        if( scenery->hasSmoke() ) {
            SmokeParticleSystem *ps = new SmokeParticleSystem(smoke_pixmap);
            ps->setBirthRate(1.0f);
            ps->setPos(centre_x, 2.0f*smoke_pixmap.height());
            ps->setScale(2.0f);
            ps->setZValue(object->pos().y() + 2000.0f);
            ps->setParentItem(object);
        }
    }
}

void PlayingGamestate::locationRemoveScenery(const Location *location, Scenery *scenery) {
    if( this->c_location == location ) {
        QGraphicsPixmapItem *object = static_cast<QGraphicsPixmapItem *>(scenery->getUserGfxData());
        scenery->setUserGfxData(NULL);
        scene->removeItem(object);
        delete object;

        if( scenery->blocksVisibility() ) {
            this->updateVisibility(player->getPos());
        }
    }
}

void PlayingGamestate::locationUpdateScenery(Scenery *scenery) {
    //qDebug("### update for: %s", scenery->getName().c_str());
    //AnimatedObject *object = new AnimatedObject();
    //this->characterUpdateGraphics(character, object);
    if( scenery->isAnimation() ) {
        //qDebug("animation");
        AnimatedObject *object = static_cast<AnimatedObject *>(scenery->getUserGfxData());
        object->clearAnimationLayers();
        //qDebug("update scenery: %s", scenery->getName().c_str());
        object->addAnimationLayer( this->scenery_animation_layers[scenery->getImageName()] );
        if( scenery->isOpened() ) {
            object->setAnimationSet("opened", true);
        }
        else {
            object->setAnimationSet("", true);
        }
    }
    else {
        QGraphicsPixmapItem *object = static_cast<QGraphicsPixmapItem *>(scenery->getUserGfxData());
        if( object != NULL ) {
            map<string, QPixmap>::iterator image_iter = this->scenery_images.find(scenery->getImageName());
            if( image_iter == this->scenery_images.end() ) {
                LOG("failed to find image for scenery: %s\n", scenery->getName().c_str());
                LOG("    image name: %s\n", scenery->getImageName().c_str());
                throw string("Failed to find scenery's image");
            }
            object->setPixmap( image_iter->second );
            object->update();
        }
    }
}

void PlayingGamestate::locationAddCharacter(const Location *location, Character *character) {
    AnimatedObject *object = new AnimatedObject();
    object->setBounce( character->isBounce() );
    this->characterUpdateGraphics(character, object);
    this->characterTurn(character, object);
    scene->addItem(object);
    object->setPos(character->getX(), character->getY());
    int character_size = std::max(object->getWidth(), object->getHeight());
    //const float desired_size = 2.0f;
    const float desired_size = 1.0f;
    float character_scale = desired_size / (float)character_size;
    qDebug("character %s size %d scale %f", character->getName().c_str(), character_size, character_scale);
    //object->setTransformOriginPoint(-32.0f*character_scale, -46.0f*character_scale);
    //object->setTransformOriginPoint(-32.0f/32.0f, -46.0f/32.0f);
    object->setTransformOriginPoint(-desired_size*64.0f/128.0f, -desired_size*92.0f/128.0f);
    object->setScale(character_scale);

    character->setListener(this, object);
    //item->setAnimationSet("attack"); // test
}

void PlayingGamestate::clickedStats() {
    LOG("clickedStats()\n");
    this->closeSubWindow();

    new StatsWindow(this);
    game_g->getScreen()->setPaused(true);
}

void PlayingGamestate::clickedItems() {
    LOG("clickedItems()\n");
    this->closeSubWindow();

    new ItemsWindow(this);
    game_g->getScreen()->setPaused(true);
}

void PlayingGamestate::clickedJournal() {
    LOG("clickedJournal()\n");
    this->closeSubWindow();
    this->playSound("turn_page");
    stringstream str;
    str << "<html><body>";
    str << "<h1>Journal</h1>";
    str << this->journal_ss.str();
    str << "</body></html>";
    this->showInfoWindow(str.str());
}

void PlayingGamestate::clickedOptions() {
    LOG("clickedOptions()\n");
    this->closeSubWindow();

    game_g->getScreen()->setPaused(true);

    QWidget *subwindow = new QWidget();
    this->addWidget(subwindow);

    QVBoxLayout *layout = new QVBoxLayout();
    subwindow->setLayout(layout);

    QPushButton *quitButton = new QPushButton("Quit game");
    game_g->initButton(quitButton);
    quitButton->setShortcut(QKeySequence(Qt::Key_Q));
    quitButton->setToolTip("Quit current game, and return to the main menu (Q)");
    quitButton->setFont(game_g->getFontBig());
    quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(quitButton);
    connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));

    QPushButton *saveButton = new QPushButton("Save game");
    game_g->initButton(saveButton);
    saveButton->setShortcut(QKeySequence(Qt::Key_S));
    saveButton->setToolTip("Save the current game (S)");
    saveButton->setFont(game_g->getFontBig());
    saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(saveButton);
    connect(saveButton, SIGNAL(clicked()), this, SLOT(clickedSave()));

    QPushButton *closeButton = new QPushButton("Back to game");
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Escape));
    closeButton->setFont(game_g->getFontBig());
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeSubWindow()));

    //subwindow->showFullScreen();
    //game_g->getScreen()->getMainWindow()->hide();
}

void PlayingGamestate::clickedRest() {
    LOG("clickedRest()\n");
    if( c_location->hasEnemies(this) ) {
        //game_g->showInfoDialog("Rest", "You cannot rest here - enemies are nearby.");
        this->showInfoDialog("You cannot rest here - enemies are nearby.");
        return;
    }
    //if( game_g->askQuestionDialog("Rest", "Rest until fully healed?") ) {
    if( this->askQuestionDialog("Rest until fully healed?") ) {
        int health_restore_percent = 100 - this->player->getHealthPercent();
        int time = (int)(health_restore_percent*10.0f/100.0f + 0.5f);
        if( time == 0 ) {
            time = 1;
        }
        this->player->restoreHealth();
        this->player->expireProfileEffects();
        stringstream str;
        str << "Rested for " << time << " hour";
        if( time > 1 )
            str << "s";
        this->addTextEffect(str.str(), player->getPos(), 2000);
    }
}

void PlayingGamestate::clickedSave() {
    LOG("PlayingGamestate::clickedSave()\n");
    if( c_location->hasEnemies(this) ) {
        //game_g->showInfoDialog("Save", "You cannot save here - enemies are nearby.");
        this->showInfoDialog("You cannot save here - enemies are nearby.");
        return;
    }
    new SaveGameWindow(this);
}

void PlayingGamestate::clickedQuit() {
    LOG("clickedQuit()\n");
    this->closeSubWindow();
    this->quitGame();
}

void PlayingGamestate::showInfoWindow(const string &html) {
    // n.b., different to showInfoDialog, as this doesn't block and wait for an answer
    qDebug("showInfoWindow()\n");

    game_g->getScreen()->setPaused(true);

    QWidget *subwindow = new QWidget();
    this->addWidget(subwindow);

    QVBoxLayout *layout = new QVBoxLayout();
    subwindow->setLayout(layout);

    QWebView *label = new QWebView();
    game_g->setWebView(label);
    label->setHtml(html.c_str());
    layout->addWidget(label);

    QPushButton *closeButton = new QPushButton("Continue");
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeSubWindow()));

    //this->showInfoDialog(html);
}

void PlayingGamestate::closeSubWindow() {
    LOG("closeSubWindow\n");
    int n_stacked_widgets = this->main_stacked_widget->count();
    if( n_stacked_widgets > 1 ) {
        QWidget *subwindow = this->main_stacked_widget->widget(n_stacked_widgets-1);
        this->main_stacked_widget->removeWidget(subwindow);
        subwindow->deleteLater();
        if( n_stacked_widgets == 2 ) {
            game_g->getScreen()->setPaused(false);
            this->view->setEnabled(true);
            this->view->grabKeyboard();
        }
    }
}

void PlayingGamestate::closeAllSubWindows() {
    LOG("closeAllSubWindows");
    while( this->main_stacked_widget->count() > 1 ) {
        QWidget *subwindow = this->main_stacked_widget->widget(this->main_stacked_widget->count()-1);
        this->main_stacked_widget->removeWidget(subwindow);
        subwindow->deleteLater();
    }
    game_g->getScreen()->setPaused(false);
    this->view->setEnabled(true);
    this->view->grabKeyboard();
}

void PlayingGamestate::quitGame() {
    //qApp->quit();

    //if( game_g->askQuestionDialog("Quit", "Are you sure you wish to quit?") ) {
    if( this->askQuestionDialog("Are you sure you wish to quit?") ) {
        GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
        game_g->pushMessage(game_message);
    }
}

void PlayingGamestate::update() {
    int elapsed_ms = game_g->getScreen()->getGameTimeTotalMS();

    if( game_g->getScreen()->isPaused() ) {
        return;
    }

    //qDebug("PlayingGamestate::update()");
    // scroll
    bool scrolled = false;
    int real_time_ms = game_g->getScreen()->getRealTimeFrameMS();
    float speed = (4.0f * real_time_ms)/1000.0f;
    if( !mobile_c ) {
        // scroll due to mouse at edge of screen
        int m_x = QCursor::pos().x();
        int m_y = QCursor::pos().y();
        int screen_width = QApplication::desktop()->width();
        int screen_height = QApplication::desktop()->height();
        //qDebug("mouse at %d, %d", m_x, m_y);
        //qDebug("screen size %d x %d", screen_width, screen_height);
        QPointF centre = this->view->mapToScene( this->view->rect() ).boundingRect().center();
        if( m_x <= 0 ) {
            centre.setX( centre.x() - speed );
            this->view->centerOn(centre);
            scrolled = true;
        }
        else if( m_x >= screen_width-1 ) {
            centre.setX( centre.x() + speed );
            this->view->centerOn(centre);
            scrolled = true;
        }
        if( m_y <= 0 ) {
            centre.setY( centre.y() - speed );
            this->view->centerOn(centre);
            scrolled = true;
        }
        else if( m_y >= screen_height-1 ) {
            centre.setY( centre.y() + speed );
            this->view->centerOn(centre);
            scrolled = true;
        }
    }
    if( !scrolled && !mobile_c ) {
        // scroll due to player near the edge
        // disabed for touchscreens, as makes drag-scrolling harder
        QPoint player_pos = this->view->mapFromScene(this->player->getX(), this->player->getY());
        if( player_pos.x() >= 0 && player_pos.x() < this->view->width() && player_pos.y() > 0 && player_pos.y() < this->view->height() ) {
            const int wid = 64;
            QPointF centre = this->view->mapToScene( this->view->rect() ).boundingRect().center();
            if( player_pos.x() < wid ) {
                centre.setX( centre.x() - speed );
                this->view->centerOn(centre);
                scrolled = true;
            }
            else if( player_pos.x() >= this->view->width() - wid ) {
                centre.setX( centre.x() + speed );
                this->view->centerOn(centre);
                scrolled = true;
            }
            if( player_pos.y() < wid ) {
                centre.setY( centre.y() - speed );
                this->view->centerOn(centre);
                scrolled = true;
            }
            else if( player_pos.y() >= this->view->height() - wid ) {
                centre.setY( centre.y() + speed );
                this->view->centerOn(centre);
                scrolled = true;
            }
        }
    }

    // update due to keyboard input
    {
        // n.b., we need to call setDirection() on the player, so that the direction still gets set even if the player can't move in that direction (e.g., blocked by NPC or scenery) - needed to we can still do Action on that NPC or scenery!
        bool moved = false;
        Vector2D dest = player->getPos();
        const float step = 0.25f;
        if( this->view->keyDown(MainGraphicsView::KEY_L) ) {
            moved = true;
            dest.x -= step;
        }
        else if( this->view->keyDown(MainGraphicsView::KEY_R) ) {
            moved = true;
            dest.x += step;
        }
        if( this->view->keyDown(MainGraphicsView::KEY_U) ) {
            moved = true;
            dest.y -= step;
        }
        else if( this->view->keyDown(MainGraphicsView::KEY_D) ) {
            moved = true;
            dest.y += step;
        }
        if( this->view->keyDown(MainGraphicsView::KEY_LU) ) {
            moved = true;
            dest.x -= step;
            dest.y -= step;
        }
        else if( this->view->keyDown(MainGraphicsView::KEY_RU) ) {
            moved = true;
            dest.x += step;
            dest.y -= step;
        }
        else if( this->view->keyDown(MainGraphicsView::KEY_LD) ) {
            moved = true;
            dest.x -= step;
            dest.y += step;
        }
        else if( this->view->keyDown(MainGraphicsView::KEY_RD) ) {
            moved = true;
            dest.x += step;
            dest.y += step;
        }
        if( moved ) {
            this->player->setDirection(dest - player->getPos());
            this->view->centreOnPlayer();
            this->requestPlayerMove(dest, NULL);
        }
    }

    // test for fog of war
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        if( character != this->player ) {
            bool is_visible = false;
            float dist = ( character->getPos() - player->getPos() ).magnitude();
            if( dist <= hit_range_c ) {
                // assume always visible
                is_visible = true;
            }
            else if( dist <= npc_visibility_c ) {
                // check line of sight
                Vector2D hit_pos;
                if( !c_location->intersectSweptSquareWithBoundaries(&hit_pos, false, player->getPos(), character->getPos(), 0.0f, Location::INTERSECTTYPE_VISIBILITY, NULL, false) ) {
                    is_visible = true;
                }
            }
            character->setVisible(is_visible);
            AnimatedObject *object = static_cast<AnimatedObject *>(character->getListenerData());
            object->setVisible(is_visible);
        }
    }

    scene->advance();

    for(set<Scenery *>::iterator iter = c_location->scenerysBegin(); iter != c_location->scenerysEnd(); ++iter) {
        Scenery *scenery = *iter;
        if( scenery->getActionType().length() > 0 ) {
            if( elapsed_ms >= scenery->getActionLastTime() + scenery->getActionDelay() ) {
                bool has_effect = false;
                if( scenery->getActionType() == "harm_player" ) {
                    if( !player->isDead() && scenery->isOn(player) ) {
                        LOG("scenery %s harms player\n", scenery->getName().c_str());
                        has_effect = true;
                        player->decreaseHealth(this, scenery->getActionValue(), false, false);
                    }
                }
                else {
                    LOG("unknown action type: %s\n", scenery->getActionType().c_str());
                    ASSERT_LOGGER(false);
                }

                if( has_effect ) {
                    scenery->setActionLastTime(elapsed_ms);
                }
            }
        }
    }

    // terror
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        if( character != player && character->isVisible() && character->getCausesTerror() && !character->hasDoneTerror() ) {
            int roll = rollDice(2, 6, character->getTerrorEffect());
            int bravery = player->getProfileIntProperty(profile_key_B_c);
            qDebug("Terror? Roll %d vs %d", roll, bravery);
            if( roll > bravery )
            {
                player->paralyse(5000);
                this->addTextEffect("You are too terrified to move!", player->getPos(), 5000);
            }
            character->setDoneTerror(true);
        }
    }

    vector<Character *> delete_characters;
    //LOG("update characters\n");
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        //LOG("update: %s\n", character->getName().c_str());
        if( character->update(this) ) {
            LOG("character is about to die: %s\n", character->getName().c_str());
            delete_characters.push_back(character);
        }
    }
    //LOG("done update characters\n");
    for(vector<Character *>::iterator iter = delete_characters.begin(); iter != delete_characters.end(); ++iter) {
        Character *character = *iter;
        LOG("character has died: %s\n", character->getName().c_str());
        c_location->removeCharacter(character);

        for(set<Character *>::iterator iter3 = c_location->charactersBegin(); iter3 != c_location->charactersEnd(); ++iter3) {
            Character *ch = *iter3;
            if( ch->getTargetNPC() == character ) {
                ch->setTargetNPC(NULL);
            }
        }

        delete character; // also removes character from the QGraphicsScene, via the listeners
        if( character == this->player ) {
            this->player = NULL;
            //game_g->showInfoDialog("Game over", "You have died!");
            //this->showInfoDialog("Game over!\n\nYou have died!");
            //this->showInfoDialog("Game over!\n\nYou have died!", ":/gfx/scenes/death.jpg");
            this->showInfoDialog("Game over!\n\nYou have died!", string(DEPLOYMENT_PATH) + "gfx/scenes/death.jpg");
            //qDebug("send game message: GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS");
            GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
            game_g->pushMessage(game_message);
        }
    }
    if( this->player != NULL && delete_characters.size() > 0 ) {
        this->checkQuestComplete();
    }
    //qDebug("PlayingGamestate::update() exit");
}

void PlayingGamestate::render() {
    // n.b., won't render immediately, but schedules for repainting from Qt's main event loop
    this->view->update();
    gui_overlay->update(); // force the GUI overlay to be updated every frame (otherwise causes drawing problems on Windows at least)
}

void PlayingGamestate::checkQuestComplete() {
    qDebug("PlayingGamestate::checkQuestComplete()");
    if( !this->quest->isCompleted() && this->quest->testIfComplete(this) ) {
        //game_g->showInfoDialog("Quest complete", "You have completed the quest! Now return to the dungeon exit.");
        this->showInfoDialog("Quest complete!\n\nYou have completed the quest! Now return to the dungeon exit.");
    }
    //qDebug("PlayingGamestate::checkQuestComplete() end");
}

void PlayingGamestate::characterUpdateGraphics(const Character *character, void *user_data) {
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    if( object == NULL ) {
        throw string("character has NULL graphics object");
    }
    object->clearAnimationLayers();
    if( character == player ) {
        //object->addAnimationLayer( this->animation_layers["clothes"] );
        //object->addAnimationLayer( this->animation_layers["head"] );
        object->addAnimationLayer( this->animation_layers["player"] );
        if( character->getCurrentWeapon() != NULL ) {
            object->addAnimationLayer( this->animation_layers[ character->getCurrentWeapon()->getAnimationName() ] );
        }
        if( character->getCurrentShield() != NULL ) {
            object->addAnimationLayer( this->animation_layers[ character->getCurrentShield()->getAnimationName() ] );
        }
    }
    else {
        if( character->isStaticImage() ) {
            object->setStaticImage( this->npc_static_images[ character->getAnimationName() ] );
        }
        else {
            object->addAnimationLayer( this->animation_layers[ character->getAnimationName() ] );
        }
    }
}

void PlayingGamestate::characterTurn(const Character *character, void *user_data) {
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    Vector2D dir = character->getDirection();
    float angle = atan2(dir.y, dir.x);
    if( angle < 0.0f )
        angle += (float)(2.0*M_PI);
    angle /= (float)(2.0*M_PI);
    float turn = angle*((int)N_DIRECTIONS) + 0.5f;
    int turn_i = (int)turn;
    /*qDebug("angle %f", angle);
    qDebug("turn %f", turn);
    qDebug("turn_i %d", turn_i);*/
    turn_i += 4; // 0 is west
    Direction direction = (Direction)(turn_i % (int)N_DIRECTIONS);
    object->setDimension(direction);
}

void PlayingGamestate::characterMoved(Character *character, void *user_data) {
    //QGraphicsItem *item = static_cast<QGraphicsItem *>(user_data);
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    QPointF old_pos = object->pos();
    QPointF new_pos(character->getX(), character->getY());
    if( new_pos != old_pos ) {
        object->setPos(new_pos);
        /*Direction direction = DIRECTION_W;
        if( new_pos.x() > old_pos.x() ) {
            direction = DIRECTION_E;
        }*/
        QPointF dir = new_pos - old_pos;
        Vector2D vdir(dir.x(), dir.y());
        character->setDirection(vdir);
        //this->characterTurn(character, user_data, vdir);

        if( character == this->player ) {
            // handle popup text
            Vector2D old_pos_v(old_pos.x(), old_pos.y());
            Vector2D new_pos_v = character->getPos();
            for(set<Scenery *>::const_iterator iter = this->c_location->scenerysBegin(); iter != this->c_location->scenerysEnd(); ++iter) {
                const Scenery *scenery = *iter;
                if( scenery->getPopupText().length() > 0 ) {
                    Vector2D scenery_pos = scenery->getPos();
                    float old_dist = (scenery_pos - old_pos_v).magnitude();
                    float new_dist = (scenery_pos - new_pos_v).magnitude();
                    const float influence_radius_c = 3.0f;
                    if( new_dist <= influence_radius_c && old_dist > influence_radius_c ) {
                        qDebug("popup text: dists %f, %f ; at %f, %f", old_dist, new_dist, scenery->getX(), scenery->getY());
                        this->addTextEffect(scenery->getPopupText(), scenery->getPos(), 2000);
                    }
                }
            }
        }
    }
    if( character == this->player ) {
        // handle traps
        vector<Trap *> delete_traps;
        for(set<Trap *>::iterator iter = this->c_location->trapsBegin(); iter != this->c_location->trapsEnd(); ++iter) {
            Trap *trap = *iter;
            if( trap->isSetOff(character) ) {
                trap->setOff(this, character);
                delete_traps.push_back(trap);
                if( character->isDead() ) {
                    break;
                }
            }
        }
        for(vector<Trap *>::iterator iter = delete_traps.begin(); iter != delete_traps.end(); ++iter) {
            Trap *trap = *iter;
            c_location->removeTrap(trap);
        }

        if( !character->isDead() ) {
            this->updateVisibility(character->getPos());
        }
    }
}

void PlayingGamestate::characterSetAnimation(const Character *character, void *user_data, const string &name, bool force_restart) {
    //LOG("PlayingGamestate::characterSetAnimation(%s, %d, %s, %d)\n", character->getName().c_str(), user_data, name.c_str(), force_restart);
    //LOG("    at %d\n", game_g->getScreen()->getGameTimeTotalMS());
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    object->setAnimationSet(name, force_restart);
}

void PlayingGamestate::characterDeath(Character *character, void *user_data) {
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    character->setListener(NULL, NULL);
    delete object;
}

/*void PlayingGamestate::mouseClick(int m_x, int m_y) {
    qDebug("PlayingGamestate::mouseClick(%d, %d)", m_x, m_y);
}*/

void PlayingGamestate::clickedOnNPC(Character *character) {
    // should be called when clicking, even if character is NULL
    if( character != NULL ) {
        float dist_from_player = (player->getPos() - character->getPos()).magnitude();
        if( dist_from_player <= talk_range_c ) {
            if( character->canTalk() ) {
                stringstream message;
                message << "<b>";
                message << character->getName();
                message << "</b>: ";
                if( character->isInteractionCompleted() ) {
                    message << character->getTalkOpeningInteractionComplete();
                }
                else if( !character->hasTalked() ) {
                    message << character->getTalkOpeningInitial();
                }
                else {
                    message << character->getTalkOpeningLater();
                }
                message << "<br/>";
                character->setHasTalked(true);
                for(;;) {
                    vector<string> buttons;
                    //vector<string> answers;
                    vector<const TalkItem *> talk_items;
                    if( !character->isHostile() ) {
                        // being hostile means the character has become hostile whilst talking, so now the only thing left is to say Goodbye!
                        int count=0;
                        for(vector<TalkItem>::const_iterator iter = character->talkItemsBegin(); iter != character->talkItemsEnd(); ++iter, count++) {
                            const TalkItem *talk_item = &*iter;
                            if( talk_item->while_not_done && character->isInteractionCompleted() ) {
                                continue;
                            }
                            if( talk_item->objective && !character->canCompleteInteraction(this) ) {
                                continue;
                            }
                            buttons.push_back(talk_item->question);
                            //answers.push_back(talk_item->answer);
                            talk_items.push_back(talk_item);
                        }
                    }
                    buttons.push_back("Goodbye");
                    InfoDialog *dialog = new InfoDialog(message.str(), "", buttons, false, true, true);
                    this->addWidget(dialog);
                    dialog->scrollToBottom();
                    int result = dialog->exec();
                    this->closeSubWindow();
                    ASSERT_LOGGER(result >= 0);
                    ASSERT_LOGGER(result < buttons.size());
                    if( result == buttons.size()-1 ) {
                        break;
                    }
                    else {
                        const TalkItem *talk_item = talk_items.at(result);
                        message << "<b>";
                        message << player->getName();
                        message << "</b>: ";
                        message << buttons.at(result);
                        message << "<br/>";

                        message << "<b>";
                        message << character->getName();
                        message << "</b>: ";
                        //message << answers.at(result);
                        message << talk_item->answer;
                        message << "<br/>";
                        if( talk_item->objective ) {
                            character->completeInteraction(this);
                        }
                        if( talk_item->action.length() > 0 ) {
                            if( talk_item->action == "SET_HOSTILE") {
                                character->setHostile(true);
                            }
                            else {
                                LOG("character %s, talk item %s has unknown action: %s\n", character->getName().c_str(), talk_item->question.c_str(), talk_item->action.c_str());
                                ASSERT_LOGGER(false);
                            }
                        }
                    }
                }
            }
            else if( character->getShop().length() > 0 ) {
                const Shop *shop = NULL;
                for(vector<Shop *>::const_iterator iter = this->shopsBegin(); iter != this->shopsEnd() && shop==NULL; ++iter) {
                    const Shop *this_shop = *iter;
                    if( this_shop->getName() == character->getShop() ) {
                        shop = this_shop;
                    }
                }
                ASSERT_LOGGER(shop != NULL);
                if( shop != NULL ) {
                    LOG("visit shop: %s\n", shop->getName().c_str());
                    new TradeWindow(this, shop->getItems(), shop->getCosts());
                }
            }
        }
    }
    player->setTargetNPC(character); // n.b., if no NPC selected, we therefore set to NULL
}

bool PlayingGamestate::handleClickForItems(Vector2D dest) {
    qDebug("PlayingGamestate::handleClickForItems");
    bool done = false;
    float min_dist = 0.0f;
    const float click_tol_items_c = 0.25f;
    // search for clicking on an item
    vector<Item *> pickup_items;
    for(set<Item *>::iterator iter = c_location->itemsBegin(); iter != c_location->itemsEnd(); ++iter) {
        Item *item = *iter;
        float icon_width = item->getIconWidth();
        float dist_from_click = (dest - item->getPos()).magnitude();
        float dist_from_player = (player->getPos() - item->getPos()).magnitude();
        if( dist_from_click <= sqrt(0.5f) * icon_width + click_tol_items_c && dist_from_player <= npc_radius_c + sqrt(0.5f)*icon_width ) {
            done = true;
            min_dist = dist_from_click;
            pickup_items.push_back(item);
        }
    }
    if( pickup_items.size() == 1 ) {
        Item *picked_item = pickup_items.at(0);
        this->addTextEffect(picked_item->getName(), player->getPos(), 2000);
        if( picked_item->getType() == ITEMTYPE_CURRENCY ) {
            this->playSound("coin");
        }
        player->pickupItem(picked_item);
        this->checkQuestComplete();
    }
    else if( pickup_items.size() > 1 ) {
        new ItemsPickerWindow(this, pickup_items);
        game_g->getScreen()->setPaused(true);
    }
    return done;
}

bool PlayingGamestate::clickedOnScenerys(bool *move, Scenery **ignore_scenery, const vector<Scenery *> &clicked_scenerys) {
    bool done = false;
    for(vector<Scenery *>::const_iterator iter = clicked_scenerys.begin(); iter != clicked_scenerys.end() && !done; ++iter) {
        Scenery *scenery = *iter;
        qDebug("clicked on scenery: %s", scenery->getName().c_str());

        bool confirm_ok = true;
        if( scenery->getConfirmText().length() > 0 ) {
            confirm_ok = this->askQuestionDialog(scenery->getConfirmText());
        }

        if( confirm_ok ) {
            if( scenery->getTrap() != NULL ) {
                scenery->getTrap()->setOff(this, player);
                scenery->setTrap(NULL);
                if( player->isDead() ) {
                    break;
                }
            }

            bool is_locked = false;
            if( scenery->isLocked() ) {
                // can we unlock it?
                is_locked = true;
                string unlock_item_name = scenery->getUnlockItemName();
                Item *item = NULL;
                if( unlock_item_name.length() > 0 ) {
                    qDebug("search for %s", unlock_item_name.c_str());
                    for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd() && is_locked; ++iter) {
                        item = *iter;
                        //LOG("    compare to: %s\n", item->getKey().c_str());
                        if( item->getKey() == unlock_item_name ) {
                            is_locked = false;
                        }
                    }
                }
                if( is_locked ) {
                    done = true;
                    if( !scenery->isLockedSilent() ) {
                        this->playSound("lock");
                    }
                    if( scenery->getLockedText().length() == 0 ) {
                        stringstream str;
                        str << "The " << scenery->getName() << " is locked!";
                        this->addTextEffect(str.str(), player->getPos(), 2000);
                    }
                    else if( scenery->getLockedText() != "none" ) {
                        this->addTextEffect(scenery->getLockedText(), player->getPos(), 2000);
                    }
                }
                else {
                    ASSERT_LOGGER(item != NULL);
                    stringstream str;
                    if( scenery->getUnlockText().length() == 0 ) {
                        str << "You unlock the " << scenery->getName() << ".";
                    }
                    else {
                        str << scenery->getUnlockText();
                    }
                    this->addTextEffect(str.str(), player->getPos(), 2000);
                    scenery->setLocked(false);
                    qDebug("isLockedUsedUp? %d", scenery->isLockedUsedUp());
                    if( scenery->isLockedUsedUp() ) {
                        player->takeItem(item);
                        delete item;
                        item = NULL;
                    }
                    player->addXP(this, scenery->getUnlockXP());
                }
            }

            if( !is_locked ) {
                if( scenery->getNItems() > 0 ) {
                    done = true;
                    bool all_gold = true;
                    for(set<Item *>::iterator iter = scenery->itemsBegin(); iter != scenery->itemsEnd(); ++iter) {
                        Item *item = *iter;
                        if( item->getType() != ITEMTYPE_CURRENCY ) {
                            all_gold = false;
                        }
                        c_location->addItem(item, player->getX(), player->getY());
                    }
                    scenery->eraseAllItems();
                    this->addTextEffect(all_gold ? "Found some gold!" : "Found some items!", player->getPos(), 2000);
                }

                if( scenery->canBeOpened() && !scenery->isOpened() ) {
                    done = true;
                    this->playSound("container");
                    scenery->setOpened(true);
                }

                if( scenery->isDoor() ) {
                    done = true;
                    qDebug("clicked on a door");
                    // open door
                    if( scenery->getName() == "Door" ) {
                        this->playSound("door");
                    }
                    this->addTextEffect("Opening door...", scenery->getPos(), 1000);
                    qApp->processEvents(); // so that the text effect gets displayed, whilst recalculating the location's distance graph
                    c_location->removeScenery(scenery);
                    delete scenery;
                    scenery = NULL;
                    *ignore_scenery = NULL;
                }
                else if( scenery->isExit() ) {
                    done = true;
                    LOG("clicked on an exit\n");
                    // exit
                    if( scenery->getName() == "Door" ) {
                        this->playSound("door");
                    }
                    this->closeSubWindow(); // just in case
                    this->quest->testIfComplete(this); // recall, in case quest no longer completed (e.g., player has dropped an item that is needed)
                    if( this->getQuest()->isCompleted() ) {
                        this->getQuest()->getQuestObjective()->completeQuest(this);
                        /*int gold = this->getQuest()->getQuestObjective()->getGold();
                        this->player->addGold(gold);*/
                        string completed_text = this->getQuest()->getCompletedText();
                        completed_text = convertToHTML(completed_text);
                        this->showInfoDialog(completed_text);
                    }
                    new CampaignWindow(this);
                    game_g->getScreen()->setPaused(true);
                    this->player->restoreHealth();
                    this->player->expireProfileEffects();
                }
                else if( scenery->getExitLocation().length() > 0 ) {
                    done = true;
                    if( scenery->getName() == "Door" ) {
                        this->playSound("door");
                    }
                    LOG("clicked on an exit location: %s\n", scenery->getExitLocation().c_str());
                    Location *new_location = quest->findLocation(scenery->getExitLocation());
                    ASSERT_LOGGER(new_location != NULL);
                    if( new_location != NULL ) {
#if !defined(Q_OS_SYMBIAN) // autosave disabled due to being slow on Nokia 5800 at least
                        this->autoSave();
#endif
                        this->moveToLocation(new_location, scenery->getExitLocationPos());
                        *move = false;
                    }
                }
                else if( scenery->getInteractType().length() > 0 ) {
                    done = true;
                    LOG("interact_type: %s\n", scenery->getInteractType().c_str());
                    string dialog_text;
                    vector<string> options = scenery->getInteractionText(&dialog_text);
                    if( options.size() == 0 ) {
                        // auto-interact
                        scenery->interact(this, 0);
                    }
                    else {
                        //if( this->askQuestionDialog(dialog_text) ) {
                        //InfoDialog *dialog = InfoDialog::createInfoDialogYesNo(dialog_text);
                        InfoDialog *dialog = new InfoDialog(dialog_text, "", options, false, false, true);
                        this->addWidget(dialog);
                        int result = dialog->exec();
                        LOG("scenery iteraction dialog returns %d\n", result);
                        this->closeSubWindow();
                        if( result != options.size()-1 ) {
                            scenery->interact(this, result);
                        }
                    }
                }
                else if( scenery->getDescription().length() > 0 ) {
                    done = true;
                    string description = scenery->getDescription();
                    description = convertToHTML(description);
                    this->showInfoDialog(description);
                }
            }
        }
    }
    return done;
}

bool PlayingGamestate::handleClickForScenerys(bool *move, Scenery **ignore_scenery, Vector2D dest) {
    //qDebug("PlayingGamestate::handleClickForScenerys(): %f, %f", dest.x, dest.y);
    // search for clicking on a scenery
    const float click_tol_scenery_c = 0.0f;
    vector<Scenery *> clicked_scenerys;
    for(set<Scenery *>::iterator iter = c_location->scenerysBegin(); iter != c_location->scenerysEnd(); ++iter) {
        Scenery *scenery = *iter;
        Vector2D scenery_pos = scenery->getPos();
        float scenery_width = scenery->getWidth();
        float scenery_height = scenery->getHeight();
        float dist_from_click = distFromBox2D(scenery_pos, scenery_width, scenery_height, dest);
        //LOG("dist_from_click for scenery %s : %f", scenery->getName().c_str(), dist_from_click);
        if( dist_from_click <= npc_radius_c && scenery->isBlocking() ) {
            // clicked on or near this scenery
            *ignore_scenery = scenery;
        }
        if( dist_from_click <= click_tol_scenery_c ) {
            // clicked on this scenery
            float player_dist = distFromBox2D(scenery_pos, scenery_width, scenery_height, player->getPos());
            //LOG("    player_dist : %f", player_dist);
            if( player_dist <= npc_radius_c + 0.5f ) {
                clicked_scenerys.push_back(scenery);
            }
        }
    }

    bool done = clickedOnScenerys(move, ignore_scenery, clicked_scenerys);
    return done;
}

void PlayingGamestate::actionCommand(bool pickup_only) {
    qDebug("PlayingGamestate::actionCommand()");
    if( game_g->getScreen()->isPaused() ) {
        game_g->getScreen()->setPaused(false);
    }

    if( player != NULL && !player->isDead() && !player->isParalysed() ) {
        bool done = false;
        Vector2D forward_dest1 = player->getPos() + player->getDirection() * npc_radius_c * 1.1f;
        Vector2D forward_dest2 = player->getPos() + player->getDirection() * npc_radius_c * 2.0f;

        // search for NPC
        if( !pickup_only )
        {
            float min_dist = 0.0f;
            Character *target_npc = NULL;
            for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
                Character *character = *iter;
                if( character == player )
                    continue;
                if( !character->isVisible() )
                    continue;
                float dist_from_click = (forward_dest2 - character->getPos()).magnitude();
                if( dist_from_click <= npc_radius_c ) {
                    if( target_npc == NULL || dist_from_click < min_dist ) {
                        // clicked on this character
                        done = true;
                        target_npc = character;
                        min_dist = dist_from_click;
                    }
                }
            }
            if( target_npc == NULL && player->getCurrentWeapon() != NULL && player->getCurrentWeapon()->isRanged() ) {
                // for ranged weapons, pick closest visible enemy to player
                for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
                    Character *character = *iter;
                    if( character == player )
                        continue;
                    if( !character->isVisible() )
                        continue;
                    if( !character->isHostile() )
                        continue;
                    float dist_from_player = (player->getPos() - character->getPos()).magnitude();
                    if( target_npc == NULL || dist_from_player < min_dist ) {
                        done = true;
                        target_npc = character;
                        min_dist = dist_from_player;
                    }
                }
            }
            this->clickedOnNPC(target_npc); // call even if target_npc is NULL
        }

        if( !done && !pickup_only ) {
            bool move = false;
            Scenery *ignore_scenery = NULL;
            done = handleClickForScenerys(&move, &ignore_scenery, forward_dest1);
            if( !done ) {
                done = handleClickForScenerys(&move, &ignore_scenery, forward_dest2);
            }
        }

        if( !done ) {
            // We do items last, unlike with mouse click, so that the player can still do other things even if standing over items.
            // If the player wants to force picking up items, they can do that via a separate shortcut
            done = handleClickForItems( player->getPos() );
        }

    }
}

void PlayingGamestate::clickedMainView(float scene_x, float scene_y) {
    if( game_g->getScreen()->isPaused() ) {
        game_g->getScreen()->setPaused(false);
    }

    if( player != NULL && !player->isDead() && !player->isParalysed() ) {
        //player->setPos(scene_x, scene_y);

        Vector2D dest(scene_x, scene_y);

        bool done = false;
        bool move = true;
        //const float click_tol_c = 0.0f;
        //const float click_tol_c = 0.5f;
        //const float click_tol_npc_c = 0.25f;

        // search for clicking on an NPC
        {
            const float click_tol_npc_c = 0.0f;
            float min_dist = 0.0f;
            Character *target_npc = NULL;
            for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
                Character *character = *iter;
                if( character == player )
                    continue;
                if( !character->isVisible() )
                    continue;
                /*float dist_from_click = (dest - character->getPos()).magnitude();
                if( dist_from_click <= npc_radius_c + click_tol_npc_c ) {
                    if( target_npc == NULL || dist_from_click < min_dist ) {*/
                /*if( target_npc == NULL || dist_from_click < min_dist ) {
                    AnimatedObject *object = static_cast<AnimatedObject *>(character->getListenerData());
                    if( object->contains(object->mapFromScene(QPointF(scene_x, scene_y)) ) ) {*/

                /* For collision detection and so on, we treat the characters as being a circle of radius npc_radius_c, centred on the character position.
                 * But for clicking on an NPC, we want to allow the user to be able to click on the visible graphical area (which may be taller, for example).
                 */
                AnimatedObject *object = static_cast<AnimatedObject *>(character->getListenerData());
                if( object->contains(object->mapFromScene(QPointF(scene_x, scene_y)) ) ) {
                    // clicked on this character
                    {
                        done = true;
                        target_npc = character;
                        min_dist = 0.0f;
                    }
                }
            }
            this->clickedOnNPC(target_npc); // call even if target_npc is NULL
        }

        if( !done ) {
            done = handleClickForItems(dest);
        }

        Scenery *ignore_scenery = NULL; // scenery to ignore for collision detection - so we can move towards a scenery that blocks
        if( !done ) {
            done = handleClickForScenerys(&move, &ignore_scenery, dest);
        }

        if( move && !player->isDead() ) {
            this->requestPlayerMove(dest, ignore_scenery);
        }
    }
}

void PlayingGamestate::requestPlayerMove(Vector2D dest, const Scenery *ignore_scenery) {
    // nudge position due to boundaries
    dest = this->c_location->nudgeToFreeSpace(player->getPos(), dest, npc_radius_c);
    if( dest != player->getPos() ) {
        qDebug("ignoring scenery: %s", ignore_scenery==NULL ? "NULL" : ignore_scenery->getName().c_str());
        player->setDestination(dest.x, dest.y, ignore_scenery);
        if( player->carryingTooMuch() ) {
            this->addTextEffect("You are carrying too much weight to move!", player->getPos(), 2000);
        }
        else if( player->tooWeakForArmour() ) {
            this->addTextEffect("The armour you are wearing is\ntoo heavy for you to move!", player->getPos(), 2000);
        }
    }
}

void PlayingGamestate::updateVisibilityForFloorRegion(FloorRegion *floor_region) {
    /*QGraphicsPolygonItem *gfx_item = static_cast<QGraphicsPolygonItem *>(floor_region->getUserGfxData());
    gfx_item->setVisible( floor_region->isVisible() );
    for(set<Scenery *>::iterator iter = floor_region->scenerysBegin(); iter != floor_region->scenerysEnd(); ++iter) {
        Scenery *scenery = *iter;
        QGraphicsPixmapItem *gfx_item2 = static_cast<QGraphicsPixmapItem *>(scenery->getUserGfxData());
        if( gfx_item2 != NULL ) {
            gfx_item2->setVisible( floor_region->isVisible() );
        }
    }
    for(set<Item *>::iterator iter = floor_region->itemsBegin(); iter != floor_region->itemsEnd(); ++iter) {
        Item *item = *iter;
        QGraphicsPixmapItem *gfx_item2 = static_cast<QGraphicsPixmapItem *>(item->getUserGfxData());
        if( gfx_item2 != NULL ) {
            gfx_item2->setVisible( floor_region->isVisible() );
        }
    }*/
    //qDebug("updateVisibilityForFloorRegion");
    ASSERT_LOGGER( floor_region->isVisible() );
    if( floor_region->isVisible() ) {
        //qDebug("floor_region %d", floor_region);
        QGraphicsPolygonItem *gfx_item = static_cast<QGraphicsPolygonItem *>(floor_region->getUserGfxData());
        //qDebug("gfx_item %d", gfx_item);
        ASSERT_LOGGER(gfx_item != NULL);
        gfx_item->setVisible( true );
        //qDebug("do sceneries");
        for(set<Scenery *>::iterator iter = floor_region->scenerysBegin(); iter != floor_region->scenerysEnd(); ++iter) {
            Scenery *scenery = *iter;
            //qDebug("scenery %d", scenery);
            //qDebug("    %s at %f, %f", scenery->getName().c_str(), scenery->getX(), scenery->getY());
            //qDebug("    at %f, %f", scenery->getX(), scenery->getY());
            QGraphicsPixmapItem *gfx_item2 = static_cast<QGraphicsPixmapItem *>(scenery->getUserGfxData());
            //qDebug("gfx_item2 %d", gfx_item2);
            if( gfx_item2 != NULL ) {
                gfx_item2->setVisible( true );
            }
        }
        //qDebug("do items");
        for(set<Item *>::iterator iter = floor_region->itemsBegin(); iter != floor_region->itemsEnd(); ++iter) {
            Item *item = *iter;
            //qDebug("item %d", item);
            QGraphicsPixmapItem *gfx_item2 = static_cast<QGraphicsPixmapItem *>(item->getUserGfxData());
            //qDebug("gfx_item2 %d", gfx_item2);
            if( gfx_item2 != NULL ) {
                gfx_item2->setVisible( true );
            }
        }
        //qDebug("ping");
    }
    //qDebug("updateVisibilityForFloorRegion done");
}

void PlayingGamestate::updateVisibility(Vector2D pos) {
    vector<FloorRegion *> update_regions = this->c_location->updateVisibility(pos);
    for(vector<FloorRegion *>::iterator iter = update_regions.begin(); iter != update_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        updateVisibilityForFloorRegion(floor_region);
    }
}

void PlayingGamestate::saveItem(FILE *file, const Item *item) const {
    return saveItem(file, item, NULL);
}

void PlayingGamestate::saveItem(FILE *file, const Item *item, const Character *character) const {
    switch( item->getType() ) {
    case ITEMTYPE_GENERAL:
        fprintf(file, "<item");
        break;
    case ITEMTYPE_WEAPON:
        fprintf(file, "<weapon");
        break;
    case ITEMTYPE_SHIELD:
        fprintf(file, "<shield");
        break;
    case ITEMTYPE_ARMOUR:
        fprintf(file, "<armour");
        break;
    case ITEMTYPE_AMMO:
        fprintf(file, "<ammo");
        break;
    case ITEMTYPE_CURRENCY:
        fprintf(file, "<currency");
        break;
    }
    fprintf(file, " name=\"%s\"", item->getKey().c_str()); // n.b., we use the key, not the name, as the latter may be overloaded to give more descriptive names
    fprintf(file, " image_name=\"%s\"", item->getImageName().c_str());
    fprintf(file, " x=\"%f\" y=\"%f\"", item->getX(), item->getY());
    fprintf(file, " icon_width=\"%f\"", item->getIconWidth());
    fprintf(file, " weight=\"%d\"", item->getWeight());
    fprintf(file, " use=\"%s\"", item->getUse().c_str());
    fprintf(file, " use_verb=\"%s\"", item->getUseVerb().c_str());
    fprintf(file, " arg1=\"%d\"", item->getArg1());
    fprintf(file, " arg2=\"%d\"", item->getArg2());
    fprintf(file, " arg1_s=\"%s\"", item->getArg1s().c_str());
    fprintf(file, " rating=\"%d\"", item->getRating());
    fprintf(file, " magical=\"%s\"", item->isMagical() ? "true": "false");
    fprintf(file, " base_template=\"%s\"", item->getBaseTemplate().c_str());
    fprintf(file, " worth_bonus=\"%d\"", item->getWorthBonus());
    // now do subclasses
    switch( item->getType() ) {
    case ITEMTYPE_GENERAL:
        break;
    case ITEMTYPE_WEAPON:
    {
        const Weapon *weapon = static_cast<const Weapon *>(item);
        fprintf(file, " animation_name=\"%s\"", weapon->getAnimationName().c_str());
        fprintf(file, " two_handed=\"%s\"", weapon->isTwoHanded() ? "true": "false");
        fprintf(file, " ranged=\"%s\"", weapon->isRanged() ? "true": "false");
        int damageX = 0, damageY = 0, damageZ = 0;
        weapon->getDamage(&damageX, &damageY, &damageZ);
        fprintf(file, " damageX=\"%d\"", damageX);
        fprintf(file, " damageY=\"%d\"", damageY);
        fprintf(file, " damageZ=\"%d\"", damageZ);
        if( weapon->getMinStrength() > 0 ) {
            fprintf(file, " min_strength=\"%d\"", weapon->getMinStrength());
        }
        if( weapon->getRequiresAmmo() ) {
            fprintf(file, " ammo=\"%s\"", weapon->getAmmoKey().c_str());
        }
        if( character != NULL && weapon == character->getCurrentWeapon() ) {
            fprintf(file, " current_weapon=\"true\"");
        }
        break;
    }
    case ITEMTYPE_SHIELD:
    {
        const Shield *shield = static_cast<const Shield *>(item);
        fprintf(file, " animation_name=\"%s\"", shield->getAnimationName().c_str());
        if( character != NULL && shield == character->getCurrentShield() ) {
            fprintf(file, " current_shield=\"true\"");
        }
        break;
    }
    case ITEMTYPE_ARMOUR:
    {
        const Armour *armour = static_cast<const Armour *>(item);
        if( armour->getMinStrength() > 0 ) {
            fprintf(file, " min_strength=\"%d\"", armour->getMinStrength());
        }
        if( character != NULL && armour == character->getCurrentArmour() ) {
            fprintf(file, " current_armour=\"true\"");
        }
        break;
    }
    case ITEMTYPE_AMMO:
    {
        const Ammo *ammo = static_cast<const Ammo *>(item);
        fprintf(file, " projectile_image_name=\"%s\"", ammo->getProjectileImageName().c_str());
        fprintf(file, " amount=\"%d\"", ammo->getAmount());
        break;
    }
    case ITEMTYPE_CURRENCY:
    {
        const Currency *currency = static_cast<const Currency *>(item);
        fprintf(file, " value=\"%d\"", currency->getValue());
        break;

    }
    }
    fprintf(file, ">");
    fprintf(file, "%s", item->getDescription().c_str());
    switch( item->getType() ) {
    case ITEMTYPE_GENERAL:
        fprintf(file, "</item>");
        break;
    case ITEMTYPE_WEAPON:
        fprintf(file, "</weapon>");
        break;
    case ITEMTYPE_SHIELD:
        fprintf(file, "</shield>");
        break;
    case ITEMTYPE_ARMOUR:
        fprintf(file, "</armour>");
        break;
    case ITEMTYPE_AMMO:
        fprintf(file, "</ammo>");
        break;
    case ITEMTYPE_CURRENCY:
        fprintf(file, "</currency>");
        break;
    }
    fprintf(file, "\n");
}

void PlayingGamestate::saveTrap(FILE *file, const Trap *trap) const {
    fprintf(file, "<trap");
    fprintf(file, " type=\"%s\"", trap->getType().c_str());
    fprintf(file, " x=\"%f\" y=\"%f\"", trap->getX(), trap->getY());
    fprintf(file, " w=\"%f\" h=\"%f\"", trap->getWidth(), trap->getHeight());
    fprintf(file, " difficulty=\"%d\"", trap->getDifficulty());
    fprintf(file, " rating=\"%d\"", trap->getRating());
    fprintf(file, " />\n");
}

bool PlayingGamestate::saveGame(const string &filename) const {
    LOG("PlayingGamestate::saveGame(%s)\n", filename.c_str());
    string full_path = game_g->getApplicationFilename(savegame_folder + filename);
    LOG("full path: %s\n", full_path.c_str());

    FILE *file = fopen(full_path.c_str(), "wt");
    if( file == NULL ) {
        LOG("failed to create file\n");
        return false;
    }

    game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);

    const int savegame_version = 1;

    fprintf(file, "<?xml version=\"1.0\" ?>\n");
    fprintf(file, "<savegame major=\"%d\" minor=\"%d\" savegame_version=\"%d\">\n", versionMajor, versionMinor, savegame_version);
    fprintf(file, "\n");
    fprintf(file, "<game difficulty=\"%s\"/>", Game::getDifficultyString(difficulty).c_str());
    fprintf(file, "<current_quest name=\"%s\"/>\n", this->quest_list.at(this->c_quest_indx).getFilename().c_str());
    fprintf(file, "\n");

    qDebug("save locations");
    for(vector<Location *>::const_iterator iter_loc = quest->locationsBegin(); iter_loc != quest->locationsEnd(); ++iter_loc) {
        const Location *location = *iter_loc;

        string type_str;
        switch( location->getType() ) {
        case Location::TYPE_INDOORS:
            type_str = "indoors";
            break;
        case Location::TYPE_OUTDOORS:
            type_str = "outdoors";
            break;
        default:
            ASSERT_LOGGER(false);
            break;
        }
        fprintf(file, "<location name=\"%s\" type=\"%s\" lighting_min=\"%d\">\n\n", location->getName().c_str(), type_str.c_str(), location->getLightingMin());

        qDebug("save location images");
        fprintf(file, "<background image_name=\"%s\"/>\n", location->getBackgroundImageName().c_str());
        fprintf(file, "<floor image_name=\"%s\"/>\n", location->getFloorImageName().c_str());
        fprintf(file, "<wall image_name=\"%s\"/>\n", location->getWallImageName().c_str());
        fprintf(file, "\n");

        for(size_t i=0;i<location->getNFloorRegions();i++) {
            const FloorRegion *floor_region = location->getFloorRegion(i);
            fprintf(file, "<floorregion shape=\"polygon\" visible=\"%s\">\n", floor_region->isVisible() ? "true" : "false");
            for(size_t j=0;j<floor_region->getNPoints();j++) {
                Vector2D point = floor_region->getPoint(j);
                fprintf(file, "    <floorregion_point x=\"%f\" y=\"%f\"/>\n", point.x, point.y);
            }
            fprintf(file, "</floorregion>\n");
        }
        fprintf(file, "\n");

        qDebug("save player and npcs");
        for(set<Character *>::const_iterator iter = location->charactersBegin(); iter != location->charactersEnd(); ++iter) {
            const Character *character = *iter;
            if( player == character )
                fprintf(file, "<player");
            else {
                fprintf(file, "<npc");
                fprintf(file, " is_hostile=\"%s\"", character->isHostile() ? "true": "false");
                fprintf(file, " animation_name=\"%s\"", character->getAnimationName().c_str());
            }
            fprintf(file, " static_image=\"%s\"", character->isStaticImage() ? "true": "false");
            fprintf(file, " name=\"%s\"", character->getName().c_str());
            fprintf(file, " is_dead=\"%s\"", character->isDead() ? "true": "false");
            fprintf(file, " x=\"%f\" y=\"%f\"", character->getX(), character->getY());
            if( character->hasDefaultPosition() ) {
                fprintf(file, " default_x=\"%f\" default_y=\"%f\"", character->getDefaultX(), character->getDefaultY());
            }
            fprintf(file, " health=\"%d\"", character->getHealth());
            fprintf(file, " max_health=\"%d\"", character->getMaxHealth());
            /*fprintf(file, " FP=\"%d\"", character->getFP());
            fprintf(file, " BS=\"%d\"", character->getBS());
            fprintf(file, " S=\"%d\"", character->getStrength());
            fprintf(file, " A=\"%d\"", character->getAttacks());
            fprintf(file, " M=\"%d\"", character->getMind());
            fprintf(file, " D=\"%d\"", character->getDexterity());
            fprintf(file, " B=\"%d\"", character->getBravery());
            fprintf(file, " Sp=\"%f\"", character->getSpeed());*/
            for(map<string, int>::const_iterator iter = character->getBaseProfile()->intPropertiesBegin(); iter != character->getBaseProfile()->intPropertiesEnd(); ++iter) {
                string key = iter->first;
                int value = iter->second;
                fprintf(file, " %s=\"%d\"", key.c_str(), value);
            }
            for(map<string, float>::const_iterator iter = character->getBaseProfile()->floatPropertiesBegin(); iter != character->getBaseProfile()->floatPropertiesEnd(); ++iter) {
                string key = iter->first;
                float value = iter->second;
                fprintf(file, " %s=\"%f\"", key.c_str(), value);
            }
            int natural_damageX = 0, natural_damageY = 0, natural_damageZ = 0;
            character->getNaturalDamage(&natural_damageX, &natural_damageY, &natural_damageZ);

            fprintf(file, " natural_damageX=\"%d\"", natural_damageX);
            fprintf(file, " natural_damageY=\"%d\"", natural_damageY);
            fprintf(file, " natural_damageZ=\"%d\"", natural_damageZ);
            fprintf(file, " can_fly=\"%s\"", character->canFly() ? "true": "false");
            fprintf(file, " is_paralysed=\"%s\"", character->isParalysed() ? "true": "false");
            fprintf(file, " paralysed_time=\"%d\"", character->getParalysedUntil() - game_g->getScreen()->getGameTimeTotalMS());
            fprintf(file, " level=\"%d\"", character->getLevel());
            fprintf(file, " xp=\"%d\"", character->getXP());
            fprintf(file, " xp_worth=\"%d\"", character->getXPWorth());
            fprintf(file, " causes_terror=\"%s\"", character->getCausesTerror() ? "true" : "false");
            fprintf(file, " terror_effect=\"%d\"", character->getTerrorEffect());
            fprintf(file, " done_terror=\"%s\"", character->hasDoneTerror() ? "true" : "false");
            fprintf(file, " requires_magical=\"%s\"", character->requiresMagical() ? "true" : "false");
            fprintf(file, " gold=\"%d\"", character->getGold());
            fprintf(file, " can_talk=\"%s\"", character->canTalk() ? "true": "false");
            fprintf(file, " has_talked=\"%s\"", character->hasTalked() ? "true": "false");
            fprintf(file, " interaction_type=\"%s\"", character->getInteractionType().c_str());
            fprintf(file, " interaction_data=\"%s\"", character->getInteractionData().c_str());
            fprintf(file, " interaction_xp=\"%d\"", character->getInteractionXP());
            fprintf(file, " interaction_completed=\"%s\"", character->isInteractionCompleted() ? "true": "false");
            fprintf(file, " shop=\"%s\"", character->getShop().c_str());
            fprintf(file, ">\n");
            if( character->getTalkOpeningInitial().length() > 0 ) {
                fprintf(file, "<opening_initial>%s</opening_initial>", character->getTalkOpeningInitial().c_str());
            }
            if( character->getTalkOpeningLater().length() > 0 ) {
                fprintf(file, "<opening_later>%s</opening_later>", character->getTalkOpeningLater().c_str());
            }
            if( character->getTalkOpeningInteractionComplete().length() > 0 ) {
                fprintf(file, "<opening_interaction_complete>%s</opening_interaction_complete>", character->getTalkOpeningInteractionComplete().c_str());
            }
            for(vector<TalkItem>::const_iterator iter2 = character->talkItemsBegin(); iter2 != character->talkItemsEnd(); ++iter2) {
                const TalkItem *talk_item = &*iter2;
                fprintf(file, "<talk");
                fprintf(file, " question=\"%s\"", talk_item->question.c_str());
                fprintf(file, " action=\"%s\"", talk_item->action.c_str());
                fprintf(file, " while_not_done=\"%s\"", talk_item->while_not_done ? "true": "false");
                fprintf(file, " objective=\"%s\"", talk_item->objective ? "true": "false");
                fprintf(file, ">\n");
                fprintf(file, "%s\n", talk_item->answer.c_str());
                fprintf(file, "</talk>\n");
            }
            for(map<string, int>::const_iterator iter2 = character->spellsBegin(); iter2 != character->spellsEnd(); ++iter2) {
                string spell_name = iter2->first;
                int spell_count = iter2->second;
                fprintf(file, "<spell name=\"%s\" count=\"%d\"/>\n", spell_name.c_str(), spell_count);
            }
            for(set<Item *>::const_iterator iter2 = character->itemsBegin(); iter2 != character->itemsEnd(); ++iter2) {
                const Item *item = *iter2;
                this->saveItem(file, item, character);
            }
            if( player == character )
                fprintf(file, "</player>\n");
            else
                fprintf(file, "</npc>\n");
        }
        fprintf(file, "\n");

        if( location == c_location ) {
            qDebug("save player additional info");
            fprintf(file, "<player_start x=\"%f\" y=\"%f\"/>\n", player->getX(), player->getY());
            fprintf(file, "\n");
        }

        qDebug("save scenery");
        for(set<Scenery *>::const_iterator iter = location->scenerysBegin(); iter != location->scenerysEnd(); ++iter) {
            const Scenery *scenery = *iter;
            fprintf(file, "<scenery");
            fprintf(file, " name=\"%s\"", scenery->getName().c_str());
            fprintf(file, " image_name=\"%s\"", scenery->getImageName().c_str());
            fprintf(file, " x=\"%f\" y=\"%f\"", scenery->getX(), scenery->getY());
            fprintf(file, " w=\"%f\" h=\"%f\"", scenery->getWidth(), scenery->getHeight());
            fprintf(file, " opacity=\"%f\"", scenery->getOpacity());
            switch( scenery->getDrawType() ) {
            case Scenery::DRAWTYPE_NORMAL:
                break;
            case Scenery::DRAWTYPE_FLOATING:
                fprintf(file, " draw_type=\"floating\"");
                break;
            case Scenery::DRAWTYPE_BACKGROUND:
                fprintf(file, " draw_type=\"background\"");
                break;
            default:
                ASSERT_LOGGER(false);
                break;
            }
            fprintf(file, " action_last_time=\"%d\"", scenery->getActionLastTime());
            fprintf(file, " action_delay=\"%d\"", scenery->getActionDelay());
            fprintf(file, " action_type=\"%s\"", scenery->getActionType().c_str());
            fprintf(file, " action_value=\"%d\"", scenery->getActionValue());
            fprintf(file, " interact_type=\"%s\"", scenery->getInteractType().c_str());
            fprintf(file, " interact_state=\"%d\"", scenery->getInteractState());
            fprintf(file, " blocking=\"%s\"", scenery->isBlocking() ? "true": "false");
            fprintf(file, " block_visibility=\"%s\"", scenery->blocksVisibility() ? "true": "false");
            fprintf(file, " is_opened=\"%s\"", scenery->isOpened() ? "true": "false");
            fprintf(file, " exit=\"%s\"", scenery->isExit() ? "true": "false");
            fprintf(file, " door=\"%s\"", scenery->isDoor() ? "true": "false");
            fprintf(file, " exit_location=\"%s\" exit_location_x=\"%f\" exit_location_y=\"%f\"", scenery->getExitLocation().c_str(), scenery->getExitLocationPos().x, scenery->getExitLocationPos().y);
            fprintf(file, " locked=\"%s\"", scenery->isLocked() ? "true": "false");
            fprintf(file, " locked_silent=\"%s\"", scenery->isLockedSilent() ? "true": "false");
            fprintf(file, " locked_text=\"%s\"", scenery->getLockedText().c_str());
            fprintf(file, " locked_used_up=\"%s\"", scenery->isLockedUsedUp() ? "true": "false");
            fprintf(file, " unlocked_by_template=\"%s\"", scenery->getUnlockItemName().c_str());
            fprintf(file, " unlock_text=\"%s\"", scenery->getUnlockText().c_str());
            fprintf(file, " unlock_xp=\"%d\"", scenery->getUnlockXP());
            fprintf(file, " confirm_text=\"%s\"", scenery->getConfirmText().c_str());
            fprintf(file, ">");
            for(set<Item *>::const_iterator iter2 = scenery->itemsBegin(); iter2 != scenery->itemsEnd(); ++iter2) {
                const Item *item = *iter2;
                this->saveItem(file, item);
            }
            if( scenery->getTrap() != NULL ) {
                this->saveTrap(file, scenery->getTrap());
            }
            if( scenery->getDescription().length() > 0 ) {
                fprintf(file, "%s", scenery->getDescription().c_str());
            }
            fprintf(file, "</scenery>\n");
        }
        fprintf(file, "\n");

        qDebug("save items");
        for(set<Item *>::const_iterator iter = location->itemsBegin(); iter != location->itemsEnd(); ++iter) {
            const Item *item = *iter;
            this->saveItem(file, item);
        }
        fprintf(file, "\n");

        qDebug("save traps");
        for(set<Trap *>::const_iterator iter = location->trapsBegin(); iter != location->trapsEnd(); ++iter) {
            const Trap *trap = *iter;
            this->saveTrap(file, trap);
        }
        fprintf(file, "\n");

        fprintf(file, "</location>\n\n");
    }

    const QuestObjective *quest_objective = this->getQuest()->getQuestObjective();
    if( quest_objective != NULL ) {
        qDebug("save quest objective");
        fprintf(file, "<quest_objective");
        fprintf(file, " type=\"%s\"", quest_objective->getType().c_str());
        fprintf(file, " arg1=\"%s\"", quest_objective->getArg1().c_str());
        fprintf(file, " gold=\"%d\"", quest_objective->getGold());
        fprintf(file, " />");
    }
    fprintf(file, "\n");

    qDebug("save quest completed text");
    fprintf(file, "<completed_text>%s</completed_text>\n", this->getQuest()->getCompletedText().c_str());
    fprintf(file, "\n");

    qDebug("save current quest info");
    fprintf(file, "<quest_info complete=\"%s\"/>\n", this->getQuest()->isCompleted() ? "true" : "false");
    fprintf(file, "\n");

    qDebug("save journal");
    fprintf(file, "<journal>\n");
    QByteArray encoded = QUrl::toPercentEncoding(this->journal_ss.str().c_str(), QByteArray(), "<>");
    fprintf(file, "%s", encoded.data());
    fprintf(file, "</journal>\n");
    fprintf(file, "\n");

    fprintf(file, "</savegame>\n");

    fclose(file);

    game_g->getScreen()->getMainWindow()->unsetCursor();
    return true;
}

void PlayingGamestate::addWidget(QWidget *widget) {
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);
    this->view->setEnabled(false);
    this->view->releaseKeyboard();
}

void PlayingGamestate::addTextEffect(const string &text, Vector2D pos, int duration_ms) {
    this->addTextEffect(text, pos, duration_ms, 255, 255, 255);
}

void PlayingGamestate::addTextEffect(const string &text, Vector2D pos, int duration_ms, int r, int g, int b) {
    LOG("PlayingGamestate::addTextEffect(%s, (%f, %f), %d, %d, %d, %d)\n", text.c_str(), pos.x, pos.y, duration_ms, r, g, b);
    QColor qt_color(r, g, b);
    TextEffect *text_effect = new TextEffect(this->view, text.c_str(), duration_ms, qt_color);
    float font_scale = 1.2f/view->getScale();
    float text_effect_width = font_scale*text_effect->boundingRect().width();
    Vector2D text_effect_pos = pos;
    text_effect_pos.x -= 0.5*text_effect_width;
    text_effect_pos.y -= 0.5f;
    QPointF view_topleft = view->mapToScene(0, 0);
    QPointF view_topright = view->mapToScene(view->width()-1, 0);
    /*qDebug("view width: %d", view->width());
    qDebug("left: %f right: %f", view_topleft.x(), view_topright.x());
    qDebug("text_effect_pos.x: %f text_effect_width: %f", text_effect_pos.x, text_effect_width);*/
    if( text_effect_pos.x < view_topleft.x() ) {
        text_effect_pos.x = std::min(pos.x, (float)view_topleft.x());
    }
    else if( text_effect_pos.x + text_effect_width > view_topright.x() ) {
        text_effect_pos.x = std::max(pos.x, (float)view_topright.x()) - text_effect_width;
    }
    text_effect->setPos( text_effect_pos.x, text_effect_pos.y );
    text_effect->setScale(font_scale);
    //text_effect->setZValue(text_effect->pos().y() + 1.0f);
    text_effect->setZValue(text_effect->pos().y() + 1000.0f);
    view->addTextEffect(text_effect);
}

void PlayingGamestate::playSound(const string &sound_effect) {
/*#ifndef Q_OS_ANDROID
    qDebug("play sound: %s\n", sound_effect.c_str());
    if( game_g->isSoundEnabled() ) {
        Sound *sound = this->sound_effects[sound_effect];
        ASSERT_LOGGER(sound != NULL);
        if( sound != NULL ) {
            if( sound->state() == Phonon::PlayingState ) {
                qDebug("    already playing");
            }
            else {
                //sound->stop();
                sound->seek(0);
                sound->play();
            }
        }
    }
#endif*/
    game_g->playSound(sound_effect);
}

void PlayingGamestate::advanceQuest() {
    this->c_quest_indx++;
    ASSERT_LOGGER(this->c_quest_indx < this->quest_list.size());
}

void PlayingGamestate::addStandardItem(Item *item) {
    this->standard_items[item->getKey()] = item;
}

Item *PlayingGamestate::cloneStandardItem(const string &name) const {
    map<string, Item *>::const_iterator iter = this->standard_items.find(name);
    if( iter == this->standard_items.end() ) {
        LOG("can't clone standard item which doesn't exist: %s\n", name.c_str());
        throw string("Unknown standard item");
    }
    const Item *item = iter->second;
    return item->clone();
}

const Spell *PlayingGamestate::findSpell(const string &name) const {
    map<string, Spell *>::const_iterator iter = this->spells.find(name);
    if( iter == this->spells.end() ) {
        LOG("can't find spell: %s\n", name.c_str());
        throw string("Unknown spell");
    }
    const Spell *spell = iter->second;
    return spell;
}

Character *PlayingGamestate::createCharacter(const string &template_name) const {
    map<string, CharacterTemplate *>::const_iterator iter = this->character_templates.find(template_name);
    if( iter == this->character_templates.end() ) {
        LOG("can't find character_templates: %s\n", template_name.c_str());
        throw string("Unknown character_template");
    }
    const CharacterTemplate *character_template = iter->second;
    Character *character = new Character(template_name, true, *character_template);
    return character;
}

Currency *PlayingGamestate::cloneGoldItem(int value) const {
    Currency *item = static_cast<Currency *>(this->cloneStandardItem("Gold"));
    item->setValue(value);
    return item;
}

QPixmap &PlayingGamestate::getItemImage(const string &name) {
    map<string, QPixmap>::iterator image_iter = this->item_images.find(name);
    if( image_iter == this->item_images.end() ) {
        LOG("failed to find image for item: %s\n", name.c_str());
        LOG("    image name: %s\n", name.c_str());
        throw string("Failed to find item's image");
    }
    return image_iter->second;
}

void PlayingGamestate::showInfoDialog(const string &message) {
    this->showInfoDialog(message, "");
}

void PlayingGamestate::showInfoDialog(const string &message, const string &picture) {
    LOG("PlayingGamestate::showInfoDialog(%s)\n", message.c_str());
    InfoDialog *dialog = InfoDialog::createInfoDialogOkay(message, picture);
    this->addWidget(dialog);
    dialog->exec();
    this->closeSubWindow();
}

bool PlayingGamestate::askQuestionDialog(const string &message) {
    LOG("PlayingGamestate::askQuestionDialog(%s)\n", message.c_str());
    InfoDialog *dialog = InfoDialog::createInfoDialogYesNo(message);
    this->addWidget(dialog);
    int result = dialog->exec();
    LOG("askQuestionDialog returns %d\n", result);
    this->closeSubWindow();
    return result == 0;
}
