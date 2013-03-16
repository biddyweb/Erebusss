#include <QtWebKit/QWebView>
#include <QtWebKit/QWebFrame>

#include <ctime>
#include <algorithm> // needed for stable_sort on Symbian at least

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

const float z_value_tilemap = E_TOL_LINEAR;
const float z_value_scenery_background = 2.0f*E_TOL_LINEAR;
const float z_value_items = 3.0f*E_TOL_LINEAR; // so items appear above DRAWTYPE_BACKGROUND Scenery
const float z_value_gui = 4.0f*E_TOL_LINEAR; // so items appear above DRAWTYPE_BACKGROUND Scenery

const float MainGraphicsView::min_zoom_c = 10.0f;
const float MainGraphicsView::max_zoom_c = 200.0f;

PlayingGamestate *PlayingGamestate::playingGamestate = NULL;

Direction directionFromVecDir(Vector2D dir) {
    if( dir.magnitude() < E_TOL_LINEAR ) {
        return DIRECTION_E; // arbitrary
    }
    dir.normalise();
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
    return direction;
}

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

    this->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
}

CharacterAction::CharacterAction(Type type, Character *source, Character *target_npc, float offset_y) : type(type), source(source), target_npc(target_npc), time_ms(0), duration_ms(0), offset_y(offset_y), hits(false), weapon_no_effect_magical(false), weapon_no_effect_holy(false), weapon_damage(0), spell(NULL), object(NULL) {
    this->source_pos = source->getPos();
    this->dest_pos = target_npc->getPos();
    this->time_ms = game_g->getScreen()->getGameTimeTotalMS();
}

CharacterAction::~CharacterAction() {
    if( this->object != NULL ) {
        delete this->object;
    }
}

void CharacterAction::implement(PlayingGamestate *playing_gamestate) const {
    if( target_npc == NULL ) {
        // target no longer exists
        return;
    }
    if( type == CHARACTERACTION_RANGED_WEAPON ) {
        if( hits ) {
            Character::hitEnemy(playing_gamestate, source, target_npc, weapon_no_effect_magical, weapon_no_effect_holy, weapon_damage);
        }
    }
    else if( type == CHARACTERACTION_SPELL ) {
        ASSERT_LOGGER( spell != NULL );
        spell->castOn(playing_gamestate, source, target_npc);
    }
}

void CharacterAction::update() {
    if( this->target_npc != NULL ) {
        // update destination
        this->dest_pos = this->target_npc->getPos();
    }
    if( this->object != NULL ) {
        int diff_ms = game_g->getScreen()->getGameTimeTotalMS() - this->time_ms;
        float alpha = ((float)diff_ms) / (float)duration_ms;
        alpha = std::max(alpha, 0.0f);
        alpha = std::min(alpha, 1.0f);
        Vector2D curr_pos = this->source_pos * (1.0f-alpha) + this->dest_pos * alpha;
        curr_pos.y += offset_y;
        this->object->setPos(curr_pos.x, curr_pos.y);
        this->object->setZValue(object->pos().y() + 1000.0f);
    }
}

void CharacterAction::notifyDead(const Character *character) {
    if( character == this->source )
        this->source = NULL;
    if( character == this->target_npc )
        this->target_npc = NULL;
}

bool CharacterAction::isExpired() const {
    if( game_g->getScreen()->getGameTimeTotalMS() >= time_ms + duration_ms ) {
        return true;
    }
    return false;
}

CharacterAction *CharacterAction::createSpellAction(PlayingGamestate *playing_gamestate, Character *source, Character *target_npc, const Spell *spell) {
    CharacterAction *character_action = new CharacterAction(CHARACTERACTION_SPELL, source, target_npc, -0.75f);
    //character_action->duration_ms = 250;
    const float speed = 0.01f; // units per ms
    character_action->duration_ms = (int)((character_action->dest_pos - character_action->source_pos).magnitude() / speed);
    character_action->spell = spell;
    character_action->object = playing_gamestate->addSpellGraphic(source->getPos() + Vector2D(0.0f, character_action->offset_y));
    return character_action;
}

CharacterAction *CharacterAction::createProjectileAction(PlayingGamestate *playing_gamestate, Character *source, Character *target_npc, bool hits, bool weapon_no_effect_magical, bool weapon_no_effect_holy, int weapon_damage, const string &projectile_key, float icon_width) {
    CharacterAction *character_action = new CharacterAction(CHARACTERACTION_RANGED_WEAPON, source, target_npc, -0.75f);
    //character_action->duration_ms = 250;
    const float speed = 0.02f; // units per ms
    character_action->duration_ms = (int)((character_action->dest_pos - character_action->source_pos).magnitude() / speed);
    character_action->hits = hits;
    character_action->weapon_no_effect_magical = weapon_no_effect_magical;
    character_action->weapon_no_effect_holy = weapon_no_effect_holy;
    character_action->weapon_damage = weapon_damage;

    AnimatedObject *object = new AnimatedObject(100);
    character_action->object = object;
    object->addAnimationLayer( playing_gamestate->getProjectileAnimationLayer(projectile_key) );
    playing_gamestate->addGraphicsItem(object, icon_width, true);

    Vector2D dir = character_action->dest_pos - character_action->source_pos;
    Direction direction = directionFromVecDir(dir);
    object->setDimension(direction);

    character_action->update(); // set position, z-value
    return character_action;
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
                int time_ms = game_g->getScreen()->getInputTimeFrameMS();
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
    case Qt::Key_N:
        done = true;
        if( done ) {
            this->playing_gamestate->cycleTargetNPC();
        }
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
    if( game_g->getScreen()->isPaused() && event->key() != Qt::Key_Control && event->key() != Qt::Key_Alt ) {
        // Qt::Key_Control, Qt::Key_Alt produced when doing multitouch zoom in/out for some reason?!
        //qDebug("unpause");
        game_g->getScreen()->setPaused(false, false);
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
    if( !mobile_c )
    {
        if( fps_timer.isValid() ) {
            int time_ms = fps_timer.elapsed();
            if( time_ms > 0 ) {
                float fps = 1000.0f/(float)time_ms;
                this->gui_overlay->setFPS(fps);
            }
        }
        fps_timer.start();
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
        if( !this->calculated_lighting_pixmap_scaled && game_g->getScreen()->getGameTimeTotalMS() > lasttime_calculated_lighting_pixmap_scaled_ms + 1000 ) {
            this->lasttime_calculated_lighting_pixmap_scaled_ms = game_g->getScreen()->getGameTimeTotalMS();
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
        if( this->has_kinetic_scroll ) {
            int time_ms = game_g->getScreen()->getInputTimeFrameMS();
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
    QPointF view_centre = this->mapToScene( this->rect() ).boundingRect().center();

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
    QWidget(view), playing_gamestate(playing_gamestate), fps(-1.0f)
{
    //this->setAttribute(Qt::WA_NoSystemBackground);
}

void GUIOverlay::paintEvent(QPaintEvent *event) {
    //qDebug("GUIOverlay::paintEvent()");

    QPainter painter(this);
    painter.setFont( game_g->getFontScene() );
    if( playing_gamestate->getPlayer() != NULL ) {
        const float bar_x = 16.0f/640.0f;
        const float bar_y = 48.0f/360.0f;
        const float portrait_size = 160.0f/1080.f;
        float text_x = bar_x;
        const float text_y = bar_y - 4.0f/360.0f;
        const Character *player = playing_gamestate->getPlayer();
        if( player->getPortrait().length() > 0 ) {
            QPixmap pixmap = playing_gamestate->getPortraitImage(player->getPortrait());
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
    playing_gamestate->addWidget(this, true);

    const Character *player = playing_gamestate->getPlayer();

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    if( player->getPortrait().length() > 0 ) {
        QPixmap pixmap = playing_gamestate->getPortraitImage(player->getPortrait());
        QLabel *portrait_label = new QLabel();
        portrait_label->setPixmap(pixmap);
        layout->addWidget(portrait_label);
    }

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
    html += this->writeStat(profile_key_FP_c, false);
    html += this->writeStat(profile_key_BS_c, false);
    html += this->writeStat(profile_key_S_c, false);
    html += this->writeStat(profile_key_A_c, false);
    html += this->writeStat(profile_key_M_c, false);
    html += this->writeStat(profile_key_D_c, false);
    html += this->writeStat(profile_key_B_c, false);
    html += this->writeStat(profile_key_Sp_c, true);

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
    if( player->isParalysed() ) {
        html += "<font color=\"#ff0000\">PARALYSED</font><br/>";
    }
    if( player->isDiseased() ) {
        html += "<font color=\"#ff0000\">DISEASED</font><br/>";
    }

    {
        stringstream str;
        int damageX = 0, damageY = 0, damageZ = 0;
        if( player->getCurrentWeapon() == NULL ) {
            player->getNaturalDamage(&damageX, &damageY, &damageZ);
            str << "<b>Current Weapon:</b> None ";
        }
        else {
            const Weapon *weapon = player->getCurrentWeapon();
            weapon->getDamage(&damageX, &damageY, &damageZ);
            str << "<b>Current Weapon:</b> " << weapon->getName() << " ";
        }
        if( damageZ != 0 ) {
            char sign = damageZ > 0 ? '+' : '-';
            str << "<b>Damage:</b> " << damageX << "D" << damageY << sign << abs(damageZ) << "<br/>";
        }
        else {
            str << "<b>Damage:</b> " << damageX << "D" << damageY << "<br/>";
        }
        html += str.str().c_str();
    }
    html += "<b>Armour Rating:</b> " + QString::number(player->getArmourRating(true, true)) + "<br/>";

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

QString StatsWindow::writeStat(const string &stat_key, bool is_float) const {
    string visual_name = getLongString(stat_key);
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
    playing_gamestate(playing_gamestate), list(NULL), weightLabel(NULL),
    armButton(NULL), wearButton(NULL), useButton(NULL), dropButton(NULL), infoButton(NULL),
    view_type(VIEWTYPE_ALL)
{
    playing_gamestate->addWidget(this, true);

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
        connect(wearButton, SIGNAL(clicked()), this, SLOT(clickedWear()));

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
    vector<Item *> sorted_list;
    for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
        Item *item = *iter;
        sorted_list.push_back(item);
    }
    std::stable_sort(sorted_list.begin(), sorted_list.end(), ItemCompare());
    for(vector<Item *>::iterator iter = sorted_list.begin(); iter != sorted_list.end(); ++iter) {
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
        else if( view_type == VIEWTYPE_MISC && !( ( item->getType() == ITEMTYPE_GENERAL || item->getType() == ITEMTYPE_RING ) && !item->isMagical() ) ) {
            continue;
        }
        QString item_str = playing_gamestate->getItemString(item, true);
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
    else if( item->getType() == ITEMTYPE_AMMO ) {
        if( playing_gamestate->getPlayer()->getCurrentAmmo() == item ) {
            armButton->setVisible(false);
        }
        else {
            armButton->setVisible(true);
            armButton->setText("Select Ammo");
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

    if( item->getType() == ITEMTYPE_ARMOUR ) {
        wearButton->setVisible(true);
        if( playing_gamestate->getPlayer()->getCurrentArmour() == item ) {
            wearButton->setText("Take Off Armour");
        }
        else {
            wearButton->setText("Wear Armour");
        }
    }
    else if( item->getType() == ITEMTYPE_RING ) {
        wearButton->setVisible(true);
        if( playing_gamestate->getPlayer()->getCurrentRing() == item ) {
            wearButton->setText("Take Off Ring");
        }
        else {
            wearButton->setText("Wear Ring");
        }
    }
    else {
        wearButton->setVisible(false);
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
    else if( item->getType() == ITEMTYPE_AMMO ) {
        Ammo *ammo = static_cast<Ammo *>(item);
        if( playing_gamestate->getPlayer()->getCurrentAmmo() != ammo ) {
            LOG("player selected ammo: %s\n", item->getName().c_str());
            playing_gamestate->getPlayer()->selectAmmo(ammo);
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
        item_widget->setText( playing_gamestate->getItemString(item, true) );
    }
    this->changedSelectedItem(index);
}

void ItemsWindow::clickedWear() {
    LOG("clickedWearArmour()\n");
    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    Item *item = list_items.at(index);
    if( item->getType() == ITEMTYPE_ARMOUR ) {
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
    }
    else if( item->getType() == ITEMTYPE_RING ) {
        Ring *ring = static_cast<Ring *>(item);
        if( playing_gamestate->getPlayer()->getCurrentRing() == ring ) {
            // take off instead
            LOG("player took off ring");
            playing_gamestate->getPlayer()->wearRing(NULL);
        }
        else {
            LOG("player put on ring: %s\n", item->getName().c_str());
            playing_gamestate->getPlayer()->wearRing(ring);
        }
    }
    else {
        LOG("not an armour or ring?!\n");
    }

    /*QListWidgetItem *item_widget = list->item(index);
    item_widget->setText( this->getItemString(item) );*/
    for(size_t i=0;i<list_items.size();i++) {
        const Item *item = list_items.at(i);
        QListWidgetItem *item_widget = list->item(i);
        item_widget->setText( playing_gamestate->getItemString(item, true) );
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
    playing_gamestate->addWidget(this, true);

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
        vector<Item *> sorted_list;
        for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
            Item *item = *iter;
            sorted_list.push_back(item);
        }
        std::stable_sort(sorted_list.begin(), sorted_list.end(), ItemCompare());
        for(vector<Item *>::iterator iter = sorted_list.begin(); iter != sorted_list.end(); ++iter) {
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
    list->setCurrentRow(0);
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
    //QString item_str = QString(item->getName().c_str()) + QString(" (") + QString::number(buy_cost) + QString(" gold)");
    QString item_str = playing_gamestate->getItemString(item, false) + QString(" (") + QString::number(buy_cost) + QString(" gold)");
    QListWidgetItem *list_item = new QListWidgetItem(item_str);
    if( buy_cost == 0 ) {
        list_item->setTextColor(Qt::gray);
        list_item->setFlags( list_item->flags() & ~Qt::ItemIsSelectable );
    }
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
    /*else {
        // no longer needed, as items that can't be bought are grayed out
        //game_g->showInfoDialog("Trade", "This shop doesn't buy that item.");
        playing_gamestate->showInfoDialog("This shop doesn't buy that item.");
    }*/
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
    playing_gamestate->addWidget(this, false);

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
    vector<Item *> sorted_list;
    for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
        Item *item = *iter;
        sorted_list.push_back(item);
    }
    std::stable_sort(sorted_list.begin(), sorted_list.end(), ItemCompare());
    for(vector<Item *>::iterator iter = sorted_list.begin(); iter != sorted_list.end(); ++iter) {
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

const int n_level_up_stats_c = 2;

LevelUpWindow::LevelUpWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate), closeButton(NULL) {
    playing_gamestate->addWidget(this, false);

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    Character *player = playing_gamestate->getPlayer();

    QString text = "You have advanced to level " + QString::number(player->getLevel()) + " (" + QString::number(player->getXP()) + " XP)";
    text += "\n\n Select " + QString::number(n_level_up_stats_c) + " statistics that you wish to improve:";
    QLabel *label = new QLabel(text);
    //label->setFont(game_g->getFontSmall());
    label->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    label->setWordWrap(true);
    //label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(label);

    {
        QGridLayout *g_layout = new QGridLayout();
        layout->addLayout(g_layout);

        int row = 0;
        g_layout->addWidget( addProfileCheckBox(profile_key_FP_c), row++, 0 );
        // mobile platforms may be too small to fit the labels (e.g., Symbian 640x360)
        if( !mobile_c ) {
            g_layout->addWidget( new QLabel("Hand-to-hand combat"), row++, 0 );
        }
        g_layout->addWidget( addProfileCheckBox(profile_key_BS_c), row++, 0 );
        if( !mobile_c ) {
            g_layout->addWidget( new QLabel("Ranged combat: bows and thrown weapons"), row++, 0 );
        }
        g_layout->addWidget( addProfileCheckBox(profile_key_S_c), row++, 0 );
        if( !mobile_c ) {
            g_layout->addWidget( new QLabel("How strong you are"), row++, 0 );
        }

        row = 0;
        g_layout->addWidget( addProfileCheckBox(profile_key_M_c), row++, 1 );
        if( !mobile_c ) {
            g_layout->addWidget( new QLabel("Your mental and psychic abilities"), row++, 1 );
        }
        g_layout->addWidget( addProfileCheckBox(profile_key_D_c), row++, 1 );
        if( !mobile_c ) {
            g_layout->addWidget( new QLabel("Useful for avoiding traps"), row++, 1 );
        }
        g_layout->addWidget( addProfileCheckBox(profile_key_B_c), row++, 1 );
        if( !mobile_c ) {
            g_layout->addWidget( new QLabel("Courage against terrifying enemies"), row++, 1 );
        }
    }

    int initial_level = player->getInitialLevel();
    /*map<string, bool> is_enabled; // needed as we can't seem to check with isEnabled() at this stage (perhaps because GUI isn't yet created?)
    for(map<string, QCheckBox *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
        string key = (*iter).first;
        is_enabled[key] = true;
    }*/
    if( initial_level != 0 ) {
        // see which can be improved
        int n_levels = player->getLevel() - initial_level;
        qDebug("player has advanced %d levels to level %d", n_levels, player->getLevel());
        int max_stat_inc = (n_levels+1)/2;
        max_stat_inc++;
        qDebug("max_stat_inc = %d", max_stat_inc);
        for(map<string, QCheckBox *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
            string key = (*iter).first;
            int initial_val = player->getInitialBaseProfile()->getIntProperty(key);
            int val = player->getBaseProfileIntProperty(key);
            qDebug("### check stat: %s %d vs %d", key.c_str(), val, initial_val);
            if( val - initial_val >= max_stat_inc ) {
                // already increased to max
                (*iter).second->setEnabled(false);
                //is_enabled[key] = false;
            }
        }

        // check we haven't disabled too many!
        int n_enabled = 0;
        for(map<string, QCheckBox *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
            //string key = (*iter).first;
            //if( is_enabled[key] ) {
            if( (*iter).second->isEnabled() ) {
                n_enabled++;
            }
        }
        ASSERT_LOGGER(n_enabled >= n_level_up_stats_c);
        if( n_enabled < n_level_up_stats_c ) {
            // runtime workaround
            LOG("error, not enough available level up stats: %d vs %d\n", n_enabled, n_level_up_stats_c);
            for(map<string, QCheckBox *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
                string key = (*iter).first;
                (*iter).second->setEnabled(true);
                //is_enabled[key] = true;
            }
        }
    }

    // select some defaults - the min and max
    {
        int max_val = -1, min_val = -1;
        string min_key, max_key;
        for(map<string, QCheckBox *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
            string key = (*iter).first;
            //qDebug("### check stat: %s", key.c_str());
            //qDebug("enabled? %d", is_enabled[key]);
            //if( !is_enabled[key] ) {
            if( !(*iter).second->isEnabled() ) {
                continue;
            }
            int val = player->getBaseProfileIntProperty(key);
            if( min_val == -1 || val < min_val ) {
                min_val = val;
                min_key = key;
            }
            if( max_val == -1 || val > max_val ) {
                max_val = val;
                max_key = key;
            }
        }
        if( min_val == max_val ) {
            // all the same, set some defaults
            int count = 0;
            for(map<string, QCheckBox *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
                string key = (*iter).first;
                if( !(*iter).second->isEnabled() ) {
                    continue;
                }
                check_boxes[key]->setChecked(true);
                selected.push_back(check_boxes[key]);
                count++;
                if( count == n_level_up_stats_c ) {
                    break;
                }
            }
        }
        else {
            check_boxes[max_key]->setChecked(true);
            selected.push_back(check_boxes[max_key]);
            check_boxes[min_key]->setChecked(true);
            selected.push_back(check_boxes[min_key]);
        }
    }

    for(map<string, QCheckBox *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
        QCheckBox *check_box = (*iter).second;
        connect(check_box, SIGNAL(stateChanged(int)), this, SLOT(clickedCheckBox(int)));
    }

    closeButton = new QPushButton("Level Up!");
    game_g->initButton(closeButton);
    closeButton->setShortcut(QKeySequence(Qt::Key_Return));
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(clickedLevelUp()));
}

QCheckBox *LevelUpWindow::addProfileCheckBox(const string &key) {
    string long_string = getLongString(key);
    QCheckBox * check_box = new QCheckBox(long_string.c_str());
    check_boxes[key] = check_box;
    return check_box;
}

void LevelUpWindow::clickedCheckBox(int state) {
    qDebug("LevelUpWindow::clickedCheckBox(%d)", state);
    QObject *sender = this->sender();
    ASSERT_LOGGER( sender != NULL );
    // count how many now selected (includes this one)
    int count = 0;
    for(map<string, QCheckBox *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
        QCheckBox *check_box = (*iter).second;
        if( check_box->checkState() == Qt::Checked ) {
            count++;
        }
    }
    qDebug("    count = %d", count);
    if( state == Qt::Unchecked ) {
        // still have to check count, as this function is called when we uncheck a state (because there are too many), rather than the user unchecking
        ASSERT_LOGGER( count <= n_level_up_stats_c );
        if( count < n_level_up_stats_c ) {
            this->closeButton->setEnabled(false);
        }
        // remove from the selected list
        for(vector<QCheckBox *>::iterator iter = selected.begin(); iter != selected.end(); ++iter) {
            QCheckBox *check_box = *iter;
            if( sender == check_box ) {
                selected.erase(iter);
                break;
            }
        }
    }
    else if( state == Qt::Checked ) {
        ASSERT_LOGGER( count > 0 );
        ASSERT_LOGGER( count <= n_level_up_stats_c+1 );
        selected.push_back( static_cast<QCheckBox *>(sender) );
        if( count >= n_level_up_stats_c ) {
            // can re-enable
            this->closeButton->setEnabled(true);
        }
        if( count > n_level_up_stats_c ) {
            // need to unselect one!
            ASSERT_LOGGER( selected.size() >= n_level_up_stats_c+1 );
            QCheckBox *check_box = selected.at( selected.size() - n_level_up_stats_c - 1 );
            ASSERT_LOGGER( check_box->checkState() == Qt::Checked );
            check_box->setCheckState(Qt::Unchecked);
        }
    }
}

void LevelUpWindow::clickedLevelUp() {
    playing_gamestate->closeSubWindow();
    Character *player = playing_gamestate->getPlayer();

    int count = 0;
    for(map<string, QCheckBox *>::iterator iter = check_boxes.begin(); iter != check_boxes.end(); ++iter) {
        QCheckBox *check_box = (*iter).second;
        if( check_box->checkState() == Qt::Checked ) {
            count++;
            string key = (*iter).first;
            player->changeBaseProfileIntProperty(key, 1);
            if( count == n_level_up_stats_c ) {
                // no more allowed
                break;
            }
        }
    }

    stringstream xp_str;
    xp_str << "Advanced to level " << player->getLevel() << "!";
    playing_gamestate->addTextEffect(xp_str.str(), player->getPos(), 2000, 255, 255, 0);
}

CampaignWindow::CampaignWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate)
{
    playing_gamestate->addWidget(this, true);

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
            QString qt_filename = DEPLOYMENT_PATH + QString(c_quest_info.getFilename().c_str());
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
    if( playing_gamestate->isPermadeath() ) {
        if( playing_gamestate->hasPermadeathSavefilename() ) {
            QString filename = playing_gamestate->getPermadeathSavefilename();
            LOG("permadeath save mode: save as existing file: %s\n", filename.toUtf8().data());
            if( playing_gamestate->saveGame(filename, true) ) {
                //playing_gamestate->showInfoDialog("The game has been successfully saved.");
                playing_gamestate->addTextEffect("The game has been successfully saved", 5000);
            }
            else {
                game_g->showErrorDialog("Failed to save game!");
            }
        }
        else {
            this->requestNewSaveGame();
        }
        return;
    }

    playing_gamestate->addWidget(this, false);

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
    save_filenames.push_back(""); // dummy entry

    /*QDir dir( QString(game_g->getApplicationFilename(savegame_folder).c_str()) );
    QStringList filter;
    filter << "*" + QString( savegame_ext.c_str() );
    QStringList files = dir.entryList(filter);
    for(int i=0;i<files.size();i++) {
        const QString &file = files.at(i);
        list->addItem(file);
    }*/
    game_g->fillSaveGameFiles(&list, &save_filenames);
    ASSERT_LOGGER(list->count() == save_filenames.size());

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

void SaveGameWindow::requestNewSaveGame() {
    QString filename;
    /*if( !playing_gamestate->isPermadeath() ) {
        filename = QString(savegame_root.c_str());
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
    }*/

    QWidget *subwindow = new QWidget();
    playing_gamestate->addWidget(subwindow, false);

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

void SaveGameWindow::clickedSave() {
    LOG("SaveGameWindow::clickedSave()\n");

    int index = list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(list->count() == save_filenames.size());
    ASSERT_LOGGER(index >= 0 && index < list->count());
    ASSERT_LOGGER(index >= 0 && index < save_filenames.size());
    if( index == 0 ) {
        this->requestNewSaveGame();
    }
    else {
        QString filename = save_filenames.at(index);
        LOG("save as existing file: %s\n", filename.toUtf8().data());
        if( playing_gamestate->saveGame(filename, false) ) {
            //game_g->showInfoDialog("Saved Game", "The game has been successfully saved.");
            //playing_gamestate->showInfoDialog("The game has been successfully saved.");
            //playing_gamestate->addTextEffect("The game has been successfully saved", playing_gamestate->getPlayer()->getPos(), 5000);
            playing_gamestate->addTextEffect("The game has been successfully saved", 5000);
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
    ASSERT_LOGGER(list->count() == save_filenames.size());
    ASSERT_LOGGER(index >= 0 && index < list->count());
    ASSERT_LOGGER(index >= 0 && index < save_filenames.size());
    if( index >= 1 ) {
        QString filename = save_filenames.at(index);
        //if( game_g->askQuestionDialog("Delete Save Game", "Are you sure you wish to delete save game: " + filename.toStdString() + "?") ) {
        if( playing_gamestate->askQuestionDialog("Are you sure you wish to delete save game: " + filename.toStdString() + "?") ) {
            LOG("delete existing file: %s\n", filename.toUtf8().data());
            QString full_path = game_g->getApplicationFilename(savegame_folder + filename);
            LOG("full path: %s\n", full_path.toUtf8().data());
            //remove(full_path.c_str());
            QFile::remove(full_path);
            QListWidgetItem *list_item = list->takeItem(index);
            delete list_item;
            save_filenames.erase(save_filenames.begin() + index);
            ASSERT_LOGGER(list->count() == save_filenames.size());
        }
    }
}

void SaveGameWindow::clickedSaveNew() {
    LOG("SaveGameWindow::clickedSaveNew()\n");
    ASSERT_LOGGER(this->edit != NULL);
    QString filename = this->edit->text();

    if( filename.length() == 0 ) {
        playing_gamestate->showInfoDialog("Please enter a filename.");
    }
    else {
        filename += savegame_ext;
        //LOG("save as new file: %s\n", filename.toStdString().c_str());
        LOG("save as new file: %s\n", filename.toUtf8().data());
        QString full_path = game_g->getApplicationFilename(savegame_folder + filename);
        QFile qfile( full_path );
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
            if( playing_gamestate->saveGame(full_path, true) ) {
                //game_g->showInfoDialog("Saved Game", "The game has been successfully saved.");
                //playing_gamestate->showInfoDialog("The game has been successfully saved.");
                //playing_gamestate->addTextEffect("The game has been successfully saved", playing_gamestate->getPlayer()->getPos(), 5000);
            }
            else {
                game_g->showErrorDialog("Failed to save game!");
            }
        }
        playing_gamestate->closeAllSubWindows();
    }
}

PlayingGamestate::PlayingGamestate(bool is_savegame, size_t player_type, const string &player_name, bool permadeath, bool cheat_mode, int cheat_start_level) :
    scene(NULL), view(NULL), gui_overlay(NULL),
    view_transform_3d(false), view_walls_3d(false),
    /*main_stacked_widget(NULL),*/ quickSaveButton(NULL),
    difficulty(DIFFICULTY_MEDIUM), permadeath(permadeath), permadeath_has_savefilename(false), player(NULL), time_hours(1), c_quest_indx(0), c_location(NULL), quest(NULL),
    target_animation_layer(NULL), target_item(NULL),
    time_last_complex_update_ms(0),
    cheat_mode(cheat_mode)
{
    LOG("PlayingGamestate::PlayingGamestate()\n");
    playingGamestate = this;

    srand( clock() );

    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->setEnabled(false);
    game_g->getScreen()->setPaused(true, true);

    {
#if defined(Q_OS_SYMBIAN)
        const int res_c = 16;
#else
        const int res_c = 64;
#endif
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
    {
#if defined(Q_OS_SYMBIAN)
        const int res_c = 16;
#else
        const int res_c = 64;
#endif
        QPixmap pixmap(res_c, res_c);
        pixmap.fill(Qt::transparent);
        QRadialGradient radialGrad((res_c-1)/2, (res_c-1)/2, (res_c-1)/2);
        radialGrad.setColorAt(0.0, QColor(255, 127, 0, 255));
        radialGrad.setColorAt(1.0, QColor(255, 127, 0, 0));
        QPainter painter(&pixmap);
        painter.setPen(Qt::NoPen);
        painter.fillRect(0, 0, res_c, res_c, radialGrad);
        painter.end();
        this->fireball_pixmap = pixmap;
    }
    {
#if defined(Q_OS_SYMBIAN)
        const int res_c = 16;
#else
        const int res_c = 64;
#endif
        /*QPixmap pixmap(res_c, res_c);
        pixmap.fill(Qt::transparent);
        QRadialGradient radialGrad((res_c-1)/2, (res_c-1)/2, (res_c-1)/2);
        radialGrad.setColorAt(0.0, QColor(255, 0, 0, 0));
        radialGrad.setColorAt(0.7, QColor(255, 0, 0, 0));
        radialGrad.setColorAt(0.75, QColor(255, 0, 0, 160));
        radialGrad.setColorAt(0.95, QColor(255, 0, 0, 160));
        radialGrad.setColorAt(1.0, QColor(255, 0, 0, 0));
        QPainter painter(&pixmap);
        painter.setPen(Qt::NoPen);
        painter.fillRect(0, 0, res_c, res_c, radialGrad);
        painter.end();
        this->target_pixmap = pixmap;*/
        int n_frames = 4;
        QPixmap full_pixmap(n_frames*res_c, res_c);
        full_pixmap.fill(Qt::transparent);
        QPainter painter(&full_pixmap);
        for(int i=0;i<n_frames;i++) {
            QRadialGradient radialGrad(i*res_c+(res_c-1)/2, (res_c-1)/2, (res_c-1)/2);
            float alpha = n_frames==1 ? 1.0f : i/(float)(n_frames-1);
            float size = (1.0f-alpha)*0.6f + alpha*1.0f;
            radialGrad.setColorAt(0.0*size, QColor(255, 0, 0, 0));
            radialGrad.setColorAt(0.7*size, QColor(255, 0, 0, 0));
            radialGrad.setColorAt(0.75*size, QColor(255, 0, 0, 160));
            radialGrad.setColorAt(0.95*size, QColor(255, 0, 0, 160));
            radialGrad.setColorAt(1.0*size, QColor(255, 0, 0, 0));
            QBrush brush(radialGrad);
            painter.fillRect(i*res_c, 0, res_c, res_c, brush);
        }
        this->target_pixmap = full_pixmap;
        vector<AnimationLayerDefinition> target_animation_layer_definition;
        target_animation_layer_definition.push_back( AnimationLayerDefinition("", 0, n_frames, AnimationSet::ANIMATIONTYPE_BOUNCE) );
        this->target_animation_layer = AnimationLayer::create(this->target_pixmap, target_animation_layer_definition, true, 0, 0, res_c, res_c, res_c, res_c, n_frames*res_c, 1);
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
    //view->setOptimizationFlag(QGraphicsView::DontSavePainterState);

    /*QWidget *centralWidget = new QWidget(window);
    this->mainwindow = centralWidget;
    LOG("mainwindow: %d\n", mainwindow);
    centralWidget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    window->setCentralWidget(centralWidget);*/
    /*if( mobile_c ) {
        this->main_stacked_widget = new QStackedWidget();
        main_stacked_widget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
        window->setCentralWidget(main_stacked_widget);
    }*/

    QWidget *centralWidget = new QWidget();
    /*if( mobile_c ) {
        main_stacked_widget->addWidget(centralWidget);
    }
    else*/ {
        window->setCentralWidget(centralWidget);
    }
    this->widget_stack.push_back(centralWidget);

    QHBoxLayout *layout = new QHBoxLayout();
    centralWidget->setLayout(layout);

    {
        QVBoxLayout *v_layout = new QVBoxLayout();
        layout->addLayout(v_layout);

        QPushButton *statsButton = new QPushButton("Stats");
        game_g->initButton(statsButton);
        statsButton->setShortcut(QKeySequence(Qt::Key_F1));
        statsButton->setToolTip("Display statistics of your character (F1)");
        statsButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        connect(statsButton, SIGNAL(clicked()), this, SLOT(clickedStats()));
        v_layout->addWidget(statsButton);

        QPushButton *itemsButton = new QPushButton("Items");
        game_g->initButton(itemsButton);
        itemsButton->setShortcut(QKeySequence(Qt::Key_F2));
        itemsButton->setToolTip("Display the items that you are carrying (F2)");
        itemsButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        connect(itemsButton, SIGNAL(clicked()), this, SLOT(clickedItems()));
        v_layout->addWidget(itemsButton);

        /*QPushButton *spellsButton = new QPushButton("Spells");
        game_g->initButton(spellsButton);
        spellsButton->setToolTip("Not supported yet");
        spellsButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        v_layout->addWidget(spellsButton);*/

        QPushButton *journalButton = new QPushButton("Journal");
        game_g->initButton(journalButton);
        journalButton->setShortcut(QKeySequence(Qt::Key_F3));
        journalButton->setToolTip("Displays information about your quests (F3)");
        journalButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        connect(journalButton, SIGNAL(clicked()), this, SLOT(clickedJournal()));
        v_layout->addWidget(journalButton);

        /*QPushButton *quitButton = new QPushButton("Quit");
        game_g->initButton(quitButton);
        quitButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        v_layout->addWidget(quitButton);
        connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));*/

        QPushButton *pauseButton = new QPushButton("Pause");
        game_g->initButton(pauseButton);
        pauseButton->setShortcut(QKeySequence(Qt::Key_P));
        pauseButton->setToolTip("Pause the game (P)");
        pauseButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        connect(pauseButton, SIGNAL(clicked()), game_g->getScreen(), SLOT(togglePaused()));
        v_layout->addWidget(pauseButton);

        QPushButton *restButton = new QPushButton("Rest");
        game_g->initButton(restButton);
        restButton->setShortcut(QKeySequence(Qt::Key_R));
        restButton->setToolTip("Rest until you are healed (R)");
        restButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        connect(restButton, SIGNAL(clicked()), this, SLOT(clickedRest()));
        v_layout->addWidget(restButton);

        QPushButton *optionsButton = new QPushButton("Options");
        game_g->initButton(optionsButton);
        optionsButton->setShortcut(QKeySequence(Qt::Key_Escape));
        optionsButton->setToolTip("Options to save game or quit");
        optionsButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        connect(optionsButton, SIGNAL(clicked()), this, SLOT(clickedOptions()));
        v_layout->addWidget(optionsButton);
    }

    layout->addWidget(view);
    /*{
        QVBoxLayout *v_layout = new QVBoxLayout();
        layout->addLayout(v_layout);

        v_layout->addWidget(view);

        {
            QHBoxLayout *h_layout = new QHBoxLayout();
            v_layout->addLayout(h_layout);

            if( !this->permadeath ) {
                quickSaveButton = new QPushButton("QS");
                game_g->initButton(quickSaveButton);
                quickSaveButton->setShortcut(QKeySequence(Qt::Key_F5));
#ifndef Q_OS_ANDROID
                // for some reason, this sometimes shows on Android when it shouldn't?
                quickSaveButton->setToolTip("Quick-save");
#endif
                quickSaveButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                connect(quickSaveButton, SIGNAL(clicked()), this, SLOT(quickSave()));
                h_layout->addWidget(quickSaveButton);
            }

            QPushButton *zoomoutButton = new QPushButton("-");
            game_g->initButton(zoomoutButton);
            zoomoutButton->setShortcut(QKeySequence(Qt::Key_Less));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            zoomoutButton->setToolTip("Zoom out");
#endif
            connect(zoomoutButton, SIGNAL(clicked()), view, SLOT(zoomOut()));
            h_layout->addWidget(zoomoutButton);

            QPushButton *zoominButton = new QPushButton("+");
            game_g->initButton(zoominButton);
            zoominButton->setShortcut(QKeySequence(Qt::Key_Greater));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            zoominButton->setToolTip("Zoom in");
#endif
            connect(zoominButton, SIGNAL(clicked()), view, SLOT(zoomIn()));
            h_layout->addWidget(zoominButton);

            QPushButton *centreButton = new QPushButton("O");
            game_g->initButton(centreButton);
            centreButton->setShortcut(QKeySequence(Qt::Key_C));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            centreButton->setToolTip("Centre view on your player's location");
#endif
            centreButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            connect(centreButton, SIGNAL(clicked()), view, SLOT(centreOnPlayer()));
            h_layout->addWidget(centreButton);
        }
    }*/

    {
        QGridLayout *layout = new QGridLayout();
        view->setLayout(layout);

        layout->setColumnStretch(0, 1);
        layout->setRowStretch(1, 1);
        int col = 1;

        if( !this->permadeath ) {
            quickSaveButton = new QPushButton("QS");
            game_g->initButton(quickSaveButton);
            quickSaveButton->setShortcut(QKeySequence(Qt::Key_F5));
#ifndef Q_OS_ANDROID
            // for some reason, this sometimes shows on Android when it shouldn't?
            quickSaveButton->setToolTip("Quick-save");
#endif
            quickSaveButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            connect(quickSaveButton, SIGNAL(clicked()), this, SLOT(quickSave()));
            layout->addWidget(quickSaveButton, 0, col++, Qt::AlignCenter);
        }

        QPushButton *zoomoutButton = new QPushButton("-");
        game_g->initButton(zoomoutButton);
        zoomoutButton->setShortcut(QKeySequence(Qt::Key_Less));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        zoomoutButton->setToolTip("Zoom out");
#endif
        zoomoutButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        connect(zoomoutButton, SIGNAL(clicked()), view, SLOT(zoomOut()));
        layout->addWidget(zoomoutButton, 0, col++, Qt::AlignCenter);

        QPushButton *zoominButton = new QPushButton("+");
        game_g->initButton(zoominButton);
        zoominButton->setShortcut(QKeySequence(Qt::Key_Greater));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        zoominButton->setToolTip("Zoom in");
#endif
        zoominButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        connect(zoominButton, SIGNAL(clicked()), view, SLOT(zoomIn()));
        layout->addWidget(zoominButton, 0, col++, Qt::AlignCenter);

        QPushButton *centreButton = new QPushButton("O");
        game_g->initButton(centreButton);
        centreButton->setShortcut(QKeySequence(Qt::Key_C));
#ifndef Q_OS_ANDROID
        // for some reason, this sometimes shows on Android when it shouldn't?
        centreButton->setToolTip("Centre view on your player's location");
#endif
        centreButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        connect(centreButton, SIGNAL(clicked()), view, SLOT(centreOnPlayer()));
        layout->addWidget(centreButton, 0, col++, Qt::AlignCenter);

        //v_layout->addStretch();
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
        */
        this->player = game_g->createPlayer(player_type, player_name);
    }

    LOG("load images\n");
    {
        QString filename = QString(DEPLOYMENT_PATH) + "data/images.xml";
        QFile file(filename);
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            qDebug("failed to open: %s", filename.toUtf8().data());
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
                    int stride_x = 0, stride_y = 0;
                    const int def_width_c = 128, def_height_c = 128;
                    int n_dimensions = 0;
                    QStringRef n_dimensions_s = reader.attributes().value("n_dimensions");
                    if( n_dimensions_s.length() > 0 ) {
                        n_dimensions = parseInt(n_dimensions_s.toString());
                    }
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

                            QStringRef stride_x_s = reader.attributes().value("stride_x");
                            if( stride_x_s.length() > 0 ) {
                                stride_x = parseInt(stride_x_s.toString());
                            }
                            else {
                                // set a default in case needed for animation
                                stride_x = def_width_c;
                            }
                            QStringRef stride_y_s = reader.attributes().value("stride_y");
                            if( stride_y_s.length() > 0 ) {
                                stride_y = parseInt(stride_y_s.toString());
                            }
                            else {
                                // set a default in case needed for animation
                                stride_y = def_height_c;
                            }
                        }
                        else {
                            // set up defaults in case needed for animation
                            if( expected_width_s.length() > 0 ) {
                                expected_width = parseInt(expected_width_s.toString());
                            }
                            else {
                                expected_width = def_width_c;
                            }

                            width = def_width_c;
                            height = def_height_c;
                            stride_x = def_width_c;
                            stride_y = def_height_c;
                        }
                        // image loaded later, lazily
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

                    if( animation_layer_definition.size() > 0 && filename.length() == 0 ) {
                        throw string("animations can only load from filenames");
                    }
                    if( animation_layer_definition.size() > 0 && !clip ) {
                        // animations must have clip data specified - we'll use the defaults set above
                        clip = true;
                    }

                    if( filename.length() > 0 ) {
                        filename = DEPLOYMENT_PATH + filename;
                    }

                    if( type == "generic") {
                        if( animation_layer_definition.size() > 0 ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("animations not supported for this animation type");
                        }
                        if( filename.length() > 0 )
                            pixmap = game_g->loadImage(filename.toStdString(), clip, xpos, ypos, width, height, expected_width);
                        this->builtin_images[name.toStdString()] = pixmap;
                    }
                    else if( type == "item") {
                        if( animation_layer_definition.size() > 0 ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("animations not supported for this animation type");
                        }
                        if( filename.length() > 0 )
                            pixmap = game_g->loadImage(filename.toStdString(), clip, xpos, ypos, width, height, expected_width);
                        this->item_images[name.toStdString()] = pixmap;
                    }
                    else if( type == "projectile") {
                        if( animation_layer_definition.size() > 0 ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("animations not supported for this animation type");
                        }
                        if( n_dimensions == 0 ) {
                            n_dimensions = 8;
                        }
                        animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 1, AnimationSet::ANIMATIONTYPE_SINGLE) );
                        if( filename.length() > 0 )
                            this->projectile_animation_layers[name.toStdString()] = AnimationLayer::create(filename.toStdString(), animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, n_dimensions);
                        else
                            this->projectile_animation_layers[name.toStdString()] = AnimationLayer::create(pixmap, animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, n_dimensions);
                    }
                    else if( type == "scenery" ) {
                        if( animation_layer_definition.size() == 0 ) {
                            animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 1, AnimationSet::ANIMATIONTYPE_SINGLE) );
                        }
                        if( filename.length() > 0 )
                            this->scenery_animation_layers[name.toStdString()] = new LazyAnimationLayer(filename.toStdString(), animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, 1);
                        else
                            this->scenery_animation_layers[name.toStdString()] = new LazyAnimationLayer(pixmap, animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, 1);
                    }
                    else if( type == "npc" ) {
                        unsigned int n_dimensions = animation_layer_definition.size() > 0 ? N_DIRECTIONS : 1;
                        if( animation_layer_definition.size() == 0 ) {
                            animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 1, AnimationSet::ANIMATIONTYPE_SINGLE) );
                        }
                        if( filename.length() > 0 )
                            this->animation_layers[name.toStdString()] = new LazyAnimationLayer(filename.toStdString(), animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, n_dimensions);
                        else
                            this->animation_layers[name.toStdString()] = new LazyAnimationLayer(pixmap, animation_layer_definition, clip, xpos, ypos, width, height, stride_x, stride_y, expected_width, n_dimensions);
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

    this->portrait_images["portrait_barbarian"] = game_g->loadImage(string(DEPLOYMENT_PATH) + "gfx/portraits/barbarian_m0.png");
    this->portrait_images["portrait_elf"] = game_g->loadImage(string(DEPLOYMENT_PATH) + "gfx/portraits/elf_f0.png");
    this->portrait_images["portrait_halfling"] = game_g->loadImage(string(DEPLOYMENT_PATH) + "gfx/portraits/halfling_f0.png");
    this->portrait_images["portrait_ranger"] = game_g->loadImage(string(DEPLOYMENT_PATH) + "gfx/portraits/ranger_m0.png");
    this->portrait_images["portrait_warrior"] = game_g->loadImage(string(DEPLOYMENT_PATH) + "gfx/portraits/warrior_m0.png");

    /*{
        // force all lazily loaded data to be loaded
        for(map<string, LazyAnimationLayer *>::iterator iter = this->animation_layers.begin(); iter != this->animation_layers.end(); ++iter) {
            LazyAnimationLayer *animation_layer = (*iter).second;
            animation_layer->getAnimationLayer();
        }
        for(map<string, LazyAnimationLayer *>::iterator iter = this->scenery_animation_layers.begin(); iter != this->scenery_animation_layers.end(); ++iter) {
            LazyAnimationLayer *animation_layer = (*iter).second;
            animation_layer->getAnimationLayer();
        }
    }*/

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
                    QStringRef causes_disease_s = reader.attributes().value("causes_disease");
                    int causes_disease = parseInt(causes_disease_s.toString(), true);
                    QStringRef causes_paralysis_s = reader.attributes().value("causes_paralysis");
                    int causes_paralysis = parseInt(causes_paralysis_s.toString(), true);
                    QStringRef requires_magical_s = reader.attributes().value("requires_magical");
                    bool requires_magical = parseBool(requires_magical_s.toString(), true);
                    QStringRef unholy_s = reader.attributes().value("unholy");
                    bool unholy = parseBool(unholy_s.toString(), true);
                    CharacterTemplate *character_template = new CharacterTemplate(animation_name_s.toString().toStdString(), FP, BS, S, A, M, D, B, Sp, health_min, health_max, gold_min, gold_max, xp_worth, causes_terror, terror_effect, causes_disease, causes_paralysis);
                    character_template->setStaticImage(static_image);
                    character_template->setBounce(bounce);
                    character_template->setRequiresMagical(requires_magical);
                    character_template->setUnholy(unholy);
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

    LOG("load Spells\n");
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
                    QStringRef effect_s = reader.attributes().value("effect");
                    QStringRef rollX_s = reader.attributes().value("rollX");
                    QStringRef rollY_s = reader.attributes().value("rollY");
                    QStringRef rollZ_s = reader.attributes().value("rollZ");
                    QStringRef damage_armour_s = reader.attributes().value("damage_armour");
                    QStringRef damage_shield_s = reader.attributes().value("damage_shield");
                    QStringRef mind_test_s = reader.attributes().value("mind_test");
                    int rollX = parseInt(rollX_s.toString(), true);
                    int rollY = parseInt(rollY_s.toString(), true);
                    int rollZ = parseInt(rollZ_s.toString(), true);
                    bool damage_armour = parseBool(damage_armour_s.toString(), true);
                    bool damage_shield = parseBool(damage_shield_s.toString(), true);
                    bool mind_test = parseBool(mind_test_s.toString(), true);
                    Spell *spell = new Spell(name_s.toString().toStdString(), type_s.toString().toStdString(), effect_s.toString().toStdString());
                    spell->setRoll(rollX, rollY, rollZ);
                    spell->setDamage(damage_armour, damage_shield);
                    spell->setMindTest(mind_test);
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
    // if we change the offsets, remember to update the hotspot in PlayingGamestate::locationAddCharacter !
    const int off_x = 32, off_y = 40, width = 64, height = 64;
    //const int off_x = 0, off_y = 0, width = 128, height = 128;
    const int expected_stride_x = 128, expected_stride_y = 128, expected_total_width = 4096;
    //float off_x = 32.0f/128.0f, off_y = 40.0f/128.0f, width = 64.0f/128.0f, height = 64.0f/128.0f;
    //float off_x = 0.0f/128.0f, off_y = 0.0f/128.0f, width = 128.0f/128.0f, height = 128.0f/128.0f;
    player_animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 4, AnimationSet::ANIMATIONTYPE_BOUNCE) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("run", 4, 8, AnimationSet::ANIMATIONTYPE_LOOP) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("attack", 12, 4, AnimationSet::ANIMATIONTYPE_SINGLE) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("ranged", 28, 4, AnimationSet::ANIMATIONTYPE_SINGLE) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("death", 18, 6, AnimationSet::ANIMATIONTYPE_SINGLE) );
    //this->animation_layers["player"] = AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/player.png", player_animation_layer_definition, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS);
    //this->animation_layers["longsword"] = AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/longsword.png", player_animation_layer_definition, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS);
    //this->animation_layers["longbow"] = AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/longbow.png", player_animation_layer_definition, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS);
    //this->animation_layers["shield"] = AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/shield.png", player_animation_layer_definition, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS);
    this->animation_layers["player"] = new LazyAnimationLayer(AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/player.png", player_animation_layer_definition, true, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS));
    this->animation_layers["longsword"] = new LazyAnimationLayer(AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/longsword.png", player_animation_layer_definition, true, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS));
    this->animation_layers["longbow"] = new LazyAnimationLayer(AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/longbow.png", player_animation_layer_definition, true, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS));
    this->animation_layers["shield"] = new LazyAnimationLayer(AnimationLayer::create(string(DEPLOYMENT_PATH) + "gfx/textures/isometric_hero/shield.png", player_animation_layer_definition, true, off_x, off_y, width, height, expected_stride_x, expected_stride_y, expected_total_width, N_DIRECTIONS));

    gui_overlay->setProgress(70);
    qApp->processEvents();

    //if( !mobile_c )
    if( game_g->isSoundEnabled() )
    {
        LOG("load sound effects\n");
        game_g->loadSound("click", string(DEPLOYMENT_PATH) + "sound/click_short.wav");
        game_g->loadSound("coin", string(DEPLOYMENT_PATH) + "sound/coin.wav");
        game_g->loadSound("container", string(DEPLOYMENT_PATH) + "sound/container.wav");
        game_g->loadSound("door", string(DEPLOYMENT_PATH) + "sound/door.wav");
        game_g->loadSound("drink", string(DEPLOYMENT_PATH) + "sound/bubble2.wav");
        game_g->loadSound("lock", string(DEPLOYMENT_PATH) + "sound/lock.wav");
        game_g->loadSound("turn_page", string(DEPLOYMENT_PATH) + "sound/turn_page.wav");
        game_g->loadSound("weapon_unsheath", string(DEPLOYMENT_PATH) + "sound/sword-unsheathe5.wav");
        game_g->loadSound("wear_armour", string(DEPLOYMENT_PATH) + "sound/chainmail1.wav");
#if !defined(Q_OS_SYMBIAN) && !defined(Q_WS_SIMULATOR)
        game_g->loadSound("swing", string(DEPLOYMENT_PATH) + "sound/swing2.wav");  // playing this sample causes strange pauses on Symbian?? (Nokia 5800)
#endif
#if !defined(Q_OS_SYMBIAN) && !defined(Q_WS_SIMULATOR) && !defined(Q_OS_ANDROID)
        game_g->loadSound("footsteps", string(DEPLOYMENT_PATH) + "sound/stepdirt_1.wav"); // strange pauses on Symbian?; conflicts with other sounds on Android
#endif
        // remember to call freeSound in the PlayingGamestate destructor!
        LOG("done loading sound effects\n");
    }
    else {
        LOG("sound is disabled\n");
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
    game_g->getScreen()->setPaused(false, true);

    if( !is_savegame ) {
        //this->player->initialiseHealth(600); // CHEAT
        //player->addGold( 1000 ); // CHEAT
        //player->addXP(this, 96); // CHEAT
    }
    if( !is_savegame && this->cheat_mode ) {
        this->c_quest_indx = cheat_start_level % this->quest_list.size();
        if( this->c_quest_indx == 1 ) {
            // CHEAT, simulate start of quest 2:
            player->addGold( 166 );
            player->deleteItem("Leather Armour");
            player->addItem(this->cloneStandardItem("Long Sword"), true);
            player->addItem(this->cloneStandardItem("Shield"), true);
            player->addItem(this->cloneStandardItem("Chain Mail Armour"), true);
            player->addItem(this->cloneStandardItem("Longbow"), true);
            player->addItem(this->cloneStandardItem("Arrows"), true);
            player->addItem(this->cloneStandardItem("Arrows"), true);
            player->addItem(this->cloneStandardItem("Arrows"), true);
            player->setXP(70);
        }
        else if( this->c_quest_indx == 2 ) {
            // CHEAT, simulate start of quest 3:
            for(int i=0;i<114;i++) {
                player->addXP(this, 10);
            }
            player->addGold( 1541 );
            player->deleteItem("Leather Armour");
            player->deleteItem("Long Sword");
            player->armWeapon(NULL);
            player->addItem(this->cloneStandardItem("Two Handed Sword"), true);
            player->addItem(this->cloneStandardItem("Plate Armour"), true);
            player->addItem(this->cloneStandardItem("Longbow"), true);
            player->addItem(this->cloneStandardItem("Arrows"), true);
            player->addItem(this->cloneStandardItem("Arrows"), true);
            player->addItem(this->cloneStandardItem("Arrows"), true);
            {
                Item *item = this->cloneStandardItem("Long Sword");
                item->setName("Magical Long Sword");
                item->setMagical(true);
                item->setBaseTemplate("Long Sword");
                player->addItem(item, true);
            }
        }
    }
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

    for(vector<CharacterAction *>::iterator iter = this->character_actions.begin(); iter != this->character_actions.end(); ++iter) {
        CharacterAction *character_action = *iter;
        delete character_action;
    }
    if( this->target_animation_layer != NULL ) {
        delete this->target_animation_layer;
    }
    /*for(map<string, AnimationLayer *>::iterator iter = this->animation_layers.begin(); iter != this->animation_layers.end(); ++iter) {
        AnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }*/
    for(map<string, LazyAnimationLayer *>::iterator iter = this->animation_layers.begin(); iter != this->animation_layers.end(); ++iter) {
        LazyAnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }
    /*for(map<string, AnimationLayer *>::iterator iter = this->scenery_animation_layers.begin(); iter != this->scenery_animation_layers.end(); ++iter) {
        AnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }*/
    for(map<string, LazyAnimationLayer *>::iterator iter = this->scenery_animation_layers.begin(); iter != this->scenery_animation_layers.end(); ++iter) {
        LazyAnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }
    for(map<string, AnimationLayer *>::iterator iter = this->projectile_animation_layers.begin(); iter != this->projectile_animation_layers.end(); ++iter) {
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
        game_g->freeSound("turn_page");
        game_g->freeSound("weapon_unsheath");
        game_g->freeSound("wear_armour");
        game_g->freeSound("swing");
        game_g->freeSound("footsteps");
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
        factor = 0.85f;
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

void PlayingGamestate::quickSave() {
    qDebug("quickSave()");
    if( !this->permadeath ) {
        if( this->canSaveHere() ) {
            this->showInfoDialog("You cannot save here - enemies are nearby.");
            return;
        }
        if( this->saveGame("quicksave.xml", false) )  {
            //this->showInfoDialog("The game has been successfully saved.");
            //this->addTextEffect("The game has been successfully saved", this->getPlayer()->getPos(), 5000);
            this->addTextEffect("The game has been successfully saved", 5000);
        }
        else {
            game_g->showErrorDialog("Failed to save game!");
        }
    }
}

void PlayingGamestate::parseXMLItemProfileAttributeInt(Item *item, const QXmlStreamReader &reader, const string &key) const {
    string attribute = "bonus_" + key;
    QStringRef attribute_sr = reader.attributes().value(attribute.c_str());
    if( attribute_sr.length() > 0 ) {
        int value = parseInt(attribute_sr.toString());
        item->setProfileBonusIntProperty(key, value);
    }
}

void PlayingGamestate::parseXMLItemProfileAttributeFloat(Item *item, const QXmlStreamReader &reader, const string &key) const {
    string attribute = "bonus_" + key;
    QStringRef attribute_sr = reader.attributes().value(attribute.c_str());
    if( attribute_sr.length() > 0 ) {
        float value = parseFloat(attribute_sr.toString());
        item->setProfileBonusFloatProperty(key, value);
    }
}

Item *PlayingGamestate::parseXMLItem(QXmlStreamReader &reader) const {
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
    QStringRef name_s = reader.attributes().value("name");
    QStringRef image_name_s = reader.attributes().value("image_name");
    QStringRef icon_width_s = reader.attributes().value("icon_width");

    if( reader.name() == "item" ) {
        QStringRef use_s = reader.attributes().value("use");
        QStringRef use_verb_s = reader.attributes().value("use_verb");
        item = new Item(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight);
        if( use_s.length() > 0 ) {
            item->setUse(use_s.toString().toStdString(), use_verb_s.toString().toStdString());
        }
    }
    else if( reader.name() == "weapon" ) {
        //qDebug("    weapon:");
        QStringRef animation_name_s = reader.attributes().value("animation_name");
        QStringRef two_handed_s = reader.attributes().value("two_handed");
        QStringRef ranged_s = reader.attributes().value("ranged");
        QStringRef thrown_s = reader.attributes().value("thrown");
        QStringRef ammo_s = reader.attributes().value("ammo");
        QStringRef damageX_s = reader.attributes().value("damageX");
        QStringRef damageY_s = reader.attributes().value("damageY");
        QStringRef damageZ_s = reader.attributes().value("damageZ");
        QStringRef min_strength_s = reader.attributes().value("min_strength");
        QStringRef unholy_only_s = reader.attributes().value("unholy_only");
        /*qDebug("    name: %s", name_s.toString().toStdString().c_str());
        qDebug("    image_name: %s", image_name_s.toString().toStdString().c_str());
        qDebug("    animation_name: %s", animation_name_s.toString().toStdString().c_str());
        qDebug("    weight: %s", weight_s.toString().toStdString().c_str());
        qDebug("    two_handed_s: %s", two_handed_s.toString().toStdString().c_str());
        qDebug("    ranged_s: %s", ranged_s.toString().toStdString().c_str());
        qDebug("    ammo_s: %s", ammo_s.toString().toStdString().c_str());*/
        bool two_handed = parseBool(two_handed_s.toString(), true);
        bool ranged = parseBool(ranged_s.toString(), true);
        bool thrown = parseBool(thrown_s.toString(), true);
        int damageX = parseInt(damageX_s.toString());
        int damageY = parseInt(damageY_s.toString());
        int damageZ = parseInt(damageZ_s.toString());
        int min_strength = parseInt(min_strength_s.toString(), true);
        bool unholy_only = parseBool(unholy_only_s.toString(), true);
        Weapon *weapon = new Weapon(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, animation_name_s.toString().toStdString(), damageX, damageY, damageZ);
        item = weapon;
        weapon->setTwoHanded(two_handed);
        if( ranged && thrown ) {
            throw string("Weapons can be ranged and thrown");
        }
        else if( ranged ) {
            weapon->setWeaponType(Weapon::WEAPONTYPE_RANGED);
        }
        else if( thrown ) {
            weapon->setWeaponType(Weapon::WEAPONTYPE_THROWN);
        }
        else {
            weapon->setWeaponType(Weapon::WEAPONTYPE_HAND);
        }
        //weapon->setRanged(ranged);
        if( ammo_s.length() > 0 ) {
            weapon->setRequiresAmmo(true, ammo_s.toString().toStdString());
        }
        weapon->setMinStrength(min_strength);
        weapon->setUnholyOnly(unholy_only);
    }
    else if( reader.name() == "shield" ) {
        QStringRef animation_name_s = reader.attributes().value("animation_name");
        Shield *shield = new Shield(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, animation_name_s.toString().toStdString());
        item = shield;
    }
    else if( reader.name() == "armour" ) {
        //qDebug("    armour:");
        QStringRef min_strength_s = reader.attributes().value("min_strength");
        int min_strength = parseInt(min_strength_s.toString(), true);
        Armour *armour = new Armour(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, rating);
        item = armour;
        armour->setMinStrength(min_strength);
    }
    else if( reader.name() == "ring" ) {
        //qDebug("    ring:");
        Ring *ring = new Ring(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight);
        item = ring;
    }
    else if( reader.name() == "ammo" ) {
        QStringRef ammo_type_s = reader.attributes().value("ammo_type");
        QStringRef projectile_image_name_s = reader.attributes().value("projectile_image_name");
        QStringRef amount_s = reader.attributes().value("amount");
        int amount = parseInt(amount_s.toString());
        QString ammo_type = ammo_type_s.toString();
        if( ammo_type.length() == 0 ) {
            ammo_type = name_s.toString(); // needed for backwards compatibility
        }
        Ammo *ammo = new Ammo(name_s.toString().toStdString(), image_name_s.toString().toStdString(), ammo_type.toStdString(), projectile_image_name_s.toString().toStdString(), weight, amount);
        item = ammo;
    }
    else if( reader.name() == "currency" ) {
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
        if( icon_width_s.length() > 0 ) {
            float icon_width = parseFloat(icon_width_s.toString());
            //LOG("icon_width: %f\n", icon_width);
            item->setIconWidth(icon_width);
        }
        item->setArg1(arg1);
        item->setArg2(arg2);
        item->setArg1s(arg1_s.toStdString());
        item->setRating(rating);
        item->setMagical(magical);
        item->setBaseTemplate(base_template.toStdString());
        item->setWorthBonus(worth_bonus);

        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_FP_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_BS_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_S_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_A_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_M_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_D_c);
        this->parseXMLItemProfileAttributeInt(item, reader, profile_key_B_c);
        this->parseXMLItemProfileAttributeFloat(item, reader, profile_key_Sp_c);

        // must be done last
        QString description = reader.readElementText(QXmlStreamReader::IncludeChildElements);
        item->setDescription(description.toStdString());
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
    // reset NPCs to their default positions
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        if( character != player && character->hasDefaultPosition() ) {
            //qDebug("### move %s from %f, %f to %f, %f", character->getName().c_str(), character->getX(), character->getY(), character->getDefaultX(), character->getDefaultY());
            character->setPos(character->getDefaultX(), character->getDefaultY());
        }
    }

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
        float desired_width_c = mobile_c ? 10.0f : 20.0f;
        if( this->view_transform_3d ) {
            desired_width_c /= 1.5f;
        }
        float initial_scale = window->width() / desired_width_c;
        LOG("width: %d\n", window->width());
        LOG("initial_scale: %f\n", initial_scale);
        view->setScale(initial_scale);
    }

    QBrush floor_brush(builtin_images[c_location->getFloorImageName()]);
    QBrush wall_brush;
    QBrush dropwall_brush;
    //const float wall_brush_ratio = 1.0f;
    float wall_brush_ratio = 1.0f;
    const float dropwall_brush_ratio = 3.0f;
    float wall_scale = 1.0f;
    float dropwall_scale = 1.0f;
    //{
        /*int pixels_per_unit = 128;
        float scale = 4.0f/(float)pixels_per_unit;*/
        float floor_scale = 4.0f/(float)builtin_images[c_location->getFloorImageName()].width();
        floor_brush.setTransform(QTransform::fromScale(floor_scale, floor_scale));
        if( c_location->getWallImageName().length() > 0 ) {
            wall_brush_ratio = c_location->getWallXScale();
            wall_brush.setTexture(builtin_images[c_location->getWallImageName()]);
            wall_scale = 0.9f/(float)builtin_images[c_location->getWallImageName()].height();
            wall_brush.setTransform(QTransform::fromScale(wall_brush_ratio*wall_scale, wall_scale));
        }
        if( c_location->getDropWallImageName().length() > 0 ) {
            dropwall_brush.setTexture(builtin_images[c_location->getDropWallImageName()]);
            dropwall_scale = 0.9f/(float)builtin_images[c_location->getDropWallImageName()].height();
            dropwall_brush.setTransform(QTransform::fromScale(dropwall_brush_ratio*dropwall_scale, dropwall_scale));
        }

        float background_scale = 1.0f/32.0f;
        QBrush background_brush(builtin_images[c_location->getBackgroundImageName()]);
        background_brush.setTransform(QTransform::fromScale(background_scale, background_scale));
        view->setBackgroundBrush(background_brush);
        //view->setBackgroundBrush(QBrush(Qt::white));
    //}

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

            if( c_location->getWallImageName().length() > 0 ) {
                if( !this->view_walls_3d || normal_into_wall.y > -E_TOL_LINEAR ) {
                    QPolygonF wall_polygon;
                    const float wall_dist = 0.1f;
                    wall_polygon.push_back(QPointF(p0.x, p0.y));
                    wall_polygon.push_back(QPointF(p0.x + wall_dist * normal_into_wall.x, p0.y + wall_dist * normal_into_wall.y));
                    wall_polygon.push_back(QPointF(p1.x + wall_dist * normal_into_wall.x, p1.y + wall_dist * normal_into_wall.y));
                    wall_polygon.push_back(QPointF(p1.x, p1.y));
                    QGraphicsPolygonItem *wall_item = new QGraphicsPolygonItem(wall_polygon, item);
                    wall_item->setPen(Qt::NoPen);
                    wall_item->setBrush(wall_brush);
                }

                if( this->view_walls_3d ) {
                    const float wall_height = this->view_transform_3d ? 0.9f : 0.5f;
                    if( fabs(normal_into_wall.y) > E_TOL_LINEAR ) {
                        QPolygonF wall_polygon_3d;
                        if( normal_into_wall.y < 0.0f )
                        {
                            QBrush wall_brush_3d = wall_brush;
                            QTransform transform;
                            transform.translate(0.0f, (p1.y - wall_height)/wall_scale - p1.x/wall_scale * (p1.y - p0.y) / (p1.x - p0.x));
                            transform.shear(0.0f, wall_brush_ratio*(p1.y - p0.y) / (p1.x - p0.x));
                            transform *= wall_brush_3d.transform();
                            wall_brush_3d.setTransform(transform);
                            wall_polygon_3d.push_back(QPointF(p0.x, p0.y));
                            wall_polygon_3d.push_back(QPointF(p0.x, p0.y - wall_height));
                            wall_polygon_3d.push_back(QPointF(p1.x, p1.y - wall_height));
                            wall_polygon_3d.push_back(QPointF(p1.x, p1.y));
                            QGraphicsPolygonItem *wall_item_3d = new QGraphicsPolygonItem(wall_polygon_3d, item);
                            wall_item_3d->setPen(Qt::NoPen);
                            wall_item_3d->setBrush(wall_brush_3d);
                            /*if( normal_into_wall.y > 0.0f ) {
                                wall_item_3d->setZValue(1000000.0);
                                wall_item_3d->setFlag(QGraphicsItem::ItemStacksBehindParent);
                            }*/
                        }
                        /*else {
                            //wall_polygon_3d.push_back(QPointF(p0.x, p0.y));
                            //wall_polygon_3d.push_back(QPointF(p0.x, p0.y + wall_height));
                            //wall_polygon_3d.push_back(QPointF(p1.x, p1.y + wall_height));
                            //wall_polygon_3d.push_back(QPointF(p1.x, p1.y));
                        }*/
                    }
                    /*if( normal_into_wall.y < E_TOL_LINEAR ) {
                        QPolygonF wall_polygon_top;
                        const float wall_dist = 0.1f;
                        wall_polygon_top.push_back(QPointF(p0.x, p0.y - wall_height));
                        wall_polygon_top.push_back(QPointF(p0.x + wall_dist * normal_into_wall.x, p0.y - wall_height + wall_dist * normal_into_wall.y));
                        wall_polygon_top.push_back(QPointF(p1.x + wall_dist * normal_into_wall.x, p1.y - wall_height + wall_dist * normal_into_wall.y));
                        wall_polygon_top.push_back(QPointF(p1.x, p1.y - wall_height));
                        QGraphicsPolygonItem *wall_item_top = new QGraphicsPolygonItem(wall_polygon_top, item);
                        wall_item_top->setPen(Qt::NoPen);
                        wall_item_top->setBrush(wall_brush);
                    }*/

                }
            }
            if( c_location->getDropWallImageName().length() > 0 && view_walls_3d ) {
                const float dropwall_height = 0.9f;
                if( normal_into_wall.y > E_TOL_LINEAR ) {
                    QPolygonF dropwall_polygon_3d;
                    QBrush dropwall_brush_3d = dropwall_brush;
                    QTransform transform;
                    transform.translate(0.0f, (p1.y - dropwall_height)/dropwall_scale - p1.x/dropwall_scale * (p1.y - p0.y) / (p1.x - p0.x));
                    transform.shear(0.0f, dropwall_brush_ratio*(p1.y - p0.y) / (p1.x - p0.x));
                    transform *= dropwall_brush_3d.transform();
                    dropwall_brush_3d.setTransform(transform);
                    dropwall_polygon_3d.push_back(QPointF(p0.x, p0.y + dropwall_height));
                    dropwall_polygon_3d.push_back(QPointF(p0.x, p0.y));
                    dropwall_polygon_3d.push_back(QPointF(p1.x, p1.y));
                    dropwall_polygon_3d.push_back(QPointF(p1.x, p1.y + dropwall_height));
                    QGraphicsPolygonItem *dropwall_item_3d = new QGraphicsPolygonItem(dropwall_polygon_3d, item);
                    dropwall_item_3d->setPen(Qt::NoPen);
                    dropwall_item_3d->setBrush(dropwall_brush_3d);
                }
            }
        }
    }

    LOG("add graphics for tilemaps");
    for(size_t i=0;i<c_location->getNTilemaps();i++) {
        const Tilemap *tilemap = c_location->getTilemap(i);
        const string &imagemap = tilemap->getImagemap();
        QPixmap image = builtin_images[imagemap];
        int image_w = image.width();
        int image_h = image.height();
        int tile_width = tilemap->getTileWidth();
        int tile_height = tilemap->getTileHeight();
        qDebug("Tilemap: image: %s w %d h %d tile w %d tile h %d", imagemap.c_str(), image_w, image_h, tile_width, tile_height);
        int n_x = image_w / tile_width;
        int n_y = image_h / tile_height;
        for(int y=0;y<tilemap->getHeighti();y++) {
            for(int x=0;x<tilemap->getWidthi();x++) {
                char ch = tilemap->getTileAt(x, y);
                if( ( ch >= '0' && ch <= '9' ) || ( ch >= 'a' && ch <= 'z' ) ) {
                    int value = 0;
                    int ch_i = (int)ch;
                    if( ch >= '0' && ch <= '9' ) {
                        value = ch_i - (int)'0';
                    }
                    else {
                        value = 10 + ch_i - (int)'a';
                    }
                    if( value >= n_x*n_y ) {
                        LOG("tilemap at %d, %d has value %d, out of bounds for %d x %d\n", x, y, value, n_x, n_y);
                        throw string("tilemap value out of bounds for imagemap");
                    }
                    int pos_x = value % n_x;
                    int pos_y = value / n_x;
                    QPixmap tile = image.copy(pos_x*tile_width, pos_y*tile_height, tile_width, tile_height);
                    //QPixmap tile = image;
                    //qDebug("### %d, %d :")
                    QGraphicsPixmapItem *item = new QGraphicsPixmapItem(tile);
                    item->setPos(tilemap->getX() + x, tilemap->getY() + y);
                    item->setZValue(z_value_tilemap);
                    float item_scale = 1.0f / item->boundingRect().width();
                    item->setScale(item_scale);
                    scene->addItem(item);
                    //this->addGraphicsItem(item, 1.0f);
                }
            }
        }
    }

#ifdef _DEBUG
    this->debug_items.clear(); // clear any from previous view, as no longer valid
    this->refreshDebugItems();
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
    this->testFogOfWar();

    /*gui_overlay->unsetProgress();
    qApp->processEvents();
    for(int i=0;i<10000;i++) {
        LOG("blah\n");
    }*/
    LOG("done\n");

}

#ifdef _DEBUG
void PlayingGamestate::refreshDebugItems() {
    qDebug("PlayingGamestate::refreshDebugItems()");
    for(vector<QGraphicsItem *>::iterator iter = this->debug_items.begin(); iter != this->debug_items.end(); ++iter) {
        QGraphicsItem *item = *iter;
        delete item;
    }
    this->debug_items.clear();

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
                QGraphicsItem *item = scene->addLine(p0.x, p0.y, p1.x, p1.y, wall_pen);
                this->debug_items.push_back(item);
            }
        }
    }
    {
        // DEBUG ONLY
        QPen wall_pen(Qt::yellow);
        const Graph *distance_graph = c_location->getDistanceGraph();
        for(size_t i=0;i<distance_graph->getNVertices();i++) {
            const GraphVertex *vertex = distance_graph->getVertex(i);
            Vector2D path_way_point = vertex->getPos();
            const float radius = 0.05f;
            QGraphicsItem *item = scene->addEllipse(path_way_point.x - radius, path_way_point.y - radius, 2.0f*radius, 2.0f*radius, wall_pen);
            this->debug_items.push_back(item);
            // n.b., draws edges twice, but doesn't matter for debug purposes...
            for(size_t j=0;j<vertex->getNNeighbours();j++) {
                const GraphVertex *o_vertex = vertex->getNeighbour(distance_graph, j);
                Vector2D o_path_way_point = o_vertex->getPos();
                float x1 = path_way_point.x;
                float y1 = path_way_point.y;
                float x2 = o_path_way_point.x;
                float y2 = o_path_way_point.y;
                item = scene->addLine(x1, y1, x2, y2, wall_pen);
                this->debug_items.push_back(item);
            }
        }
    }
#endif
}
#endif

Character *PlayingGamestate::loadNPC(bool *is_player, Vector2D *pos, const QXmlStreamReader &reader) const {
    Character *npc = NULL;
    *is_player = false;
    pos->set(0.0f, 0.0f);

    QStringRef name_s = reader.attributes().value("name");
    QStringRef template_s = reader.attributes().value("template");
    QStringRef pos_x_s = reader.attributes().value("x");
    float pos_x = parseFloat(pos_x_s.toString());
    QStringRef pos_y_s = reader.attributes().value("y");
    float pos_y = parseFloat(pos_y_s.toString());
    pos->set(pos_x, pos_y);

    QStringRef default_pos_x_s = reader.attributes().value("default_x");
    QStringRef default_pos_y_s = reader.attributes().value("default_y");
    if( template_s.length() > 0 ) {
        if( reader.name() == "player" ) {
            LOG("error at line %d\n", reader.lineNumber());
            throw string("didn't expect player element to load by template");
        }
        // load from template
        npc = this->createCharacter(name_s.toString().toStdString(), template_s.toString().toStdString());
        QStringRef is_hostile_s = reader.attributes().value("is_hostile");
        bool is_hostile = is_hostile_s.length() == 0 ? true : parseBool(is_hostile_s.toString());
        npc->setHostile(is_hostile);
    }
    else {
        if( reader.name() == "player" ) {
            qDebug("player: %s", name_s.toString().toStdString().c_str());
            npc = new Character(name_s.toString().toStdString(), "", false);
            *is_player = true;
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
        QStringRef portrait_s = reader.attributes().value("portrait");
        if( portrait_s.length() > 0 ) {
            npc->setPortrait(portrait_s.toString().toStdString());
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

        QStringRef level_s = reader.attributes().value("level");
        int level = parseInt(level_s.toString(), true);
        npc->setLevel(level);
        QStringRef xp_s = reader.attributes().value("xp");
        int xp = parseInt(xp_s.toString(), true);
        npc->setXP(xp);
        npc->setProfile(FP, BS, S, A, M, D, B, Sp); // must be done after setting level, so that the initial level is set correctly

        QStringRef initial_level_s = reader.attributes().value("initial_level");
        if( initial_level_s.length() > 0 ) {
            int initial_level = parseInt(initial_level_s.toString());
            QStringRef initial_FP_s = reader.attributes().value("initial_FP");
            int initial_FP = parseInt(initial_FP_s.toString());
            QStringRef initial_BS_s = reader.attributes().value("initial_BS");
            int initial_BS = parseInt(initial_BS_s.toString());
            QStringRef initial_S_s = reader.attributes().value("initial_S");
            int initial_S = parseInt(initial_S_s.toString());
            QStringRef initial_A_s = reader.attributes().value("initial_A");
            int initial_A = parseInt(initial_A_s.toString());
            QStringRef initial_M_s = reader.attributes().value("initial_M");
            int initial_M = parseInt(initial_M_s.toString());
            QStringRef initial_D_s = reader.attributes().value("initial_D");
            int initial_D = parseInt(initial_D_s.toString());
            QStringRef initial_B_s = reader.attributes().value("initial_B");
            int initial_B = parseInt(initial_B_s.toString());
            QStringRef initial_Sp_s = reader.attributes().value("initial_Sp");
            float initial_Sp = parseFloat(initial_Sp_s.toString());
            npc->setInitialProfile(initial_level, initial_FP, initial_BS, initial_S, initial_A, initial_M, initial_D, initial_B, initial_Sp);
        }

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
        QStringRef is_diseased_s = reader.attributes().value("is_diseased");
        bool is_diseased = parseBool(is_diseased_s.toString(), true);
        npc->setDiseased(is_diseased);
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
        QStringRef is_fleeing_s = reader.attributes().value("is_fleeing");
        bool is_fleeing = parseBool(is_fleeing_s.toString(), true);
        npc->setFleeing(is_fleeing);
        QStringRef causes_disease_s = reader.attributes().value("causes_disease");
        int causes_disease = parseInt(causes_disease_s.toString(), true);
        npc->setCausesDisease(causes_disease);
        QStringRef causes_paralysis_s = reader.attributes().value("causes_paralysis");
        int causes_paralysis = parseInt(causes_paralysis_s.toString(), true);
        npc->setCausesParalysis(causes_paralysis);
        QStringRef requires_magical_s = reader.attributes().value("requires_magical");
        bool requires_magical = parseBool(requires_magical_s.toString(), true);
        npc->setRequiresMagical(requires_magical);
        QStringRef unholy_s = reader.attributes().value("unholy");
        bool unholy = parseBool(unholy_s.toString(), true);
        npc->setUnholy(unholy);
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
    QStringRef interaction_reward_item_s = reader.attributes().value("interaction_reward_item");
    npc->setInteractionRewardItem(interaction_reward_item_s.toString().toStdString());
    QStringRef interaction_journal_s = reader.attributes().value("interaction_journal");
    npc->setInteractionJournal(interaction_journal_s.toString().toStdString());
    QStringRef shop_s = reader.attributes().value("shop");
    if( shop_s.length() > 0 ) {
        npc->setShop(shop_s.toString().toStdString());
    }
    QStringRef objective_id_s = reader.attributes().value("objective_id");
    if( objective_id_s.length() > 0 ) {
        npc->setObjectiveId(objective_id_s.toString().toStdString());
    }

    if( default_pos_x_s.length() > 0 && default_pos_y_s.length() > 0 ) {
        float default_pos_x = parseFloat(default_pos_x_s.toString());
        float default_pos_y = parseFloat(default_pos_y_s.toString());
        npc->setDefaultPosition(default_pos_x, default_pos_y);
    }

    // now read remaining elements

    return npc;
}

Item *PlayingGamestate::loadItem(Vector2D *pos, QXmlStreamReader &reader, Scenery *scenery, Character *npc) const {
    Item *item = NULL;
    pos->set(0.0f, 0.0f);

    if( scenery != NULL && npc != NULL ) {
        throw string("loadItem called with both scenery and npc specified");
    }

    // need to read everything we want from the reader, before calling parseXMLItem.
    QStringRef template_s = reader.attributes().value("template");
    bool current_weapon = false, current_ammo = false, current_shield = false, current_armour = false, current_ring = false;
    if( scenery != NULL ) {
    }
    else if( npc != NULL ) {
        QStringRef current_weapon_s = reader.attributes().value("current_weapon");
        current_weapon = parseBool(current_weapon_s.toString(), true);
        QStringRef current_ammo_s = reader.attributes().value("current_ammo");
        current_ammo = parseBool(current_ammo_s.toString(), true);
        QStringRef current_shield_s = reader.attributes().value("current_shield");
        current_shield = parseBool(current_shield_s.toString(), true);
        QStringRef current_armour_s = reader.attributes().value("current_armour");
        current_armour = parseBool(current_armour_s.toString(), true);
        QStringRef current_ring_s = reader.attributes().value("current_ring");
        current_ring = parseBool(current_ring_s.toString(), true);
    }
    else {
        QStringRef pos_x_s = reader.attributes().value("x");
        float pos_x = parseFloat(pos_x_s.toString());
        QStringRef pos_y_s = reader.attributes().value("y");
        float pos_y = parseFloat(pos_y_s.toString());
        pos->set(pos_x, pos_y);
    }

    if( reader.name() == "gold" ) {
        // special case
        QStringRef amount_s = reader.attributes().value("amount");
        int amount = parseInt(amount_s.toString());
        item = this->cloneGoldItem(amount);
    }
    else if( template_s.length() > 0 ) {
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

    if( scenery != NULL ) {
        scenery->addItem(item);
    }
    else if( npc != NULL ) {
        npc->addItem(item, npc != this->player);
        if( current_weapon ) {
            if( item->getType() != ITEMTYPE_WEAPON ) {
                LOG("error at line %d\n", reader.lineNumber());
                throw string("current_weapon is not a weapon");
            }
            Weapon *weapon = static_cast<Weapon *>(item);
            npc->armWeapon(weapon);
        }
        else if( current_ammo ) {
            if( item->getType() != ITEMTYPE_AMMO ) {
                LOG("error at line %d\n", reader.lineNumber());
                throw string("current_weapon is not an ammo");
            }
            Ammo *ammo = static_cast<Ammo *>(item);
            npc->selectAmmo(ammo);
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
                throw string("current_armour is not an armour");
            }
            Armour *armour = static_cast<Armour *>(item);
            npc->wearArmour(armour);
        }
        else if( current_ring ) {
            if( item->getType() != ITEMTYPE_RING ) {
                LOG("error at line %d\n", reader.lineNumber());
                throw string("current_ring is not a ring");
            }
            Ring *ring = static_cast<Ring *>(item);
            npc->wearRing(ring);
        }
    }

    return item;
}

void PlayingGamestate::loadQuest(const QString &filename, bool is_savegame) {
    LOG("PlayingGamestate::loadQuest(%s)\n", filename.toUtf8().data());
    // filename should be full path

    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->setEnabled(false);
    game_g->getScreen()->setPaused(true, true);

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

    if( !is_savegame ) {
        this->time_hours = 1; // reset
    }

    qDebug("create new quest\n");
    this->quest = new Quest();
    //this->quest->setCompleted(true); // test

    if( !is_savegame ) {
        this->view_transform_3d = true;
        this->view_walls_3d = true;
        // if not a savegame, we should be loading new quest data that ought to be compatible
        // if it is a savegame, we check the versioning when loading
    }

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
        QFile file(filename);
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open quest xml file");
        }
        qint64 size = file.size();
        int progress_count = 0;
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            //qDebug("read %d element: %s", reader.tokenType(), reader.name().toString().toStdString().c_str());
            if( reader.isStartElement() )
            {
                progress_count++;
                if( progress_count % 20 == 0 ) {
                    float progress = ((float)file.pos()) / ((float)size);
                    gui_overlay->setProgress((1.0f - progress) * progress_lo + progress * progress_hi);
                    qApp->processEvents();
                }

                qDebug("read start element: %s (questXMLType=%d)", reader.name().toString().toStdString().c_str(), questXMLType);
                if( reader.name() == "quest" ) {
                    if( is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: quest element not expected in save games");
                    }
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: quest element wasn't expected here");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    quest->setName(name_s.toString().toStdString());
                }
                else if( reader.name() == "info" ) {
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
                else if( reader.name() == "savegame" ) {
                    if( !is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: savegame element only allowed in save games");
                    }
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: savegame element wasn't expected here");
                    }
                    QStringRef savegame_version_s = reader.attributes().value("savegame_version");
                    int savegame_version = parseInt(savegame_version_s.toString());
                    LOG("savegame_version = %d\n", savegame_version);
                    if( savegame_version >= 2 ) {
                        this->view_transform_3d = true;
                        this->view_walls_3d = true;
                    }
                }
                else if( reader.name() == "current_quest" ) {
                    if( !is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: current_quest element only allowed in save games");
                    }
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: current_quest element wasn't expected here");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    bool found = false;
                    int c = 0;
                    for(vector<QuestInfo>::const_iterator iter = this->quest_list.begin(); iter != this->quest_list.end(); ++iter, c++) {
                        const QuestInfo *quest_info = &*iter;
                        if( quest_info->getFilename() == name_s.toString().toStdString() ) {
                            found = true;
                            this->c_quest_indx = c;
                        }
                    }
                    if( !found ) {
                        LOG("error, current quest not found in quest list: %s\n", name_s.toString().toStdString().c_str());
                        this->c_quest_indx = 0;
                    }
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
                else if( reader.name() == "time_hours" ) {
                    if( !is_savegame ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: time_hours element only allowed in save games");
                    }
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: time_hours element wasn't expected here");
                    }
                    QStringRef time_hours_s = reader.attributes().value("value");
                    this->time_hours = parseInt(time_hours_s.toString());
                    LOG("time_hours = %d\n", time_hours);
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
                            this->setDifficulty(test_difficulty);
                            break;
                        }
                    }
                    // if not defined, we keep to the default

                    QStringRef permadeath_s = reader.attributes().value("permadeath");
                    qDebug("read permadeath: %s\n", permadeath_s.toString().toStdString().c_str());
                    this->permadeath = parseBool(permadeath_s.toString(), true);
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
                        QStringRef image_x_scale_s = reader.attributes().value("image_x_scale");
                        if( image_x_scale_s.length() > 0 ) {
                            float image_x_scale = parseFloat(image_x_scale_s.toString());
                            location->setWallXScale(image_x_scale);
                        }
                    }
                }
                else if( reader.name() == "dropwall" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: dropwall element wasn't expected here");
                    }
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: dropwall element outside of location");
                    }
                    if( image_name_s.length() > 0 ) {
                        location->setDropWallImageName(image_name_s.toString().toStdString());
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
                else if( reader.name() == "wandering_monster" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: wandering_monster element wasn't expected here");
                    }
                    QStringRef template_s = reader.attributes().value("template");
                    if( template_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: wandering_monster element has no template attribute");
                    }
                    QStringRef time_s = reader.attributes().value("time");
                    QStringRef rest_chance_s = reader.attributes().value("rest_chance");
                    int time = parseInt(time_s.toString());
                    int rest_chance = parseInt(rest_chance_s.toString());
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: wandering_monster element outside of location");
                    }
                    location->setWanderingMonster(template_s.toString().toStdString(), time, rest_chance);
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
                else if( reader.name() == "tilemap" ) {
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: tilemap element outside of location");
                    }
                    QStringRef x_s = reader.attributes().value("x");
                    float x = parseFloat(x_s.toString());
                    QStringRef y_s = reader.attributes().value("y");
                    float y = parseFloat(y_s.toString());
                    QStringRef imagemap_s = reader.attributes().value("imagemap");
                    QString imagemap = imagemap_s.toString();
                    QStringRef tile_width_s = reader.attributes().value("tile_width");
                    int tile_width = parseInt(tile_width_s.toString());
                    QStringRef tile_height_s = reader.attributes().value("tile_height");
                    int tile_height = parseInt(tile_height_s.toString());
                    QString map = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    if( map.length() > 0 ) {
                        qDebug("tilemap:");
                        int index = 0;
                        if( map.at(index) == '\n' )
                            index++;
                        vector<string> map_lines;
                        for(;;) {
                            int next_index =  map.indexOf('\n', index);
                            if( next_index == -1 || next_index == index )
                                break;
                            QString line = map.mid(index, next_index - index); // doesn't copy the newline
                            map_lines.push_back(line.toStdString());
                            qDebug("    %s", line.toStdString().c_str());
                            index = next_index+1;
                        }
                        Tilemap *tilemap = new Tilemap(x, y, imagemap.toStdString(), tile_width, tile_height, map_lines);
                        location->addTilemap(tilemap);
                    }
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

                    bool is_player = false;
                    Vector2D pos;
                    npc = this->loadNPC(&is_player, &pos, reader);
                    if( is_player ) {
                        this->player = npc;
                    }
                    else {
                        if( !npc->isStaticImage() ) {
                            this->animation_layers[ npc->getAnimationName() ]->getAnimationLayer(); // force animation to be loaded
                        }
                    }

                    // if an NPC doesn't have a default position defined in the file, we set it to the position
                    if( !npc->hasDefaultPosition() ) {
                        npc->setDefaultPosition(pos.x, pos.y);
                    }
                    if( location == NULL ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: npc/player element outside of location");
                    }
                    location->addCharacter(npc, pos.x, pos.y);
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
                    QStringRef journal_s = reader.attributes().value("journal");
                    QString journal = journal_s.toString();
                    QStringRef while_not_done_s = reader.attributes().value("while_not_done");
                    bool while_not_done = parseBool(while_not_done_s.toString(), true);
                    QStringRef objective_s = reader.attributes().value("objective");
                    bool objective = parseBool(objective_s.toString(), true);
                    QString answer = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    npc->addTalkItem(question.toStdString(), answer.toStdString(), action.toStdString(), journal.toStdString(), while_not_done, objective);
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
                else if( reader.name() == "item" || reader.name() == "weapon" || reader.name() == "shield" || reader.name() == "armour" || reader.name() == "ring" || reader.name() == "ammo" || reader.name() == "currency" || reader.name() == "gold" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE && questXMLType != QUEST_XML_TYPE_SCENERY && questXMLType != QUEST_XML_TYPE_NPC ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: item element wasn't expected here");
                    }

                    Vector2D pos;
                    Item *item = this->loadItem(&pos, reader, scenery, npc);

                    if( questXMLType == QUEST_XML_TYPE_NONE ) {
                        if( location == NULL ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unexpected quest xml: item element outside of location");
                        }
                        location->addItem(item, pos.x, pos.y);
                    }
                }
                else if( reader.name() == "scenery" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: scenery element wasn't expected here");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    QStringRef big_image_name_s = reader.attributes().value("big_image_name");
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    QStringRef opacity_s = reader.attributes().value("opacity");
                    QStringRef has_smoke_s = reader.attributes().value("has_smoke");
                    QStringRef smoke_x_s = reader.attributes().value("smoke_x");
                    QStringRef smoke_y_s = reader.attributes().value("smoke_y");
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
                    QStringRef key_always_needed_s = reader.attributes().value("key_always_needed");
                    bool key_always_needed = parseBool(key_always_needed_s.toString(), true);
                    QStringRef unlock_item_name_s = reader.attributes().value("unlocked_by_template");
                    QStringRef unlock_text_s = reader.attributes().value("unlock_text");
                    QStringRef unlock_xp_s = reader.attributes().value("unlock_xp");
                    QStringRef confirm_text_s = reader.attributes().value("confirm_text");
                    float size_w = 0.0f, size_h = 0.0f;
                    QStringRef size_s = reader.attributes().value("size");
                   /* bool is_animation = false;
                   *map<string, QPixmap>::iterator image_iter = this->scenery_images.find(image_name_s.toString().toStdString());
                    map<string, LazyAnimationLayer *>::const_iterator animation_iter = this->scenery_animation_layers.find(image_name_s.toString().toStdString());
                    if( image_iter == this->scenery_images.end() ) {
                        if( animation_iter == this->scenery_animation_layers.end() ) {
                            LOG("failed to find image for scenery: %s\n", name_s.toString().toStdString().c_str());
                            LOG("    image name: %s\n", image_name_s.toString().toStdString().c_str());
                            throw string("Failed to find scenery's image");
                        }
                        else {
                            is_animation = true;
                            animation_iter->second->getAnimationLayer(); // force animation to be loaded
                        }
                    }
                    else {
                        is_animation = false;
                    }*/
                    map<string, LazyAnimationLayer *>::const_iterator animation_iter = this->scenery_animation_layers.find(image_name_s.toString().toStdString());
                    if( animation_iter == this->scenery_animation_layers.end() ) {
                        LOG("failed to find image for scenery: %s\n", name_s.toString().toStdString().c_str());
                        LOG("    image name: %s\n", image_name_s.toString().toStdString().c_str());
                        throw string("Failed to find scenery's image");
                    }
                    else {
                        animation_iter->second->getAnimationLayer(); // force animation to be loaded
                    }

                    if( size_s.length() > 0 ) {
                        float size = parseFloat(size_s.toString());
                        /*QPixmap image;
                        if( is_animation ) {
                            const AnimationLayer *animation_layer = animation_iter->second->getAnimationLayer();
                            image = animation_layer->getAnimationSet("")->getFrame(0, 0);
                        }
                        else {
                            image = image_iter->second;
                        }*/
                        const AnimationLayer *animation_layer = animation_iter->second->getAnimationLayer();
                        QPixmap image = animation_layer->getAnimationSet("")->getFrame(0, 0);
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

                    float visual_h = 0.0f;
                    QStringRef visual_h_s = reader.attributes().value("visual_h");
                    if( visual_h_s.length() > 0 ) {
                        visual_h = parseFloat(visual_h_s.toString());
                    }
                    else {
                        visual_h = size_h;
                    }

                    if( this->view_transform_3d ) {
                        // hack to get height right in 3D mode!
                        //if( door || exit /*|| draw_type_s.length() > 0*/ ) {
                        if( size_s.length() == 0 ) {
                            // if we have specified w/h values explicitly, we assume these are in world coordinates already
                        }
                        else {
                            // otherwise we assume the width/height of the image are already in "isometric"/3D format
                            if( visual_h_s.length() == 0 )
                                visual_h *= 2.0f; // to counter the QGraphicsView scaling
                            size_h = size_w;
                            if( visual_h_s.length() == 0 ) {
                                visual_h = std::max(visual_h, size_h);
                                ASSERT_LOGGER( visual_h - size_h >= 0.0f );
                            }
                        }
                    }

                    //scenery = new Scenery(name_s.toString().toStdString(), image_name_s.toString().toStdString(), is_animation, size_w, size_h, visual_h);
                    scenery = new Scenery(name_s.toString().toStdString(), image_name_s.toString().toStdString(), true, size_w, size_h, visual_h);
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

                    /*if( is_animation ) {
                        map<string, LazyAnimationLayer *>::const_iterator animation_iter = this->scenery_animation_layers.find(scenery->getImageName());
                        if( animation_iter != this->scenery_animation_layers.end() ) {
                            const AnimationLayer *animation_layer = animation_iter->second->getAnimationLayer();
                            if( animation_layer->getAnimationSet("opened") != NULL ) {
                                scenery->setCanBeOpened(true);
                            }
                        }
                    }*/
                    const AnimationLayer *animation_layer = animation_iter->second->getAnimationLayer();
                    if( animation_layer->getAnimationSet("opened") != NULL ) {
                        scenery->setCanBeOpened(true);
                    }

                    if( big_image_name_s.length() > 0 ) {
                        scenery->setBigImageName(big_image_name_s.toString().toStdString());
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
                        // we allow a default for backwards compatibility, for when the smoke positio was hardcoded for the campfire
                        Vector2D smoke_pos(0.5f, 0.4167f);
                        if( smoke_x_s.length() > 0 ) {
                            smoke_pos.x = parseFloat(smoke_x_s.toString());
                        }
                        if( smoke_y_s.length() > 0 ) {
                            smoke_pos.y = parseFloat(smoke_y_s.toString());
                        }
                        scenery->setHasSmoke(has_smoke, smoke_pos);
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
                    scenery->setKeyAlwaysNeeded(key_always_needed);
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

    if( this->permadeath && is_savegame ) {
        this->permadeath_has_savefilename = true;
        this->permadeath_savefilename = filename;
        if( this->quickSaveButton != NULL ) {
            delete this->quickSaveButton;
            this->quickSaveButton = NULL;
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
        loc->addSceneryToFloorRegions();
        loc->calculateDistanceGraph();

        progress_count++;
        if( progress_count % 4 == 0 ) {
            float progress = ((float)progress_count) / ((float)quest->getNLocations());
            gui_overlay->setProgress((1.0f - progress) * progress_lo + progress * progress_hi);
            qApp->processEvents();
        }
    }

    gui_overlay->unsetProgress();
    qApp->processEvents();

    this->c_location->setListener(this, NULL); // must do after creating the location and its contents, so it doesn't try to add items to the scene, etc
    this->setupView();

    window->setEnabled(true);
    game_g->getScreen()->setPaused(false, true);
    game_g->getScreen()->restartElapsedTimer();

    //qApp->processEvents();

    if( !is_savegame && quest->getInfo().length() > 0 ) {
        string quest_info = quest->getInfo();
        quest_info = convertToHTML(quest_info);
        stringstream str;
        str << "<html><body>";
        str << "<h1>Quest: " << quest->getName() << "</h1>";
        str << "<p>" << quest_info << "</p>";
        str << "</body></html>";
        this->showInfoWindow(str.str());

        this->journal_ss << "<hr/><p><b>Quest Details: " << quest->getName() << "</b></p>";
        this->journal_ss << "<p>" << quest_info << "</p>";
    }

    qDebug("View is transformed? %d", view->isTransformed());
    LOG("done\n");
}

void PlayingGamestate::createRandomQuest() {
    LOG("PlayingGamestate::createRandomQuest()\n");

    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->setEnabled(false);
    game_g->getScreen()->setPaused(true, true);

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

    qDebug("create random quest\n");
    this->quest = new Quest();
    //this->quest->setCompleted(true); // test

    this->view_transform_3d = true;
    this->view_walls_3d = true;

    Vector2D player_start;
    Location *location = LocationGenerator::generateLocation(&player_start);

    this->quest->addLocation(location);
    this->c_location = location;
    this->c_location->addCharacter(player, player_start.x, player_start.y);

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
        loc->addSceneryToFloorRegions();
        loc->calculateDistanceGraph();

        progress_count++;
        if( progress_count % 4 == 0 ) {
            float progress = ((float)progress_count) / ((float)quest->getNLocations());
            gui_overlay->setProgress((1.0f - progress) * progress_lo + progress * progress_hi);
            qApp->processEvents();
        }
    }

    gui_overlay->unsetProgress();
    qApp->processEvents();

    this->c_location->setListener(this, NULL); // must do after creating the location and its contents, so it doesn't try to add items to the scene, etc
    this->setupView();

    window->setEnabled(true);
    game_g->getScreen()->setPaused(false, true);
    game_g->getScreen()->restartElapsedTimer();

    //qApp->processEvents();

    if( quest->getInfo().length() > 0 ) {
        string quest_info = quest->getInfo();
        quest_info = convertToHTML(quest_info);
        stringstream str;
        str << "<html><body>";
        str << "<h1>Quest: " << quest->getName() << "</h1>";
        str << "<p>" << quest_info << "</p>";
        str << "</body></html>";
        this->showInfoWindow(str.str());

        this->writeJournal("<hr/><p><b>Quest Details: ");
        this->writeJournal(quest->getName());
        this->writeJournal("</b></p><p>");
        this->writeJournal(quest_info);
        this->writeJournal("</p>");
    }

    qDebug("View is transformed? %d", view->isTransformed());
    LOG("done\n");
}

void PlayingGamestate::addGraphicsItem(QGraphicsItem *object, float width, bool undo_3d) {
    float item_scale = width / object->boundingRect().width();
    if( this->view_transform_3d && undo_3d ) {
        // undo the 3D transform
        float centre_x = 0.5f*object->boundingRect().width();
        float centre_y = 0.5f*object->boundingRect().height();
        QTransform transform;
        transform = transform.scale(item_scale, 2.0f*item_scale);
        transform = transform.translate(-centre_x, -centre_y);
        object->setTransform(transform);
    }
    else {
        object->setTransformOriginPoint(-0.5f*object->boundingRect().width()*item_scale, -0.5f*object->boundingRect().height()*item_scale);
        object->setScale(item_scale);
    }
    scene->addItem(object);
}

void PlayingGamestate::locationAddItem(const Location *location, Item *item, bool visible) {
    if( this->c_location == location ) {
        QGraphicsPixmapItem *object = new QGraphicsPixmapItem();
        item->setUserGfxData(object);
        object->setPixmap( this->getItemImage( item->getImageName() ) );
        /*{
            // DEBUG
            QPen pen(Qt::red);
            scene->addEllipse(item->getX() - 0.5f*item_width, item->getY() - 0.5f*item_width, item_width, item_width, pen);
            //scene->addEllipse(item->getX() - 0.05f, item->getY() - 0.05f, 0.1f, 0.1f, pen);
        }*/
        float icon_width = item->getIconWidth();
        object->setPos(item->getX(), item->getY());
        object->setZValue(z_value_items);
        object->setVisible(visible);
        this->addGraphicsItem(object, icon_width, true);
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
            object = new AnimatedObject(100);
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
            //qDebug("background: %s", scenery->getName().c_str());
            z_value = z_value_scenery_background;
        }
        object->setZValue(z_value);
        float scenery_scale_w = scenery->getWidth() / object->boundingRect().width();
        float scenery_scale_h = scenery->getVisualHeight() / object->boundingRect().height();
        float centre_x = 0.5f*object->boundingRect().width();
        float centre_y = 0.5f*object->boundingRect().height();
        centre_y += 0.5f * (scenery->getVisualHeight() - scenery->getHeight()) / scenery_scale_h;
        if( scenery->getVisualHeight() - scenery->getHeight() < 0.0f ) {
            qDebug(">>> %s at %f, %f", scenery->getName().c_str(), scenery->getX(), scenery->getY());
            ASSERT_LOGGER( scenery->getVisualHeight() - scenery->getHeight() >= 0.0f );
        }
        QTransform transform;
        transform = transform.scale(scenery_scale_w, scenery_scale_h);
        transform = transform.translate(-centre_x, -centre_y);
        object->setTransform(transform);
        if( scenery->hasSmoke() ) {
            SmokeParticleSystem *ps = new SmokeParticleSystem(smoke_pixmap);
            ps->setBirthRate(25.0f);
            //ps->setPos(centre_x, 30.0f);
            //ps->setPos(0.5f*object->boundingRect().width(), 0.4167f*object->boundingRect().height());
            Vector2D smoke_pos = scenery->getSmokePos();
            ps->setPos(smoke_pos.x*object->boundingRect().width(), smoke_pos.y*object->boundingRect().height());
            ps->setZValue(object->pos().y() + 2000.0f);
            ps->setParentItem(object);
            QTransform transform2;
            qDebug("### scenery scale %f , %f", scenery_scale_w, scenery_scale_h);
            qDebug("### object %f x %f", object->boundingRect().width(), object->boundingRect().height());
            const float mult_y = this->view_transform_3d ? 2.0f : 1.0f;
            transform2 = transform2.scale(1.0f/(64.0f*scenery_scale_w), mult_y/(64.0f*scenery_scale_h));
            ps->setTransform(transform2);
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
#ifdef _DEBUG
            this->refreshDebugItems();
#endif
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
        object->addAnimationLayer( this->scenery_animation_layers[scenery->getImageName()]->getAnimationLayer() );
        if( scenery->isOpened() ) {
            object->setAnimationSet("opened", true);
        }
        else {
            object->setAnimationSet("", true);
        }
    }
    /*else {
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
    }*/
}

void PlayingGamestate::locationAddCharacter(const Location *location, Character *character) {
    AnimatedObject *object = new AnimatedObject(100);
    object->setBounce( character->isBounce() );
    this->characterUpdateGraphics(character, object);
    this->characterTurn(character, object);
    scene->addItem(object);
    object->setPos(character->getX(), character->getY());
    object->setZValue( object->pos().y() );
    if( character != player ) {
        // default to invisible for NPCs, until set by testFogOfWar()
        object->setVisible(false);
    }
    int character_size = std::max(object->getWidth(), object->getHeight());
    float ratio_w = object->getWidth()/(float)character_size;
    float ratio_h = object->getHeight()/(float)character_size;
    const int off_x = 64 - 32;
    const int off_y = 94 - 40;
    if( this->view_transform_3d ) {
        const float desired_size = 0.75f;
        float character_scale = desired_size / (float)character_size;
        qDebug("character %s size %d scale %f", character->getName().c_str(), character_size, character_scale);
        {
            Vector2D scale_centre(-desired_size*ratio_w*off_x/64.0f, -2.0f*desired_size*ratio_h*off_y/64.0f);
            QTransform transform;
            transform.translate(scale_centre.x, scale_centre.y);
            transform.scale(character_scale, 2.0f*character_scale);
            transform.translate(-scale_centre.x, -scale_centre.y);
            object->setTransform(transform);
        }
    }
    else {
        const float desired_size = 1.0f;
        float character_scale = desired_size / (float)character_size;
        qDebug("character %s size %d scale %f", character->getName().c_str(), character_size, character_scale);
        object->setTransformOriginPoint(-desired_size*ratio_w*off_x/64.0f, -desired_size*ratio_h*off_y/64.0f);
        object->setScale(character_scale);
    }

    character->setListener(this, object);
    //item->setAnimationSet("attack"); // test
}

void PlayingGamestate::clickedStats() {
    LOG("clickedStats()\n");
    this->closeSubWindow();

    new StatsWindow(this);
    game_g->getScreen()->setPaused(true, true);
}

void PlayingGamestate::clickedItems() {
    LOG("clickedItems()\n");
    this->closeSubWindow();

    new ItemsWindow(this);
    game_g->getScreen()->setPaused(true, true);
}

void PlayingGamestate::clickedJournal() {
    LOG("clickedJournal()\n");
    this->closeSubWindow();
    this->playSound("turn_page");
    stringstream str;
    str << "<html><body>";
    /*for(int i=0;i<100;i++) {
        str << "i = " << i << "<br/>";
    }*/
    str << "<h1>Journal</h1>";
    str << this->journal_ss.str();
    str << "<hr/><p>" << this->getJournalDate() << "</p>";
    str << "</body></html>";
    QWebView *web_view = this->showInfoWindow(str.str());
    web_view->page()->mainFrame()->setScrollBarValue(Qt::Vertical, web_view->page()->mainFrame()->scrollBarMaximum(Qt::Vertical));
}

void PlayingGamestate::clickedOptions() {
    LOG("clickedOptions()\n");
    this->closeSubWindow();

    game_g->getScreen()->setPaused(true, true);

    QWidget *subwindow = new QWidget();
    this->addWidget(subwindow, false);

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

int PlayingGamestate::getRestTime() const {
    int health_restore_percent = 100 - this->player->getHealthPercent();
    int time = (int)(health_restore_percent*10.0f/100.0f + 0.5f);
    if( time == 0 ) {
        time = 1;
    }
    return time;
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
        bool rest_ok = true;
        if( c_location->getWanderingMonsterTemplate().length() > 0 && c_location->getWanderingMonsterRestChance() > 0 ) {
            if( rand() % 100 < c_location->getWanderingMonsterRestChance() ) {
                Vector2D free_pvec;
                Character *enemy = this->createCharacter(c_location->getWanderingMonsterTemplate(), c_location->getWanderingMonsterTemplate());
                if( c_location->findFreeWayPoint(&free_pvec, this->player->getPos(), true, enemy->canFly()) ) {
                    rest_ok = false;
                    this->addTextEffect("You are disturbed by a\nwandering monster!", player->getPos(), 2000);
                    enemy->setDefaultPosition(free_pvec.x, free_pvec.y);
                    c_location->addCharacter(enemy, free_pvec.x, free_pvec.y);
                }
                else {
                    delete enemy;
                }
            }
        }
        if( rest_ok ) {
            int time = this->getRestTime();
            this->player->restoreHealth();
            this->player->expireProfileEffects();
            // also set all NPCs to no longer be fleeing
            for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
                Character *character = *iter;
                if( character->isFleeing() ) {
                    character->setFleeing(false);
                    character->setStateIdle();
                }
            }
            stringstream str;
            str << "Rested for " << time << " hour";
            if( time > 1 )
                str << "s";
            this->addTextEffect(str.str(), player->getPos(), 2000);
            time_hours += time;
        }
    }
}

bool PlayingGamestate::canSaveHere() {
    return c_location->hasEnemies(this);
}

void PlayingGamestate::clickedSave() {
    LOG("PlayingGamestate::clickedSave()\n");
    if( this->canSaveHere() ) {
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

QWebView *PlayingGamestate::showInfoWindow(const string &html) {
    // n.b., different to showInfoDialog, as this doesn't block and wait for an answer
    qDebug("showInfoWindow()\n");

    game_g->getScreen()->setPaused(true, true);

    QWidget *subwindow = new QWidget();

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

    this->addWidget(subwindow, false); // should be last, so resizing is already correct - needed for things like scrolling to bottom to work
    return label;
}

void PlayingGamestate::closeSubWindow() {
    LOG("closeSubWindow\n");
    /*int n_stacked_widgets = this->main_stacked_widget->count();
    if( n_stacked_widgets > 1 ) {
        QWidget *subwindow = this->main_stacked_widget->widget(n_stacked_widgets-1);
        this->main_stacked_widget->removeWidget(subwindow);
        subwindow->deleteLater();
        if( n_stacked_widgets == 2 ) {
            game_g->getScreen()->setPaused(false, true);
            this->view->setEnabled(true);
            this->view->grabKeyboard();
        }
    }*/
    int n_stacked_widgets = this->widget_stack.size();
    if( n_stacked_widgets > 1 ) {
        QWidget *subwindow = this->widget_stack.at(n_stacked_widgets-1);
        /*if( this->main_stacked_widget != NULL ) {
            this->main_stacked_widget->removeWidget(subwindow);
        }*/
        subwindow->deleteLater();
        this->widget_stack.erase( this->widget_stack.begin() + n_stacked_widgets-1);
        if( n_stacked_widgets == 2 ) {
            game_g->getScreen()->getMainWindow()->activateWindow(); // needed for Symbian at least
            game_g->getScreen()->setPaused(false, true);
            this->view->setEnabled(true);
            this->view->grabKeyboard();
        }
    }
}

void PlayingGamestate::closeAllSubWindows() {
    LOG("closeAllSubWindows");
    /*while( this->main_stacked_widget->count() > 1 ) {
        QWidget *subwindow = this->main_stacked_widget->widget(this->main_stacked_widget->count()-1);
        this->main_stacked_widget->removeWidget(subwindow);
        subwindow->deleteLater();
    }*/
    while( this->widget_stack.size() > 1 ) {
        QWidget *subwindow = this->widget_stack.at(this->widget_stack.size()-1);
        /*if( this->main_stacked_widget != NULL ) {
            this->main_stacked_widget->removeWidget(subwindow);
        }*/
        subwindow->deleteLater();
        this->widget_stack.erase(this->widget_stack.begin()+this->widget_stack.size()-1);
    }
    game_g->getScreen()->setPaused(false, true);
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

void PlayingGamestate::testFogOfWar() {
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        if( character != this->player ) {
            bool is_visible = c_location->visibilityTest(player->getPos(), character->getPos());
            character->setVisible(is_visible);
            AnimatedObject *object = static_cast<AnimatedObject *>(character->getListenerData());
            object->setVisible(is_visible);
        }
    }
}

void PlayingGamestate::update() {
    // update target item
    if( this->player != NULL ) {
        if( this->player->getTargetNPC() != NULL && this->player->getTargetNPC()->isHostile() ) {
            Vector2D target_pos = this->player->getTargetNPC()->getPos();
            if( this->target_item == NULL ) {
                //this->target_item = this->addPixmapGraphic(this->target_pixmap, target_pos, 0.5f, false, true);
                AnimatedObject *object = new AnimatedObject(100);
                this->target_item = object;
                object->addAnimationLayer( target_animation_layer );
                object->setZValue(z_value_gui);
                object->setPos(target_pos.x, target_pos.y);
                this->addGraphicsItem(object, 0.5f, false);
            }
            else {
                this->target_item->setPos(target_pos.x, target_pos.y);
            }
        }
        else {
            if( this->target_item != NULL ) {
                delete this->target_item;
                this->target_item = NULL;
            }
        }
    }
    else {
        if( this->target_item != NULL ) {
            delete this->target_item;
            this->target_item = NULL;
        }
    }

    if( this->player == NULL ) {
        return;
    }
//#define TIMING_INFO

    int elapsed_ms = game_g->getScreen()->getGameTimeTotalMS();

    //qDebug("PlayingGamestate::update()");
    bool do_complex_update = false;
    int complex_time_ms = 0; // time since last "complex" update

    if( elapsed_ms - this->time_last_complex_update_ms > 100 ) {
        if( time_last_complex_update_ms == 0 ) {
            complex_time_ms = game_g->getScreen()->getGameTimeFrameMS();
        }
        else {
            complex_time_ms = elapsed_ms - time_last_complex_update_ms;
        }
        this->time_last_complex_update_ms = elapsed_ms;
        do_complex_update = true;

        // we need to run the update() loop at a fast rate (preferably 60FPS) so that the movement and animation is updated as smoothly as possible on fast systems
        // but we don't need to run other things every frame (and doing so risks the update taking too long on slower systems, leading to a spiral of death)
    }

    if( do_complex_update ) {
#ifdef TIMING_INFO
        QElapsedTimer timer_complex;
        timer_complex.start();
#endif
        // we can get away with not updating this every call
        this->testFogOfWar();

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

        // spawning wandering monsters
        if( c_location->getWanderingMonsterTemplate().length() > 0 && c_location->getWanderingMonsterTimeMS() > 0 ) {
            int prob = poisson(c_location->getWanderingMonsterTimeMS(), complex_time_ms);
            int roll = rand();
            //qDebug("prob: %d vs %d (update frame time %d, rate %d)", roll, prob, complex_time_ms, c_location->getWanderingMonsterTimeMS());
            if( roll < prob ) {
                Vector2D free_pvec;
                Character *enemy = this->createCharacter(c_location->getWanderingMonsterTemplate(), c_location->getWanderingMonsterTemplate());
                if( c_location->findFreeWayPoint(&free_pvec, this->player->getPos(), false, enemy->canFly()) ) {
                    qDebug("spawn wandering monster at %f, %f", free_pvec.x, free_pvec.y);
                    enemy->setDefaultPosition(free_pvec.x, free_pvec.y);
                    c_location->addCharacter(enemy, free_pvec.x, free_pvec.y);
                }
                else {
                    delete enemy;

                }
            }
        }

#ifdef TIMING_INFO
        qDebug("complex update took %d", timer_complex.elapsed());
#endif
    }

    {
#ifdef TIMING_INFO
        QElapsedTimer timer_kinput;
        timer_kinput.start();
#endif
        // update due to keyboard input
        // note that this doesn't actually move the player directly, but sets a target direction to move to

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
            // check if we need to update the target
            bool need_update = true;
            if( this->player->hasPath() ) {
                Vector2D current_dest = this->player->getDestination();
                Vector2D current_dir = current_dest - this->player->getPos();
                if( current_dir.magnitude() > 0.5f*step ) {
                    Vector2D new_dir = dest - player->getPos();
                    current_dir.normalise();
                    new_dir.normalise();
                    if( current_dir.isEqual(new_dir, E_TOL_LINEAR) ) {
                        need_update = false;
                    }
                }
            }
            if( need_update ) {
                //qDebug("update");
                this->requestPlayerMove(dest, NULL);
            }
            /*else {
                qDebug("no update");
            }*/
        }
#ifdef TIMING_INFO
        qDebug("keyboard input took %d", timer_kinput.elapsed());
#endif
    }

#ifdef TIMING_INFO
    QElapsedTimer timer_advance;
    timer_advance.start();
#endif
    scene->advance();
#ifdef TIMING_INFO
    qDebug("scene advance took %d", timer_advance.elapsed());
#endif


    vector<CharacterAction *> delete_character_actions;
    for(vector<CharacterAction *>::iterator iter = this->character_actions.begin(); iter != this->character_actions.end(); ++iter) {
        CharacterAction *character_action = *iter;
        if( character_action->isExpired() ) {
            delete_character_actions.push_back(character_action);
        }
        else {
            character_action->update();
        }
    }
    for(vector<CharacterAction *>::iterator iter = delete_character_actions.begin(); iter != delete_character_actions.end(); ++iter) {
        CharacterAction *character_action = *iter;
        for(vector<CharacterAction *>::iterator iter2 = this->character_actions.begin(); iter2 != this->character_actions.end(); ++iter2) {
            CharacterAction *character_action2 = *iter2;
            if( character_action2 == character_action ) {
                this->character_actions.erase(iter2);
                break;
            }
        }
        character_action->implement(this);
        delete character_action;
    }

#ifdef TIMING_INFO
    QElapsedTimer timer_cupdate;
    timer_cupdate.start();
#endif
    // Character::update() also handles movement, so need to do that with every update() call
    // though we could split out the AI etc to a separate function, as that doesn't need to be done with every update() call
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
#ifdef TIMING_INFO
        qDebug("character update took %d", timer_cupdate.elapsed());
#endif
    //LOG("done update characters\n");
    // handle deleted characters
#ifdef TIMING_INFO
        QElapsedTimer timer_dcharacters;
        timer_dcharacters.start();
#endif
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
        for(vector<CharacterAction *>::iterator iter = this->character_actions.begin(); iter != this->character_actions.end(); ++iter) {
            CharacterAction *character_action = *iter;
            character_action->notifyDead(character);
        }

        if( character == this->player ) {
            if( this->permadeath && this->permadeath_has_savefilename ) {
                QString full_path = this->permadeath_savefilename;
                if( QFile::remove(full_path) ) {
                    LOG("permadeath mode: removed save filename: %s\n", full_path.toUtf8().data());
                }
                else {
                    LOG("permadeath mode: failed to remove save filename!: %s\n", full_path.toUtf8().data());
                }
            }
            stringstream death_message;
            int r = rand() % 3;
            if( r == 0 ) {
                death_message << "<p><b>Game over</b></p><p>You have died! Your noble quest has come to an end. Your corpse rots away, left for future brave adventurers to encounter.</p>";
            }
            else if( r == 1 ) {
                death_message << "<p><b>Game over</b></p><p>You have died! Your adventure has met an untimely end. Better luck next time!</p>";
            }
            else {
                death_message << "<p><b>Game over</b></p><p>You are dead! Your time on this mortal plane is over, and your adventure ends here.</p>";
            }
            death_message << "<p><b>Achieved Level:</b> " << player->getLevel() << "<br/><b>Achieved XP:</b> " << player->getXP() << "</p>";
            this->showInfoDialog(death_message.str(), string(DEPLOYMENT_PATH) + "gfx/scenes/death.jpg");

            this->player = NULL;
            GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
            game_g->pushMessage(game_message);
        }
        delete character; // also removes character from the QGraphicsScene, via the listeners
    }
    if( this->player != NULL && delete_characters.size() > 0 ) {
        this->checkQuestComplete();
    }
#ifdef TIMING_INFO
        qDebug("deleted characters took %d", timer_dcharacters.elapsed());
#endif
    //qDebug("PlayingGamestate::update() exit");

}

void PlayingGamestate::updateInput() {
    if( this->player == NULL ) {
        return;
    }

    // scroll
    bool scrolled = false;
    int real_time_ms = game_g->getScreen()->getInputTimeFrameMS();
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

    this->view->updateInput();
}

void PlayingGamestate::render() {
    // n.b., won't render immediately, but schedules for repainting from Qt's main event loop
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
        object->addAnimationLayer( this->animation_layers["player"]->getAnimationLayer() );
        if( character->getCurrentWeapon() != NULL && character->getCurrentWeapon()->getAnimationName().length() > 0 ) {
            object->addAnimationLayer( this->animation_layers[ character->getCurrentWeapon()->getAnimationName() ]->getAnimationLayer() );
        }
        if( character->getCurrentShield() != NULL && character->getCurrentShield()->getAnimationName().length() > 0 ) {
            object->addAnimationLayer( this->animation_layers[ character->getCurrentShield()->getAnimationName() ]->getAnimationLayer() );
        }
    }
    else {
        object->addAnimationLayer( this->animation_layers[ character->getAnimationName() ]->getAnimationLayer() );
    }
}

void PlayingGamestate::characterTurn(const Character *character, void *user_data) {
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    Vector2D dir = character->getDirection();
    Direction direction = directionFromVecDir(dir);
    object->setDimension(direction);
}

void PlayingGamestate::characterMoved(Character *character, void *user_data) {
    //QGraphicsItem *item = static_cast<QGraphicsItem *>(user_data);
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    QPointF old_pos = object->pos();
    QPointF new_pos(character->getX(), character->getY());
    if( new_pos != old_pos ) {
        object->setPos(new_pos);
        object->setZValue( object->pos().y() );
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
                    vector<TalkItem *> talk_items;
                    if( !character->isHostile() ) {
                        // being hostile means the character has become hostile whilst talking, so now the only thing left is to say Goodbye!
                        int count=0;
                        for(vector<TalkItem>::iterator iter = character->talkItemsBegin(); iter != character->talkItemsEnd(); ++iter, count++) {
                            TalkItem *talk_item = &*iter;
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
                    this->addWidget(dialog, false);
                    dialog->scrollToBottom();
                    int result = dialog->exec();
                    this->closeSubWindow();
                    ASSERT_LOGGER(result >= 0);
                    ASSERT_LOGGER(result < buttons.size());
                    if( result == buttons.size()-1 ) {
                        break;
                    }
                    else {
                        TalkItem *talk_item = talk_items.at(result);
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
                        if( talk_item->journal.length() > 0 ) {
                            this->writeJournal("<hr/><p>");
                            this->writeJournalDate();
                            this->writeJournal(talk_item->journal);
                            this->writeJournal("</p>");
                            talk_item->journal = ""; // stops the journal text being written repeatedly
                        }
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
    //float min_dist = 0.0f;
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
            //min_dist = dist_from_click;
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
        game_g->getScreen()->setPaused(true, true);
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
                        if( item->getType() == ITEMTYPE_WEAPON && player->getCurrentWeapon() != item )
                            continue;
                        else if( item->getType() == ITEMTYPE_ARMOUR && player->getCurrentArmour() != item )
                            continue;
                        else if( item->getType() == ITEMTYPE_SHIELD && player->getCurrentShield() != item )
                            continue;
                        else if( item->getType() == ITEMTYPE_RING && player->getCurrentRing() != item )
                            continue;
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
                    qDebug("isKeyAlwaysNeeded? %d", scenery->isKeyAlwaysNeeded());
                    if( !scenery->isKeyAlwaysNeeded() ) {
                        scenery->setLocked(false);
                    }
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
                    if( this->quest->getQuestObjective() != NULL && this->quest->getQuestObjective()->getType() == "find_exit" && this->quest->getQuestObjective()->getArg1() == scenery->getName() ) {
                        this->quest->setCompleted(true);
                    }

                    this->quest->testIfComplete(this); // recall, in case quest no longer completed (e.g., player has dropped an item that is needed)
                    if( this->getQuest()->isCompleted() ) {
                        this->getQuest()->getQuestObjective()->completeQuest(this);
                        /*int gold = this->getQuest()->getQuestObjective()->getGold();
                        this->player->addGold(gold);*/
                        string completed_text = this->getQuest()->getCompletedText();
                        completed_text = convertToHTML(completed_text);
                        this->showInfoDialog(completed_text);
                        this->writeJournal("<hr/><p>");
                        this->writeJournalDate();
                        this->writeJournal(completed_text);
                        this->writeJournal("</p>");
                    }
                    new CampaignWindow(this);
                    game_g->getScreen()->setPaused(true, true);
                    this->time_hours += 48 + this->getRestTime();
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
                        this->addWidget(dialog, false);
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
                    this->showInfoDialog(description, scenery->getBigImageName());
                }
            }
        }
    }
    return done;
}

bool PlayingGamestate::handleClickForScenerys(bool *move, Scenery **ignore_scenery, Vector2D dest, bool is_click) {
    //qDebug("PlayingGamestate::handleClickForScenerys(): %f, %f", dest.x, dest.y);
    // search for clicking on a scenery
    const float click_tol_scenery_c = 0.0f;
    vector<Scenery *> clicked_scenerys;
    for(set<Scenery *>::iterator iter = c_location->scenerysBegin(); iter != c_location->scenerysEnd(); ++iter) {
        Scenery *scenery = *iter;
        Vector2D scenery_pos = scenery->getPos();
        float scenery_width = scenery->getWidth();
        float scenery_height = is_click ? scenery->getVisualHeight() : scenery->getHeight();
        float dist_from_click = distFromBox2D(scenery_pos, scenery_width, scenery_height, dest);
        //LOG("dist_from_click for scenery %s : %f", scenery->getName().c_str(), dist_from_click);
        if( dist_from_click <= npc_radius_c && scenery->isBlocking() ) {
            // clicked on or near this scenery
            *ignore_scenery = scenery;
        }
        if( dist_from_click <= click_tol_scenery_c ) {
            // clicked on this scenery
            float player_dist = distFromBox2D(scenery_pos, scenery_width, scenery_height, player->getPos());
            //qDebug("    player_dist from %s: %f", scenery->getName().c_str(), player_dist);
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

    if( player != NULL && !player->isDead() && !player->isParalysed() ) {
        bool done = false;
        Vector2D forward_dest1 = player->getPos() + player->getDirection() * npc_radius_c * 1.1f;
        Vector2D forward_dest2 = player->getPos() + player->getDirection() * npc_radius_c * 2.0f;
        Vector2D forward_dest3 = player->getPos() + player->getDirection() * npc_radius_c * 3.0f;

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
            if( target_npc == NULL && player->getCurrentWeapon() != NULL && player->getCurrentWeapon()->isRangedOrThrown() ) {
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
            done = handleClickForScenerys(&move, &ignore_scenery, forward_dest1, false);
            if( !done ) {
                done = handleClickForScenerys(&move, &ignore_scenery, forward_dest2, false);
            }
            if( !done && player->getDirection().y < 0.0f ) {
                // needed for scenery on walls
                done = handleClickForScenerys(&move, &ignore_scenery, forward_dest3, false);
            }
        }

        if( !done ) {
            // We do items last, unlike with mouse click, so that the player can still do other things even if standing over items.
            // If the player wants to force picking up items, they can do that via a separate shortcut
            done = handleClickForItems( player->getPos() );
        }

    }
}

void PlayingGamestate::cycleTargetNPC() {
    if( this->player != NULL ) {
        Character *current_target = this->player->getTargetNPC();
        this->player->setTargetNPC(NULL);
        bool was_last = false;
        Character *first_npc = NULL;
        for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
            Character *character = *iter;
            if( character == player )
                continue;
            if( !character->isHostile() )
                continue;
            if( !character->isVisible() )
                continue;
            if( first_npc == NULL ) {
                first_npc = character;
            }
            if( current_target == NULL ) {
                this->player->setTargetNPC(character);
                break;
            }
            else if( current_target == character ) {
                was_last = true;
            }
            else if( was_last ) {
                this->player->setTargetNPC(character);
                break;
            }
        }
        if( this->player->getTargetNPC() == NULL && first_npc != NULL ) {
            // reset to first NPC
            this->player->setTargetNPC(first_npc);
        }
    }

}

void PlayingGamestate::clickedMainView(float scene_x, float scene_y) {
    if( game_g->getScreen()->isPaused() ) {
        game_g->getScreen()->setPaused(false, false);
    }

    if( player != NULL && !player->isDead() && !player->isParalysed() ) {
        //player->setPos(scene_x, scene_y);

        Vector2D dest(scene_x, scene_y);

        bool done = false;
        bool move = true;

        // do items first - so that player can always pick up items dropped near friendly (or otherwise non-moving) NPCs, as well as behind scenery!
        if( !done ) {
            done = handleClickForItems(dest);
        }

        // search for clicking on an NPC
        if( !done )
        {
            //qDebug("look for clicking on NPCs");
            float min_dist = 0.0f;
            Character *target_npc = NULL;
            for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
                Character *character = *iter;
                if( character == player )
                    continue;
                if( !character->isVisible() )
                    continue;
                /*
                const float click_tol_npc_c = 0.0f;
                float dist_from_click = (dest - character->getPos()).magnitude();
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
                    float this_dist = (character->getPos() - dest).magnitude();
                    //qDebug("  clicked on npc, dist: %f", this_dist);
                    if( !done || this_dist < min_dist )
                    {
                        done = true;
                        target_npc = character;
                        min_dist = this_dist;
                    }
                }
            }
            this->clickedOnNPC(target_npc); // call even if target_npc is NULL
        }

        Scenery *ignore_scenery = NULL; // scenery to ignore for collision detection - so we can move towards a scenery that blocks
        if( !done ) {
            done = handleClickForScenerys(&move, &ignore_scenery, dest, true);
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
        //qDebug("ignoring scenery: %s", ignore_scenery==NULL ? "NULL" : ignore_scenery->getName().c_str());
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

void PlayingGamestate::saveItemProfileBonusInt(QTextStream &stream, const Item *item, const string &key) const {
    int value = item->getRawProfileBonusIntProperty(key);
    if( value > 0 ) {
        //fprintf(file, " bonus_%s=\"%d\"", key.c_str(), value);
        stream << " bonus_" << key.c_str() << "=\"" << value << "\"";
    }
}

void PlayingGamestate::saveItemProfileBonusFloat(QTextStream &stream, const Item *item, const string &key) const {
    float value = item->getRawProfileBonusFloatProperty(key);
    if( value > 0 ) {
        //fprintf(file, " bonus_%s=\"%f\"", key.c_str(), value);
        stream << " bonus_" << key.c_str() << "=\"" << value << "\"";
    }
}

void PlayingGamestate::saveItem(QTextStream &stream, const Item *item) const {
    return saveItem(stream, item, NULL);
}

void PlayingGamestate::saveItem(QTextStream &stream, const Item *item, const Character *character) const {
    switch( item->getType() ) {
    case ITEMTYPE_GENERAL:
        //fprintf(file, "<item");
        stream << "<item";
        break;
    case ITEMTYPE_WEAPON:
        //fprintf(file, "<weapon");
        stream << "<weapon";
        break;
    case ITEMTYPE_SHIELD:
        //fprintf(file, "<shield");
        stream << "<shield";
        break;
    case ITEMTYPE_ARMOUR:
        //fprintf(file, "<armour");
        stream << "<armour";
        break;
    case ITEMTYPE_RING:
        //fprintf(file, "<ring");
        stream << "<ring";
        break;
    case ITEMTYPE_AMMO:
        //fprintf(file, "<ammo");
        stream << "<ammo";
        break;
    case ITEMTYPE_CURRENCY:
        //fprintf(file, "<currency");
        stream << "<currency";
        break;
    }
    //fprintf(file, " name=\"%s\"", item->getKey().c_str()); // n.b., we use the key, not the name, as the latter may be overloaded to give more descriptive names
    stream << " name=\"" << item->getKey().c_str() << "\""; // n.b., we use the key, not the name, as the latter may be overloaded to give more descriptive names
    //fprintf(file, " image_name=\"%s\"", item->getImageName().c_str());
    stream << " image_name=\"" << item->getImageName().c_str() << "\"";
    //fprintf(file, " x=\"%f\" y=\"%f\"", item->getX(), item->getY());
    stream << " x=\"" << item->getX() << "\" y=\"" << item->getY() << "\"";
    //fprintf(file, " icon_width=\"%f\"", item->getIconWidth());
    stream << " icon_width=\"" << item->getIconWidth() << "\"";
    //fprintf(file, " weight=\"%d\"", item->getWeight());
    stream << " weight=\"" << item->getWeight() << "\"";
    if( item->getUse().length() > 0 ) {
        //fprintf(file, " use=\"%s\"", item->getUse().c_str());
        stream << " use=\"" << item->getUse().c_str() << "\"";
        //fprintf(file, " use_verb=\"%s\"", item->getUseVerb().c_str());
        stream << " use_verb=\"" << item->getUseVerb().c_str() << "\"";
    }
    if( item->getArg1() != 0 ) {
        //fprintf(file, " arg1=\"%d\"", item->getArg1());
        stream << " arg1=\"" << item->getArg1() << "\"";
    }
    if( item->getArg2() != 0 ) {
        //fprintf(file, " arg2=\"%d\"", item->getArg2());
        stream << " arg2=\"" << item->getArg2() << "\"";
    }
    if( item->getArg1s().length() > 0 ) {
        //fprintf(file, " arg1_s=\"%s\"", item->getArg1s().c_str());
        stream << " arg1_s=\"" << item->getArg1s().c_str() << "\"";
    }
    //fprintf(file, " rating=\"%d\"", item->getRating()); // n.b., should always explicitly save, due to hack with default rating being 1, not 0
    stream << " rating=\"" << item->getRating() << "\""; // n.b., should always explicitly save, due to hack with default rating being 1, not 0
    if( item->isMagical() ) {
        //fprintf(file, " magical=\"true\"");
        stream << " magical=\"true\"";
    }
    if( item->getBaseTemplate().length() > 0 ) {
        //fprintf(file, " base_template=\"%s\"", item->getBaseTemplate().c_str());
        stream << " base_template=\"" << item->getBaseTemplate().c_str() << "\"";
    }
    if( item->getWorthBonus() > 0 ) {
        //fprintf(file, " worth_bonus=\"%d\"", item->getWorthBonus());
        stream << " worth_bonus=\"" << item->getWorthBonus() << "\"";
    }
    this->saveItemProfileBonusInt(stream, item, profile_key_FP_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_BS_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_S_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_A_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_M_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_D_c);
    this->saveItemProfileBonusInt(stream, item, profile_key_B_c);
    this->saveItemProfileBonusFloat(stream, item, profile_key_Sp_c);
    // now do subclasses
    switch( item->getType() ) {
    case ITEMTYPE_GENERAL:
        break;
    case ITEMTYPE_WEAPON:
    {
        const Weapon *weapon = static_cast<const Weapon *>(item);
        //fprintf(file, " animation_name=\"%s\"", weapon->getAnimationName().c_str());
        stream << " animation_name=\"" << weapon->getAnimationName().c_str() << "\"";
        if( weapon->isTwoHanded() ) {
            //fprintf(file, " two_handed=\"true\"");
            stream << " two_handed=\"true\"";
        }
        if( weapon->getWeaponType() == Weapon::WEAPONTYPE_RANGED ) {
            //fprintf(file, " ranged=\"true\"");
            stream << " ranged=\"true\"";
        }
        else if( weapon->getWeaponType() == Weapon::WEAPONTYPE_THROWN ) {
            //fprintf(file, " thrown=\"true\"");
            stream << " thrown=\"true\"";
        }
        int damageX = 0, damageY = 0, damageZ = 0;
        weapon->getDamage(&damageX, &damageY, &damageZ);
        //fprintf(file, " damageX=\"%d\"", damageX);
        //fprintf(file, " damageY=\"%d\"", damageY);
        //fprintf(file, " damageZ=\"%d\"", damageZ);
        stream << " damageX=\"" << damageX << "\"";
        stream << " damageY=\"" << damageY << "\"";
        stream << " damageZ=\"" << damageZ << "\"";
        if( weapon->getMinStrength() > 0 ) {
            //fprintf(file, " min_strength=\"%d\"", weapon->getMinStrength());
            stream << " min_strength=\"" << weapon->getMinStrength() << "\"";
        }
        if( weapon->isUnholyOnly() ) {
            //fprintf(file, " unholy_only=\"true\"");
            stream << " unholy_only=\"true\"";
        }
        if( weapon->getRequiresAmmo() ) {
            //fprintf(file, " ammo=\"%s\"", weapon->getAmmoKey().c_str());
            stream << " ammo=\"" << weapon->getAmmoKey().c_str() << "\"";
        }
        if( character != NULL && weapon == character->getCurrentWeapon() ) {
            //fprintf(file, " current_weapon=\"true\"");
            stream << " current_weapon=\"true\"";
        }
        break;
    }
    case ITEMTYPE_SHIELD:
    {
        const Shield *shield = static_cast<const Shield *>(item);
        //fprintf(file, " animation_name=\"%s\"", shield->getAnimationName().c_str());
        stream << " animation_name=\"" << shield->getAnimationName().c_str() << "\"";
        if( character != NULL && shield == character->getCurrentShield() ) {
            //fprintf(file, " current_shield=\"true\"");
            stream << " current_shield=\"true\"";
        }
        break;
    }
    case ITEMTYPE_ARMOUR:
    {
        const Armour *armour = static_cast<const Armour *>(item);
        if( armour->getMinStrength() > 0 ) {
            //fprintf(file, " min_strength=\"%d\"", armour->getMinStrength());
            stream << " min_strength=\"" << armour->getMinStrength() << "\"";
        }
        if( character != NULL && armour == character->getCurrentArmour() ) {
            //fprintf(file, " current_armour=\"true\"");
            stream << " current_armour=\"true\"";
        }
        break;
    }
    case ITEMTYPE_RING:
    {
        const Ring *ring = static_cast<const Ring *>(item);
        if( character != NULL && ring == character->getCurrentRing() ) {
            //fprintf(file, " current_ring=\"true\"");
            stream << " current_ring=\"true\"";
        }
        break;
    }
    case ITEMTYPE_AMMO:
    {
        const Ammo *ammo = static_cast<const Ammo *>(item);
        stream << " ammo_type=\"" << ammo->getAmmoType().c_str() << "\"";
        //fprintf(file, " projectile_image_name=\"%s\"", ammo->getProjectileImageName().c_str());
        stream << " projectile_image_name=\"" << ammo->getProjectileImageName().c_str() << "\"";
        //fprintf(file, " amount=\"%d\"", ammo->getAmount());
        stream << " amount=\"" << ammo->getAmount() << "\"";
        if( character != NULL && ammo == character->getCurrentAmmo() ) {
            stream << " current_ammo=\"true\"";
        }
        break;
    }
    case ITEMTYPE_CURRENCY:
    {
        const Currency *currency = static_cast<const Currency *>(item);
        //fprintf(file, " value=\"%d\"", currency->getValue());
        stream << " value=\"" << currency->getValue() << "\"";
        break;

    }
    }
    //fprintf(file, ">");
    stream << ">";
    //fprintf(file, "%s", item->getDescription().c_str());
    stream << item->getDescription().c_str();
    switch( item->getType() ) {
    case ITEMTYPE_GENERAL:
        //fprintf(file, "</item>");
        stream << "</item>";
        break;
    case ITEMTYPE_WEAPON:
        //fprintf(file, "</weapon>");
        stream << "</weapon>";
        break;
    case ITEMTYPE_SHIELD:
        //fprintf(file, "</shield>");
        stream << "</shield>";
        break;
    case ITEMTYPE_ARMOUR:
        //fprintf(file, "</armour>");
        stream << "</armour>";
        break;
    case ITEMTYPE_RING:
        //fprintf(file, "</ring>");
        stream << "</ring>";
        break;
    case ITEMTYPE_AMMO:
        //fprintf(file, "</ammo>");
        stream << "</ammo>";
        break;
    case ITEMTYPE_CURRENCY:
        //fprintf(file, "</currency>");
        stream << "</currency>";
        break;
    }
    //fprintf(file, "\n");
    stream << "\n";
}

void PlayingGamestate::saveTrap(QTextStream &stream, const Trap *trap) const {
    //fprintf(file, "<trap");
    //fprintf(file, " type=\"%s\"", trap->getType().c_str());
    //fprintf(file, " x=\"%f\" y=\"%f\"", trap->getX(), trap->getY());
    //fprintf(file, " w=\"%f\" h=\"%f\"", trap->getWidth(), trap->getHeight());
    //fprintf(file, " difficulty=\"%d\"", trap->getDifficulty());
    //fprintf(file, " rating=\"%d\"", trap->getRating());
    //fprintf(file, " />\n");
    stream << "<trap type=\"" << trap->getType().c_str() << "\" x=\"" << trap->getX() << "\" y=\"" << trap->getY() << "\"";
    stream << " w=\"" << trap->getWidth() << "\" h=\"" << trap->getHeight() << "\" difficulty=\"" << trap->getDifficulty() << "\"";
    stream << " rating=\"" << trap->getRating() << "\"";
    stream << " />\n";
}

bool PlayingGamestate::saveGame(const QString &filename, bool already_fullpath) {
    LOG("PlayingGamestate::saveGame(%s)\n", filename.toUtf8().data());
    QString full_path;
    if( already_fullpath )
        full_path = filename;
    else
        full_path = game_g->getApplicationFilename(savegame_folder + filename);
    LOG("full path: %s\n", full_path.toUtf8().data());

    /*FILE *file = fopen(full_path.toUtf8().data(), "wt");
    if( file == NULL ) {
        LOG("failed to create file\n");
        return false;
    }*/
    QFile file(full_path);
    if( !file.open(QIODevice::WriteOnly | QIODevice::Text) ) {
        LOG("failed to create file\n");
        return false;
    }
    QTextStream stream(&file);

    game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);

    const int savegame_version = 2;

    //fprintf(file, "<?xml version=\"1.0\" ?>\n");
    //fprintf(file, "<savegame major=\"%d\" minor=\"%d\" savegame_version=\"%d\">\n", versionMajor, versionMinor, savegame_version);
    //fprintf(file, "\n");
    //fprintf(file, "<game difficulty=\"%s\" permadeath=\"%s\"/>", Game::getDifficultyString(difficulty).c_str(), permadeath ? "true" : "false");
    //fprintf(file, "<current_quest name=\"%s\"/>\n", this->quest_list.at(this->c_quest_indx).getFilename().c_str());
    //fprintf(file, "\n");
    stream << "<?xml version=\"1.0\" ?>\n";
    stream << "<savegame major=\"" << versionMajor << "\" minor=\"" << versionMinor << "\" savegame_version=\"" << savegame_version << "\">\n";
    stream << "\n";
    stream << "<game difficulty=\"" << Game::getDifficultyString(difficulty).c_str() << "\" permadeath=\"" << (permadeath ? "true" : "false") << "\"/>";
    stream << "<current_quest name=\"" << this->quest_list.at(this->c_quest_indx).getFilename().c_str() << "\"/>\n";
    stream << "\n";

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
        //fprintf(file, "<location name=\"%s\" type=\"%s\" lighting_min=\"%d\">\n\n", location->getName().c_str(), type_str.c_str(), location->getLightingMin());
        stream << "<location name=\"" << location->getName().c_str() << "\" type=\"" << type_str.c_str() << "\" lighting_min=\"" << static_cast<int>(location->getLightingMin()) << "\">\n\n";

        qDebug("save location images");
        //fprintf(file, "<background image_name=\"%s\"/>\n", location->getBackgroundImageName().c_str());
        stream << "<background image_name=\"" << location->getBackgroundImageName().c_str() << "\"/>\n";
        //fprintf(file, "<floor image_name=\"%s\"/>\n", location->getFloorImageName().c_str());
        stream << "<floor image_name=\"" << location->getFloorImageName().c_str() << "\"/>\n";
        if( location->getWallImageName().length() > 0 ) {
            //fprintf(file, "<wall image_name=\"%s\" image_x_scale=\"%f\"/>\n", location->getWallImageName().c_str(), location->getWallXScale());
            stream << "<wall image_name=\"" << location->getWallImageName().c_str() << "\" image_x_scale=\"" << location->getWallXScale() << "\"/>\n";
        }
        if( location->getDropWallImageName().length() > 0 ) {
            //fprintf(file, "<dropwall image_name=\"%s\"/>\n", location->getDropWallImageName().c_str());
            stream << "<dropwall image_name=\"" << location->getDropWallImageName().c_str() << "\"/>\n";
        }
        if( location->getWanderingMonsterTemplate().length() > 0 ) {
            //fprintf(file, "<wandering_monster template=\"%s\" time=\"%d\" rest_chance=\"%d\"/>\n", location->getWanderingMonsterTemplate().c_str(), location->getWanderingMonsterTimeMS(), location->getWanderingMonsterRestChance());
            stream << "<wandering_monster template=\"" << location->getWanderingMonsterTemplate().c_str() << "\" time=\"" << location->getWanderingMonsterTimeMS() << "\" rest_chance=\"" << location->getWanderingMonsterRestChance() << "\"/>\n";
        }
        //fprintf(file, "\n");
        stream << "\n";

        for(size_t i=0;i<location->getNFloorRegions();i++) {
            const FloorRegion *floor_region = location->getFloorRegion(i);
            //fprintf(file, "<floorregion shape=\"polygon\" visible=\"%s\">\n", floor_region->isVisible() ? "true" : "false");
            stream << "<floorregion shape=\"polygon\" visible=\"" << (floor_region->isVisible() ? "true" : "false") << "\">\n";
            for(size_t j=0;j<floor_region->getNPoints();j++) {
                Vector2D point = floor_region->getPoint(j);
                //fprintf(file, "    <floorregion_point x=\"%f\" y=\"%f\"/>\n", point.x, point.y);
                stream << "    <floorregion_point x=\"" << point.x << "\" y=\"" << point.y << "\"/>\n";
            }
            //fprintf(file, "</floorregion>\n");
            stream << "</floorregion>\n";
        }
        //fprintf(file, "\n");
        stream << "\n";

        for(size_t i=0;i<c_location->getNTilemaps();i++) {
            const Tilemap *tilemap = c_location->getTilemap(i);
            //fprintf(file, "<tilemap imagemap=\"%s\" tile_width=\"%d\" tile_height=\"%d\" x=\"%f\" y=\"%f\">\n", tilemap->getImagemap().c_str(), tilemap->getTileWidth(), tilemap->getTileHeight(), tilemap->getX(), tilemap->getY());
            stream << "<tilemap imagemap=\"" << tilemap->getImagemap().c_str() << "\" tile_width=\"" << tilemap->getTileWidth() << "\" tile_height=\"" << tilemap->getTileHeight() << "\" x=\"" << tilemap->getX() << "\" y=\"" << tilemap->getY() << "\">\n";
            for(int y=0;y<tilemap->getHeighti();y++) {
                for(int x=0;x<tilemap->getWidthi();x++) {
                    char ch = tilemap->getTileAt(x, y);
                    //fprintf(file, "%c", ch);
                    stream << ch;
                }
                //fprintf(file, "\n");
                stream << "\n";
            }
            //fprintf(file, "</tilemap>\n");
            stream << "</tilemap>\n";
        }
        //fprintf(file, "\n");
        stream << "\n";

        qDebug("save player and npcs");
        for(set<Character *>::const_iterator iter = location->charactersBegin(); iter != location->charactersEnd(); ++iter) {
            const Character *character = *iter;
            if( player == character ) {
                //fprintf(file, "<player");
                stream << "<player";
            }
            else {
                //fprintf(file, "<npc");
                //fprintf(file, " is_hostile=\"%s\"", character->isHostile() ? "true": "false");
                //fprintf(file, " animation_name=\"%s\"", character->getAnimationName().c_str());
                stream << "<npc is_hostile=\"" << (character->isHostile() ? "true": "false") << "\" animation_name=\"" << character->getAnimationName().c_str() << "\"";
            }
            if( character->isStaticImage() ) {
                //fprintf(file, " static_image=\"%s\"", character->isStaticImage() ? "true": "false");
                stream << " static_image=\"" << (character->isStaticImage() ? "true": "false") << "\"";
            }
            if( character->getPortrait().length() > 0 ) {
                //fprintf(file, " portrait=\"%s\"", character->getPortrait().c_str());
                stream << " portrait=\"" << character->getPortrait().c_str() << "\"";
            }
            if( character->isBounce() ) {
                //fprintf(file, " bounce=\"true\"");
                stream << " bounce=\"true\"";
            }
            //fprintf(file, " name=\"%s\"", character->getName().c_str());
            stream << " name=\"" << character->getName().c_str() << "\"";
            if( character->isDead() ) {
                //fprintf(file, " is_dead=\"true\"");
                stream << " is_dead=\"true\"";
            }
            //fprintf(file, " x=\"%f\" y=\"%f\"", character->getX(), character->getY());
            stream << " x=\"" << character->getX() << "\" y=\"" << character->getY() << "\"";
            if( character->hasDefaultPosition() ) {
                //fprintf(file, " default_x=\"%f\" default_y=\"%f\"", character->getDefaultX(), character->getDefaultY());
                stream << " default_x=\"" << character->getDefaultX() << "\" default_y=\"" << character->getDefaultY() << "\"";
            }
            //fprintf(file, " health=\"%d\"", character->getHealth());
            stream << " health=\"" << character->getHealth() << "\"";
            //fprintf(file, " max_health=\"%d\"", character->getMaxHealth());
            stream << " max_health=\"" << character->getMaxHealth() << "\"";
            for(map<string, int>::const_iterator iter = character->getBaseProfile()->intPropertiesBegin(); iter != character->getBaseProfile()->intPropertiesEnd(); ++iter) {
                string key = iter->first;
                int value = iter->second;
                //fprintf(file, " %s=\"%d\"", key.c_str(), value);
                stream << " " << key.c_str() << "=\"" << value << "\"";
            }
            for(map<string, float>::const_iterator iter = character->getBaseProfile()->floatPropertiesBegin(); iter != character->getBaseProfile()->floatPropertiesEnd(); ++iter) {
                string key = iter->first;
                float value = iter->second;
                //fprintf(file, " %s=\"%f\"", key.c_str(), value);
                stream << " " << key.c_str() << "=\"" << value << "\"";
            }
            if( character == this->getPlayer() ) {
                // only care about initial stats for player for now
                stream << " initial_level=\"" << character->getInitialLevel() << "\"";
                for(map<string, int>::const_iterator iter = character->getInitialBaseProfile()->intPropertiesBegin(); iter != character->getInitialBaseProfile()->intPropertiesEnd(); ++iter) {
                    string key = iter->first;
                    int value = iter->second;
                    stream << " initial_" << key.c_str() << "=\"" << value << "\"";
                }
                for(map<string, float>::const_iterator iter = character->getInitialBaseProfile()->floatPropertiesBegin(); iter != character->getInitialBaseProfile()->floatPropertiesEnd(); ++iter) {
                    string key = iter->first;
                    float value = iter->second;
                    stream << " initial_" << key.c_str() << "=\"" << value << "\"";
                }
            }
            int natural_damageX = 0, natural_damageY = 0, natural_damageZ = 0;
            character->getNaturalDamage(&natural_damageX, &natural_damageY, &natural_damageZ);

            //fprintf(file, " natural_damageX=\"%d\"", natural_damageX);
            //fprintf(file, " natural_damageY=\"%d\"", natural_damageY);
            //fprintf(file, " natural_damageZ=\"%d\"", natural_damageZ);
            stream << " natural_damageX=\"" << natural_damageX << "\"";
            stream << " natural_damageY=\"" << natural_damageY << "\"";
            stream << " natural_damageZ=\"" << natural_damageZ << "\"";
            if( character->canFly() ) {
                //fprintf(file, " can_fly=\"true\"");
                stream << " can_fly=\"true\"";
            }
            if( character->isParalysed() ) {
                //fprintf(file, " is_paralysed=\"true\"");
                stream << " is_paralysed=\"true\"";
                //fprintf(file, " paralysed_time=\"%d\"", character->getParalysedUntil() - game_g->getScreen()->getGameTimeTotalMS());
                stream << " paralysed_time=\"" << (character->getParalysedUntil() - game_g->getScreen()->getGameTimeTotalMS()) << "\"";
            }
            if( character->isDiseased() ) {
                //fprintf(file, " is_diseased=\"true\"");
                stream << " is_diseased=\"true\"";
            }
            //fprintf(file, " level=\"%d\"", character->getLevel());
            stream << " level=\"" << character->getLevel() << "\"";
            //fprintf(file, " xp=\"%d\"", character->getXP());
            stream << " xp=\"" << character->getXP() << "\"";
            //fprintf(file, " xp_worth=\"%d\"", character->getXPWorth());
            stream << " xp_worth=\"" << character->getXPWorth() << "\"";
            if( character->getCausesTerror() ) {
                //fprintf(file, " causes_terror=\"true\"");
                stream << " causes_terror=\"true\"";
                //fprintf(file, " terror_effect=\"%d\"", character->getTerrorEffect());
                stream << " terror_effect=\"" << character->getTerrorEffect() << "\"";
            }
            if( character->hasDoneTerror() ) {
                //fprintf(file, " done_terror=\"%s\"", character->hasDoneTerror() ? "true" : "false");
                stream << " done_terror=\"true\"";
            }
            if( character->isFleeing() ) {
                stream << " is_fleeing=\"true\"";
            }
            if( character->getCausesDisease() > 0 ) {
                //fprintf(file, " causes_disease=\"%d\"", character->getCausesDisease());
                stream << " causes_disease=\"" << character->getCausesDisease() << "\"";
            }
            if( character->getCausesParalysis() > 0 ) {
                //fprintf(file, " causes_paralysis=\"%d\"", character->getCausesParalysis());
                stream << " causes_paralysis=\"" << character->getCausesParalysis() << "\"";
            }
            if( character->requiresMagical() ) {
                //fprintf(file, " requires_magical=\"true\"");
                stream << " requires_magical=\"true\"";
            }
            if( character->isUnholy() ) {
                //fprintf(file, " unholy=\"true\"");
                stream << " unholy=\"true\"";
            }
            //fprintf(file, " gold=\"%d\"", character->getGold());
            stream << " gold=\"" << character->getGold() << "\"";
            if( character->canTalk() ) {
                //fprintf(file, " can_talk=\"true\"");
                stream << " can_talk=\"true\"";
            }
            if( character->hasTalked() ) {
                //fprintf(file, " has_talked=\"true\"");
                stream << " has_talked=\"true\"";
            }
            if( character->getInteractionType().length() > 0 ) {
                //fprintf(file, " interaction_type=\"%s\"", character->getInteractionType().c_str());
                stream << " interaction_type=\"" << character->getInteractionType().c_str() << "\"";
            }
            if( character->getInteractionData().length() > 0 ) {
                //fprintf(file, " interaction_data=\"%s\"", character->getInteractionData().c_str());
                stream << " interaction_data=\"" << character->getInteractionData().c_str() << "\"";
            }
            if( character->getInteractionJournal().length() > 0 ) {
                stream << " interaction_journal=\"" << character->getInteractionJournal().c_str() << "\"";
            }
            if( character->getInteractionXP() != 0 ) {
                //fprintf(file, " interaction_xp=\"%d\"", character->getInteractionXP());
                stream << " interaction_xp=\"" << character->getInteractionXP() << "\"";
            }
            if( character->getInteractionRewardItem().length() > 0 ) {
                //fprintf(file, " interaction_reward_item=\"%s\"", character->getInteractionRewardItem().c_str());
                stream << " interaction_reward_item=\"" << character->getInteractionRewardItem().c_str() << "\"";
            }
            if( character->isInteractionCompleted() ) {
                //fprintf(file, " interaction_completed=\"true\"");
                stream << " interaction_completed=\"true\"";
            }
            if( character->getShop().length() > 0 ) {
                //fprintf(file, " shop=\"%s\"", character->getShop().c_str());
                stream << " shop=\"" << character->getShop().c_str() << "\"";
            }
            if( character->getObjectiveId().length() > 0 ) {
                //fprintf(file, " objective_id=\"%s\"", character->getObjectiveId().c_str());
                stream << " objective_id=\"" << character->getObjectiveId().c_str() << "\"";
            }
            //fprintf(file, ">\n");
            stream << ">\n";
            if( character->getTalkOpeningInitial().length() > 0 ) {
                //fprintf(file, "<opening_initial>%s</opening_initial>", character->getTalkOpeningInitial().c_str());
                stream << "<opening_initial>" << character->getTalkOpeningInitial().c_str() << "</opening_initial>";
            }
            if( character->getTalkOpeningLater().length() > 0 ) {
                //fprintf(file, "<opening_later>%s</opening_later>", character->getTalkOpeningLater().c_str());
                stream << "<opening_later>" << character->getTalkOpeningLater().c_str() << "</opening_later>";
            }
            if( character->getTalkOpeningInteractionComplete().length() > 0 ) {
                //fprintf(file, "<opening_interaction_complete>%s</opening_interaction_complete>", character->getTalkOpeningInteractionComplete().c_str());
                stream << "<opening_interaction_complete>" << character->getTalkOpeningInteractionComplete().c_str() << "</opening_interaction_complete>";
            }
            for(vector<TalkItem>::const_iterator iter2 = character->talkItemsBegin(); iter2 != character->talkItemsEnd(); ++iter2) {
                const TalkItem *talk_item = &*iter2;
                //fprintf(file, "<talk");
                //fprintf(file, " question=\"%s\"", talk_item->question.c_str());
                //fprintf(file, " action=\"%s\"", talk_item->action.c_str());
                //fprintf(file, " while_not_done=\"%s\"", talk_item->while_not_done ? "true": "false");
                //fprintf(file, " objective=\"%s\"", talk_item->objective ? "true": "false");
                //fprintf(file, ">\n");
                //fprintf(file, "%s\n", talk_item->answer.c_str());
                //fprintf(file, "</talk>\n");
                stream << "<talk question=\"" << talk_item->question.c_str() << "\"";
                if( talk_item->action.length() > 0 ) {
                    stream << " action=\"" << talk_item->action.c_str() << "\"";
                }
                if( talk_item->journal.length() > 0 ) {
                    stream << " journal=\"" << talk_item->journal.c_str() << "\"";
                }
                stream << " while_not_done=\"" << (talk_item->while_not_done ? "true": "false") << "\"";
                stream << " objective=\"" << (talk_item->objective ? "true": "false") << "\"";
                stream << ">\n";
                stream << talk_item->answer.c_str() << "\n";
                stream << "</talk>\n";
            }
            for(map<string, int>::const_iterator iter2 = character->spellsBegin(); iter2 != character->spellsEnd(); ++iter2) {
                string spell_name = iter2->first;
                int spell_count = iter2->second;
                //fprintf(file, "<spell name=\"%s\" count=\"%d\"/>\n", spell_name.c_str(), spell_count);
                stream << "<spell name=\"" << spell_name.c_str() << "\" count=\"" << spell_count << "\"/>\n";
            }
            for(set<Item *>::const_iterator iter2 = character->itemsBegin(); iter2 != character->itemsEnd(); ++iter2) {
                const Item *item = *iter2;
                this->saveItem(stream, item, character);
            }
            if( player == character ) {
                //fprintf(file, "</player>\n");
                stream << "</player>\n";
            }
            else {
                //fprintf(file, "</npc>\n");
                stream << "</npc>\n";
            }
        }
        //fprintf(file, "\n");
        stream << "\n";

        if( location == c_location ) {
            qDebug("save player additional info");
            //fprintf(file, "<player_start x=\"%f\" y=\"%f\"/>\n", player->getX(), player->getY());
            //fprintf(file, "\n");
            stream << "<player_start x=\"" << player->getX() << "\" y=\"" << player->getY() << "\"/>\n\n";
        }

        qDebug("save scenery");
        for(set<Scenery *>::const_iterator iter = location->scenerysBegin(); iter != location->scenerysEnd(); ++iter) {
            const Scenery *scenery = *iter;
            //fprintf(file, "<scenery");
            stream << "<scenery";
            //fprintf(file, " name=\"%s\"", scenery->getName().c_str());
            stream << " name=\"" << scenery->getName().c_str() << "\"";
            //fprintf(file, " image_name=\"%s\"", scenery->getImageName().c_str());
            stream << " image_name=\"" << scenery->getImageName().c_str() << "\"";
            if( scenery->getBigImageName().length() > 0 ) {
                //fprintf(file, " big_image_name=\"%s\"", scenery->getBigImageName().c_str());
                stream << " big_image_name=\"" << scenery->getBigImageName().c_str() << "\"";
            }
            //fprintf(file, " x=\"%f\" y=\"%f\"", scenery->getX(), scenery->getY());
            stream << " x=\"" << scenery->getX() << "\" y=\"" << scenery->getY() << "\"";
            //fprintf(file, " w=\"%f\" h=\"%f\" visual_h=\"%f\"", scenery->getWidth(), scenery->getHeight(), scenery->getVisualHeight());
            stream << " w=\"" << scenery->getWidth() << "\" h=\"" << scenery->getHeight() << "\" visual_h=\"" << scenery->getVisualHeight() << "\"";
            //fprintf(file, " opacity=\"%f\"", scenery->getOpacity());
            stream << " opacity=\"" << scenery->getOpacity() << "\"";
            switch( scenery->getDrawType() ) {
            case Scenery::DRAWTYPE_NORMAL:
                break;
            case Scenery::DRAWTYPE_FLOATING:
                //fprintf(file, " draw_type=\"floating\"");
                stream << " draw_type=\"floating\"";
                break;
            case Scenery::DRAWTYPE_BACKGROUND:
                //fprintf(file, " draw_type=\"background\"");
                stream << " draw_type=\"background\"";
                break;
            default:
                ASSERT_LOGGER(false);
                break;
            }
            //fprintf(file, " action_last_time=\"%d\"", scenery->getActionLastTime());
            //fprintf(file, " action_delay=\"%d\"", scenery->getActionDelay());
            //fprintf(file, " action_type=\"%s\"", scenery->getActionType().c_str());
            //fprintf(file, " action_value=\"%d\"", scenery->getActionValue());
            //fprintf(file, " interact_type=\"%s\"", scenery->getInteractType().c_str());
            //fprintf(file, " interact_state=\"%d\"", scenery->getInteractState());
            stream << " action_last_time=\"" << scenery->getActionLastTime() << "\" action_delay=\"" << scenery->getActionDelay() << "\"";
            stream << " action_type=\"" << scenery->getActionType().c_str() << "\"";
            stream << " action_value=\"" << scenery->getActionValue() << "\"";
            stream << " interact_type=\"" << scenery->getInteractType().c_str() << "\"";
            stream << " interact_state=\"" << scenery->getInteractState() << "\"";
            if( scenery->isBlocking() ) {
                //fprintf(file, " blocking=\"true\"");
                stream << " blocking=\"true\"";
            }
            if( scenery->blocksVisibility() ) {
                //fprintf(file, " block_visibility=\"true\"");
                stream << " block_visibility=\"true\"";
            }
            if( scenery->hasSmoke() ) {
                //fprintf(file, " has_smoke=\"true\"");
                //fprintf(file, " smoke_x=\"%f\"", scenery->getSmokePos().x);
                //fprintf(file, " smoke_y=\"%f\"", scenery->getSmokePos().y);
                stream << " has_smoke=\"true\" smoke_x=\"" << scenery->getSmokePos().x << "\" smoke_y=\"" << scenery->getSmokePos().y << "\"";
            }
            if( scenery->isOpened() ) {
                //fprintf(file, " is_opened=\"true\"");
                stream << " is_opened=\"true\"";
            }
            if( scenery->isExit() ) {
                //fprintf(file, " exit=\"true\"");
                stream << " exit=\"true\"";
            }
            if( scenery->isDoor() ) {
                //fprintf(file, " door=\"true\"");
                stream << " door=\"true\"";
            }
            if( scenery->getExitLocation().length() > 0 ) {
                //fprintf(file, " exit_location=\"%s\" exit_location_x=\"%f\" exit_location_y=\"%f\"", scenery->getExitLocation().c_str(), scenery->getExitLocationPos().x, scenery->getExitLocationPos().y);
                stream << " exit_location=\"" << scenery->getExitLocation().c_str() << "\" exit_location_x=\"" << scenery->getExitLocationPos().x << "\" exit_location_y=\"" << scenery->getExitLocationPos().y << "\"";
            }
            if( scenery->isLocked() ) {
                //fprintf(file, " locked=\"true\"");
                stream << " locked=\"true\"";
            }
            if( scenery->isLockedSilent() ) {
                //fprintf(file, " locked_silent=\"true\"");
                stream << " locked_silent=\"true\"";
            }
            if( scenery->getLockedText().length() > 0 ) {
                //fprintf(file, " locked_text=\"%s\"", scenery->getLockedText().c_str());
                stream << " locked_text=\"" << scenery->getLockedText().c_str() << "\"";
            }
            if( scenery->isLockedUsedUp() ) {
                //fprintf(file, " locked_used_up=\"true\"");
                stream << " locked_used_up=\"true\"";
            }
            if( scenery->isKeyAlwaysNeeded() ) {
                //fprintf(file, " key_always_needed=\"true\"");
                stream << " key_always_needed=\"true\"";
            }
            if( scenery->getUnlockItemName().length() > 0 ) {
                //fprintf(file, " unlocked_by_template=\"%s\"", scenery->getUnlockItemName().c_str());
                stream << " unlocked_by_template=\"" << scenery->getUnlockItemName().c_str() << "\"";
            }
            if( scenery->getUnlockText().length() > 0 ) {
                //fprintf(file, " unlock_text=\"%s\"", scenery->getUnlockText().c_str());
                stream << " unlock_text=\"" << scenery->getUnlockText().c_str() << "\"";
            }
            if( scenery->getUnlockXP() > 0 ) {
                //fprintf(file, " unlock_xp=\"%d\"", scenery->getUnlockXP());
                stream << " unlock_xp=\"" << scenery->getUnlockXP() << "\"";
            }
            if( scenery->getConfirmText().length() > 0 ) {
                //fprintf(file, " confirm_text=\"%s\"", scenery->getConfirmText().c_str());
                stream << " confirm_text=\"" << scenery->getConfirmText().c_str() << "\"";
            }
            //fprintf(file, ">");
            stream << ">";
            for(set<Item *>::const_iterator iter2 = scenery->itemsBegin(); iter2 != scenery->itemsEnd(); ++iter2) {
                const Item *item = *iter2;
                this->saveItem(stream, item);
            }
            if( scenery->getTrap() != NULL ) {
                this->saveTrap(stream, scenery->getTrap());
            }
            if( scenery->getDescription().length() > 0 ) {
                //fprintf(file, "%s", scenery->getDescription().c_str());
                stream << scenery->getDescription().c_str();
            }
            //fprintf(file, "</scenery>\n");
            stream << "</scenery>\n";
        }
        //fprintf(file, "\n");
        stream << "\n";

        qDebug("save items");
        for(set<Item *>::const_iterator iter = location->itemsBegin(); iter != location->itemsEnd(); ++iter) {
            const Item *item = *iter;
            this->saveItem(stream, item);
        }
        //fprintf(file, "\n");
        stream << "\n";

        qDebug("save traps");
        for(set<Trap *>::const_iterator iter = location->trapsBegin(); iter != location->trapsEnd(); ++iter) {
            const Trap *trap = *iter;
            this->saveTrap(stream, trap);
        }
        //fprintf(file, "\n");
        stream << "\n";

        //fprintf(file, "</location>\n\n");
        stream << "</location>\n\n";
    }

    const QuestObjective *quest_objective = this->getQuest()->getQuestObjective();
    if( quest_objective != NULL ) {
        qDebug("save quest objective");
        //fprintf(file, "<quest_objective");
        //fprintf(file, " type=\"%s\"", quest_objective->getType().c_str());
        //fprintf(file, " arg1=\"%s\"", quest_objective->getArg1().c_str());
        //fprintf(file, " gold=\"%d\"", quest_objective->getGold());
        //fprintf(file, " />");
        stream << "<quest_objective type=\"" << quest_objective->getType().c_str() << "\"";
        stream << " arg1=\"" << quest_objective->getArg1().c_str() << "\"";
        stream << " gold=\"" << quest_objective->getGold() << "\"";
        stream << " />";
    }
    //fprintf(file, "\n");
    stream << "\n";

    qDebug("save quest completed text");
    //fprintf(file, "<completed_text>%s</completed_text>\n", this->getQuest()->getCompletedText().c_str());
    //fprintf(file, "\n");
    stream << "<completed_text>" << this->getQuest()->getCompletedText().c_str() << "</completed_text>\n\n";

    qDebug("save current quest info");
    //fprintf(file, "<quest_info complete=\"%s\"/>\n", this->getQuest()->isCompleted() ? "true" : "false");
    //fprintf(file, "\n");
    stream << "<quest_info complete=\"" << (this->getQuest()->isCompleted() ? "true" : "false") << "\"/>\n\n";

    qDebug("save journal");
    //fprintf(file, "<journal>\n");
    stream << "<journal>\n";
    QByteArray encoded = QUrl::toPercentEncoding(this->journal_ss.str().c_str(), QByteArray(), "<>");
    //fprintf(file, "%s", encoded.data());
    stream << encoded;
    //fprintf(file, "</journal>\n");
    //fprintf(file, "\n");
    stream << "</journal>\n\n";

    stream << "<time_hours value=\"" << this->time_hours << "\"/>";

    //fprintf(file, "</savegame>\n");
    stream << "</savegame>\n";

    //fclose(file);

    if( this->permadeath ) {
        this->permadeath_has_savefilename = true;
        this->permadeath_savefilename = full_path;
    }

    game_g->getScreen()->getMainWindow()->unsetCursor();
    return true;
}

void PlayingGamestate::addWidget(QWidget *widget, bool fullscreen_hint) {
    this->widget_stack.push_back(widget);
    /*if( mobile_c ) {
        this->main_stacked_widget->addWidget(widget);
        this->main_stacked_widget->setCurrentWidget(widget);
    }
    else*/ {
        MainWindow *window = game_g->getScreen()->getMainWindow();
        //widget->setParent(window);
        widget->setParent(window->centralWidget());
        widget->setWindowModality(Qt::ApplicationModal);
        //widget->setWindowFlags(Qt::Dialog);
        //widget->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        if( mobile_c ) {
            // always fullscreen
            widget->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
            widget->showFullScreen();
        }
        else if( window->isFullScreen() ) {
            widget->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
            if( fullscreen_hint ) {
                widget->showFullScreen();
            }
            else {
                widget->show();
            }
        }
        else {
            // always windowed
            widget->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
            widget->show();
        }
    }
    this->view->setEnabled(false);
    this->view->releaseKeyboard();
}

void PlayingGamestate::addTextEffect(const string &text, int duration_ms) {
    QPointF pos = this->view->mapToScene(0.5f*view->width(), 0.5f*view->height());
    this->addTextEffect(text, Vector2D(pos.x(), pos.y()), duration_ms);
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
    if( this->view_transform_3d ) {
        QTransform transform;
        transform.scale(font_scale, 2.0f*font_scale);
        text_effect->setTransform(transform);
    }
    else {
        text_effect->setScale(font_scale);
    }
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

QGraphicsItem *PlayingGamestate::addPixmapGraphic(const QPixmap &pixmap, Vector2D pos, float width, bool undo_3d, bool on_ground) {
    qDebug("PlayingGamestate::addPixmapGraphic(%f, %f, %f)", pos.x, pos.y, width);
    QGraphicsPixmapItem *object = new QGraphicsPixmapItem();
    object->setPixmap(pixmap);
    object->setPos(pos.x, pos.y);
    if( on_ground ) {
        object->setZValue(z_value_gui);
    }
    else {
        object->setZValue(pos.y + 1000.0f);
    }
    this->addGraphicsItem(object, width, undo_3d);
    return object;
}

QGraphicsItem *PlayingGamestate::addSpellGraphic(Vector2D pos) {
    qDebug("PlayingGamestate::addSpellGraphic(%f, %f)", pos.x, pos.y);
    return this->addPixmapGraphic(this->fireball_pixmap, pos, 0.5f, true, false);
}

void PlayingGamestate::addStandardItem(Item *item) {
    this->standard_items[item->getKey()] = item;
}

Item *PlayingGamestate::cloneStandardItem(const string &name) const {
    const Item *item = this->getStandardItem(name);
    return item->clone();
}

const Item *PlayingGamestate::getStandardItem(const string &name) const {
    map<string, Item *>::const_iterator iter = this->standard_items.find(name);
    if( iter == this->standard_items.end() ) {
        LOG("can't find standard item which doesn't exist: %s\n", name.c_str());
        throw string("Unknown standard item");
    }
    const Item *item = iter->second;
    return item;
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

Character *PlayingGamestate::createCharacter(const string &name, const string &template_name) const {
    map<string, CharacterTemplate *>::const_iterator iter = this->character_templates.find(template_name);
    if( iter == this->character_templates.end() ) {
        LOG("can't find character_templates: %s\n", template_name.c_str());
        throw string("Unknown character_template");
    }
    const CharacterTemplate *character_template = iter->second;
    Character *character = new Character(name, true, *character_template);
    return character;
}

Currency *PlayingGamestate::cloneGoldItem(int value) const {
    Currency *item = static_cast<Currency *>(this->cloneStandardItem("Gold"));
    item->setValue(value);
    return item;
}

AnimationLayer *PlayingGamestate::getProjectileAnimationLayer(const string &name) {
    map<string, AnimationLayer *>::iterator image_iter = this->projectile_animation_layers.find(name);
    if( image_iter == this->projectile_animation_layers.end() ) {
        LOG("failed to find image for projectile: %s\n", name.c_str());
        LOG("    image name: %s\n", name.c_str());
        throw string("Failed to find projectile's image");
    }
    return image_iter->second;
}

QPixmap &PlayingGamestate::getPortraitImage(const string &name) {
    map<string, QPixmap>::iterator image_iter = this->portrait_images.find(name);
    if( image_iter == this->portrait_images.end() ) {
        LOG("failed to find image for portrait_images: %s\n", name.c_str());
        LOG("    image name: %s\n", name.c_str());
        throw string("Failed to find portrait_images's image");
    }
    return image_iter->second;
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

QString PlayingGamestate::getItemString(const Item *item, bool want_weight) const {
    QString item_str = item->getName().c_str();
    if( this->getPlayer()->getCurrentWeapon() == item ) {
        item_str += " [Armed]";
    }
    else if( this->getPlayer()->getCurrentAmmo() == item ) {
        item_str += " [Armed]";
    }
    else if( this->getPlayer()->getCurrentShield() == item ) {
        item_str += " [Armed]";
    }
    else if( this->getPlayer()->getCurrentArmour() == item ) {
        item_str += " [Worn]";
    }
    else if( this->getPlayer()->getCurrentRing() == item ) {
        item_str += " [Worn]";
    }
    if( item->getWeight() > 0 ) {
        item_str += " (Weight " + QString::number(item->getWeight()) + ")";
    }
    return item_str;
}

void PlayingGamestate::showInfoDialog(const string &message) {
    this->showInfoDialog(message, "");
}

void PlayingGamestate::showInfoDialog(const string &message, const string &picture) {
    LOG("PlayingGamestate::showInfoDialog(%s)\n", message.c_str());
    InfoDialog *dialog = InfoDialog::createInfoDialogOkay(message, picture);
    this->addWidget(dialog, false);
    dialog->exec();
    this->closeSubWindow();
}

bool PlayingGamestate::askQuestionDialog(const string &message) {
    LOG("PlayingGamestate::askQuestionDialog(%s)\n", message.c_str());
    InfoDialog *dialog = InfoDialog::createInfoDialogYesNo(message);
    this->addWidget(dialog, false);
    int result = dialog->exec();
    LOG("askQuestionDialog returns %d\n", result);
    this->closeSubWindow();
    return result == 0;
}
