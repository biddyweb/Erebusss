#include "rpg/location.h"
#include "rpg/item.h"

#include "game.h"
#include "qt_screen.h"
#include "qt_utils.h"

#include <ctime>

#include <QtWebKit/QWebView>
#include <QXmlStreamReader>

#ifdef _DEBUG
#include <cassert>
#endif

#ifdef _DEBUG
//#define DEBUG_SHOW_PATH
#endif

#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
const bool mobile_c = true;
#else
const bool mobile_c = false;
#endif

Game *game_g = NULL;
OptionsGamestate *OptionsGamestate::optionsGamestate = NULL;
PlayingGamestate *PlayingGamestate::playingGamestate = NULL;

const float MainGraphicsView::min_zoom_c = 10.0f;
const float MainGraphicsView::max_zoom_c = 200.0f;

const int versionMajor = 0;
const int versionMinor = 1;

//const float player_scale = 1.0f/32.0f; // 32 pixels for 1 metre
//const float item_scale = 1.0f/64.0f; // 64 pixels for 1 metre
//const float item_width = 0.5f;
//const float scenery_width = item_width;
//const float font_scale = 1.0f/64.0f;

//const int scene_item_character_key_c = 0;

AnimationSet::AnimationSet(AnimationType animation_type, size_t n_frames, vector<QPixmap> pixmaps) : animation_type(animation_type), n_frames(n_frames), pixmaps(pixmaps) {
    if( pixmaps.size() != N_DIRECTIONS * n_frames ) {
        LOG("AnimationSet error: pixmaps size %d, n_frames %d, N_DIRECTIONS %d\n", pixmaps.size(), n_frames, N_DIRECTIONS);
        throw string("AnimationSet has incorrect pixmaps size");
    }
}

AnimationSet::~AnimationSet() {
}

const QPixmap &AnimationSet::getFrame(Direction c_direction, size_t c_frame) const {
    //qDebug("animation type: %d", this->animation_type);
    switch( this->animation_type ) {
    case ANIMATIONTYPE_BOUNCE:
        if( n_frames == 1 )
            c_frame = 0;
        else {
            int n_total_frames = 2*n_frames-2;
            c_frame = c_frame % n_total_frames;
            if( c_frame > n_frames-1 )
                c_frame = n_total_frames-c_frame;
        }
        break;
    case ANIMATIONTYPE_SINGLE:
        //qDebug("%d : %d", c_frame, n_frames);
        if( c_frame > n_frames-1 )
            c_frame = n_frames-1;
        break;
    default:
        c_frame = c_frame % n_frames;
        break;
    }

    //qDebug("get frame %d", c_frame);
    return this->pixmaps[((int)c_direction)*n_frames + c_frame];
}

AnimationSet *AnimationSet::create(const QPixmap &image, AnimationType animation_type, int size, int x_offset, size_t n_frames) {
    vector<QPixmap> frames;
    for(int i=0;i<N_DIRECTIONS;i++) {
        for(size_t j=0;j<n_frames;j++) {
            frames.push_back( image.copy(size*(x_offset+j), size*i, size, size));
        }
    }
    AnimationSet *animation_set = new AnimationSet(animation_type, n_frames, frames);
    return animation_set;
}

AnimationLayer *AnimationLayer::create(const QPixmap &image, const vector<AnimationLayerDefinition> &animation_layer_definitions) {
    if( image.height() % N_DIRECTIONS != 0 ) {
        throw string("image height is not multiple of 8");
    }
    AnimationLayer *layer = new AnimationLayer(image.height() / N_DIRECTIONS);
    /*LOG("AnimationLayer::create: %s\n", filename);
    QPixmap image = game_g->loadImage(filename);*/
    LOG("    loaded image\n");
    for(vector<AnimationLayerDefinition>::const_iterator iter = animation_layer_definitions.begin(); iter != animation_layer_definitions.end(); ++iter) {
        const AnimationLayerDefinition animation_layer_definition = *iter;
        AnimationSet *animation_set = AnimationSet::create(image, animation_layer_definition.animation_type, layer->size, animation_layer_definition.position, animation_layer_definition.n_frames);
        layer->addAnimationSet(animation_layer_definition.name, animation_set);
    }
    LOG("    done\n");
    return layer;
}

AnimationLayer *AnimationLayer::create(string filename, const vector<AnimationLayerDefinition> &animation_layer_definitions) {
    QPixmap image = game_g->loadImage(filename.c_str());
    return create(image, animation_layer_definitions);
}

AnimatedObject::AnimatedObject() : /*animation_layer(NULL), c_animation_set(NULL),*/
    set_c_animation_name(false), c_direction(DIRECTION_E), c_frame(0), animation_time_start_ms(0)
{
    for(vector<const AnimationSet *>::const_iterator iter = c_animation_sets.begin(); iter != c_animation_sets.end(); ++iter) {
        const AnimationSet *c_animation_set = *iter;
        delete c_animation_set;
    }
}

AnimatedObject::~AnimatedObject() {
}

void AnimatedObject::advance(int phase) {
    //qDebug("AnimatedObject::advance() phase %d", phase);
    if( phase == 1 ) {
        int ms_per_frame = 100;
        //int time_elapsed_ms = game_g->getScreen()->getElapsedMS() - animation_time_start_ms;
        int time_elapsed_ms = game_g->getScreen()->getGameTimeTotalMS() - animation_time_start_ms;
        size_t n_frame = ( time_elapsed_ms / ms_per_frame );
        if( n_frame != c_frame ) {
            c_frame = n_frame;
            this->update();
        }
        this->setZValue( this->pos().y() );
    }
}

QRectF AnimatedObject::boundingRect() const {
    //qDebug("boundingRect");
    return QRectF(0.0f, 0.0f, 64.0f, 64.0f);
}

void AnimatedObject::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {

    //qDebug("paint");

    /*int ms_per_frame = 100;
    int time_elapsed_ms = game_g->getScreen()->getElapsedMS() - animation_time_start_ms;
    int c_frame = ( time_elapsed_ms / ms_per_frame );*/

    for(vector<const AnimationSet *>::const_iterator iter = c_animation_sets.begin(); iter != c_animation_sets.end(); ++iter) {
        const AnimationSet *c_animation_set = *iter;
        const QPixmap &pixmap = c_animation_set->getFrame(c_direction, c_frame);
        painter->drawPixmap(0, 0, pixmap);
        //break;
    }
}

void AnimatedObject::addAnimationLayer(AnimationLayer *animation_layer) {
    this->animation_layers.push_back(animation_layer);
    const AnimationSet *c_animation_set = animation_layer->getAnimationSet("");
    this->c_animation_sets.push_back(c_animation_set);
    //this->setFrame(0);
    this->update();
}

void AnimatedObject::clearAnimationLayers() {
    this->animation_layers.clear();
    this->c_animation_sets.clear();
}

void AnimatedObject::setAnimationSet(string name) {
    if( !this->set_c_animation_name || this->c_animation_name != name ) {
        //qDebug("set animation set: %s", name.c_str());
        this->c_animation_sets.clear();
        for(vector<AnimationLayer *>::const_iterator iter = animation_layers.begin(); iter != animation_layers.end(); ++iter) {
            const AnimationLayer *animation_layer = *iter;
            const AnimationSet *c_animation_set = animation_layer->getAnimationSet(name);
            if( c_animation_set == NULL ) {
                LOG("unknown animation set: %s\n", name.c_str());
                throw string("Unknown animation set");
            }
            this->c_animation_sets.push_back(c_animation_set);
        }
    }
    else {
        // useful as a warning, in case we didn't intend to do this
        qDebug("reset animation set: %s", name.c_str());
    }

    animation_time_start_ms = 0;
    //this->setFrame(0);
    this->c_frame = 0;
    this->update();

    this->set_c_animation_name = true;
    this->c_animation_name = name;
}

void AnimatedObject::setDirection(Direction c_direction) {
    if( this->c_direction != c_direction ) {
        this->c_direction = c_direction;
        //this->setFrame(0);
        this->update();
    }
}

int AnimatedObject::getSize() const {
    ASSERT_LOGGER( this->animation_layers.size() > 0 );
    const AnimationLayer *animation_layer = this->animation_layers.at(0);
    return animation_layer->getSize();
}

TextEffect::TextEffect(MainGraphicsView *view, const QString &text, int duration_ms) :
    QGraphicsTextItem(text), time_expire(0), view(view) {
    this->setDefaultTextColor(Qt::white);
    this->time_expire = game_g->getScreen()->getGameTimeTotalMS() + duration_ms;
    this->setFont(game_g->getFontStd());

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

OptionsGamestate::OptionsGamestate()
{
    LOG("OptionsGamestate::OptionsGamestate()\n");
    optionsGamestate = this;

    MainWindow *window = game_g->getScreen()->getMainWindow();
    QFont font = game_g->getFontBig();
    window->setFont(font);

    QWidget *centralWidget = new QWidget(window);
    centralWidget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    window->setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout();
    centralWidget->setLayout(layout);

    //QLabel *titleLabel = new QLabel("erebus");
    QLabel *titleLabel = new QLabel("");
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // needed for fix layout problem on Qt Simulator at least, when returning to the options gamestate after playing the game
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("QLabel { color : red; }");
    titleLabel->setFont(font);
    /*{
        QFont font = titleLabel->font();
        font.setPointSize( font.pointSize() + 8 );
        titleLabel->setFont(font);
    }*/
    layout->addWidget(titleLabel);

    QPushButton *startButton = new QPushButton("Start game");
    startButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(startButton);
    connect(startButton, SIGNAL(clicked()), this, SLOT(clickedStart()));
    //this->initButton(prevButton);

    QPushButton *loadButton = new QPushButton("Load game");
    loadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(loadButton);
    connect(loadButton, SIGNAL(clicked()), this, SLOT(clickedLoad()));
    //this->initButton(prevButton);

#ifndef Q_OS_ANDROID
    // applications don't quit on Android.
    QPushButton *quitButton = new QPushButton("Quit game");
    quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(quitButton);
    connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));
    //this->initButton(prevButton);
#endif
}

OptionsGamestate::~OptionsGamestate() {
    LOG("OptionsGamestate::~OptionsGamestate()\n");
    /*VI_flush(0); // delete all the gamestate objects, but leave the game level objects (which should be set at persistence level -1)
    VI_GraphicsEnvironment *genv = game_g->getGraphicsEnvironment();
    game_g->getGraphicsEnvironment()->setPanel(NULL); // as the main panel is now destroyed
    */
    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->centralWidget()->deleteLater();
    window->setCentralWidget(NULL);

    optionsGamestate = NULL;
    LOG("deleted OptionsGamestate\n");
}

void OptionsGamestate::quitGame() {
    LOG("OptionsGamestate::quitGame()\n");
    qApp->quit();
}

void OptionsGamestate::clickedStart() {
    LOG("OptionsGamestate::clickedStart()\n");
    game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);
    GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING);
    game_g->pushMessage(game_message);
}

void OptionsGamestate::clickedLoad() {
    LOG("OptionsGamestate::clickedLoad()\n");
}

void OptionsGamestate::clickedQuit() {
    LOG("OptionsGamestate::clickedQuit()\n");
    this->quitGame();
}

MainGraphicsView::MainGraphicsView(PlayingGamestate *playing_gamestate, QGraphicsScene *scene, QWidget *parent) :
    QGraphicsView(scene, parent), playing_gamestate(playing_gamestate), mouse_down_x(0), mouse_down_y(0), /*gui_overlay_item(NULL),*/ gui_overlay(NULL), c_scale(1.0f)
{
}

void MainGraphicsView::zoom(bool in) {
    qDebug("MainGraphicsView::zoom(%d)", in);
    const float factor_c = 1.1f;
    if( in ) {
        float n_scale = c_scale * factor_c;
        this->setScale(n_scale);
    }
    else {
        float n_scale = c_scale / factor_c;
        this->setScale(n_scale);
    }
}

bool MainGraphicsView::event(QEvent *event) {
    //qDebug("MainGraphicsView::event() type %d\n", event->type());
    // multitouch done by touch events manually - gestures don't seem to work properly on my Android phone?
    if( event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd ) {
        QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
        QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
        if (touchPoints.count() == 2) {
            // determine scale factor
            const QTouchEvent::TouchPoint &touchPoint0 = touchPoints.first();
            const QTouchEvent::TouchPoint &touchPoint1 = touchPoints.last();
            /*float scale_factor =
                    QLineF(touchPoint0.pos(), touchPoint1.pos()).length()
                    / QLineF(touchPoint0.startPos(), touchPoint1.startPos()).length();*/
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
            this->setScale(n_scale);
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
    return QGraphicsView::event(event);
}

void MainGraphicsView::mousePressEvent(QMouseEvent *event) {
    //qDebug("MainGraphicsView::mousePressEvent");
    if( event->button() == Qt::LeftButton ) {
        this->mouse_down_x = event->x();
        this->mouse_down_y = event->y();
    }

    QGraphicsView::mousePressEvent(event);
}

void MainGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    //qDebug("MainGraphicsView::mouseReleaseEvent");
    if( event->button() == Qt::LeftButton ) {
        int m_x = event->x();
        int m_y = event->y();
        int xdist = abs(this->mouse_down_x - m_x);
        int ydist = abs(this->mouse_down_y - m_y);
        // On a touchscreen phone, it's very hard to press and release without causing a drag, so need to allow some tolerance!
        // Needs to be higher on Symbian, at least for Nokia 5800, as touching the display seems to cause it to move so much more easily.
#if defined(Q_OS_SYMBIAN)
        const int drag_tol_c = 24;
#else
        const int drag_tol_c = 16;
#endif
        //if( m_x == this->mouse_down_x && m_y == this->mouse_down_y ) {
        if( xdist <= drag_tol_c && ydist <= drag_tol_c ) {
            QPointF m_scene = this->mapToScene(m_x, m_y);
            LOG("clicked: %f, %f\n", m_scene.x(), m_scene.y());
            playing_gamestate->clickedMainView(m_scene.x(), m_scene.y());
        }
        // else, this was a drag operation
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void MainGraphicsView::wheelEvent(QWheelEvent *event) {
    if( !mobile_c ) {
        // mobile UI needs to be done via multitouch instead
        if( event->delta() > 0 ) {
            this->zoom(true);
        }
        else if( event->delta() < 0 ) {
            this->zoom(false);
        }
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

void MainGraphicsView::setScale(float c_scale) {
    LOG("MainGraphicsView::setScale(%f)\n", c_scale);
    this->c_scale = c_scale;
    this->c_scale = std::min(this->c_scale, max_zoom_c);
    this->c_scale = std::max(this->c_scale, min_zoom_c);
    this->resetTransform();
    this->scale(this->c_scale, this->c_scale);
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
    for(set<TextEffect *>::const_iterator iter = text_effects.begin(); iter != text_effects.end(); ++iter) {
        const TextEffect *te = *iter;
        float te_w = te->boundingRect().width() * font_scale;
        float te_h = te->boundingRect().height() * font_scale;
        /*if( text_effect->boundingRect().right() >= te->boundingRect().left() &&
            text_effect->boundingRect().left() <= te->boundingRect().right() &&
            text_effect->boundingRect().bottom() >= te->boundingRect().top() &&
            text_effect->boundingRect().top() <= te->boundingRect().bottom() ) {*/
        if( text_effect->x() + text_effect_w >= te->x() &&
            text_effect->x() <= te->x() + te_w &&
            text_effect->y() + text_effect_h >= te->y() &&
            text_effect->y() <= te->y() + te_h ) {
            //qDebug("    shift text effect by %f", te->boundingRect().bottom() - text_effect->y() );
            /*qDebug("    shift text effect by %f", te->y() + te_h - text_effect->y() );
            qDebug("    te at %f, %f", te->x(), te->y());
            qDebug("    rect %f, %f : %f, %f", te->boundingRect().left(), te->boundingRect().top(), te->boundingRect().right(), te->boundingRect().bottom());
            qDebug("    size %f x %f", te_w, te_h);*/
            //text_effect->setPos( text_effect->x(), te->boundingRect().bottom() );
            text_effect->setPos( text_effect->x(), te->y() + te_h );
        }
    }
    this->scene()->addItem(text_effect);
    text_effects.insert(text_effect);
}

void MainGraphicsView::removeTextEffect(TextEffect *text_effect) {
    this->scene()->removeItem(text_effect);
    text_effects.erase(text_effect);
}

GUIOverlay::GUIOverlay(PlayingGamestate *playing_gamestate, MainGraphicsView *view) :
    QWidget(view), playing_gamestate(playing_gamestate)
{
    //this->setAttribute(Qt::WA_NoSystemBackground);
}

void GUIOverlay::paintEvent(QPaintEvent *event) {
    //qDebug("GUIOverlay::paintEvent()");

    //this->move(0, 0);
    QPainter painter(this);
    painter.setFont( game_g->getFontStd() );
    /*QBrush brush(QColor(255, 0, 0, 255));
    painter.fillRect(QRectF(QPointF(0, 0), this->size()), brush);*/
    //qDebug("%d, %d\n", view->rect().width(), view->rect().height());
    /*painter.setPen(Qt::green);
    painter.drawText(16, 16, "test blah blah blah 123");*/
    if( playing_gamestate->getPlayer() != NULL ) {
        float bar_x = 16.0f/640.0f;
        float bar_y = 32.0f/360.0f;
        float text_y = bar_y - 4.0f/360.0f;
        //int bar_y = 32;
        //int text_y = bar_y - 4;
        const Character *player = playing_gamestate->getPlayer();
        painter.setPen(Qt::white);
        //painter.drawText(16.0f, text_y, player->getName().c_str());
        painter.drawText(bar_x*width(), text_y*height(), player->getName().c_str());
        float fraction = ((float)player->getHealthPercent()) / (float)100.0f;
        //this->drawBar(painter, 16, bar_y, 100, 16, fraction, Qt::darkGreen);
        this->drawBar(painter, bar_x, bar_y, 100.0f/640.0f, 16.0f/360.0f, fraction, Qt::darkGreen);
        if( player->getTargetNPC() != NULL ) {
            const Character *enemy = player->getTargetNPC();
            //qDebug("enemy: %d", enemy);
            //qDebug("name: %s", enemy->getName().c_str());
            float bar_x2 = 132.0f/640.0f;
            painter.setPen(Qt::white);
            //painter.drawText(132, text_y, enemy->getName().c_str());
            painter.drawText(bar_x2*width(), text_y*height(), enemy->getName().c_str());
            fraction = ((float)enemy->getHealthPercent()) / (float)100.0f;
            //this->drawBar(painter, 132, bar_y, 100, 16, fraction, Qt::darkRed);
            this->drawBar(painter, bar_x2, bar_y, 100.0f/640.0f, 16.0f/360.0f, fraction, Qt::darkRed);
        }
    }

    if( this->display_progress ) {
        /*const int x_off = 16;
        const int hgt = 64;
        this->drawBar(painter, x_off, this->height()/2 - hgt/2, this->width() - 2*x_off, hgt, ((float)this->progress_percent)/100.0f, Qt::darkRed);*/
        const float x_off = 16.0f/640.0f;
        const float hgt = 64.0f/360.0f;
        this->drawBar(painter, x_off, 0.5f - 0.5f*hgt, 1.0f - 2.0f*x_off, hgt, ((float)this->progress_percent)/100.0f, Qt::darkRed);
    }

    /*{
        // DEBUG ONLY
        painter.setPen(Qt::red);
        float fps = 1000.0f / game_g->getScreen()->getGameTimeFrameMS();
        painter.drawText(8, height() - 16, QString::number(fps));
    }*/
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
    painter.drawRect(x, y, width-1, height-1);
    painter.fillRect(x2, y2, width2, height2, brush_bg);
    painter.fillRect(x2, y2, width2*fraction, height2, brush_fg);
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

    Character *player = playing_gamestate->getPlayer();

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    QString html ="<html><body>";

    html += "<b>Name:</b> ";
    html += player->getName().c_str();
    html += "<br/>";

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

    html += "</body></html>";

    QWebView *label = new QWebView();
    label->setHtml(html);
    layout->addWidget(label);

    QPushButton *closeButton = new QPushButton("Continue");
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(clickedCloseSubwindow()));
}

ScrollingListWidget::ScrollingListWidget() : QListWidget(), saved_x(0), saved_y(0) {
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
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

ItemsWindow::ItemsWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate), list(NULL),
    dropButton(NULL), armButton(NULL), wearButton(NULL), useButton(NULL),
    view_type(VIEWTYPE_ALL)
{
    playing_gamestate->addWidget(this);

    Character *player = playing_gamestate->getPlayer();

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *viewAllButton = new QPushButton("All");
        h_layout->addWidget(viewAllButton);
        connect(viewAllButton, SIGNAL(clicked()), this, SLOT(clickedViewAll()));

        QPushButton *viewWeaponsButton = new QPushButton("Weapons");
        h_layout->addWidget(viewWeaponsButton);
        connect(viewWeaponsButton, SIGNAL(clicked()), this, SLOT(clickedViewWeapons()));

        QPushButton *viewAmmoButton = new QPushButton("Ammo");
        h_layout->addWidget(viewAmmoButton);
        connect(viewAmmoButton, SIGNAL(clicked()), this, SLOT(clickedViewAmmo()));

        QPushButton *viewShieldsButton = new QPushButton("Shields");
        h_layout->addWidget(viewShieldsButton);
        connect(viewShieldsButton, SIGNAL(clicked()), this, SLOT(clickedViewShields()));

        QPushButton *viewArmourButton = new QPushButton("Armour");
        h_layout->addWidget(viewArmourButton);
        connect(viewArmourButton, SIGNAL(clicked()), this, SLOT(clickedViewArmour()));

        QPushButton *viewMagicButton = new QPushButton("Magic");
        h_layout->addWidget(viewMagicButton);
        connect(viewMagicButton, SIGNAL(clicked()), this, SLOT(clickedViewMagic()));

        QPushButton *viewMiscButton = new QPushButton("Misc");
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

    this->refreshList();

    connect(list, SIGNAL(currentRowChanged(int)), this, SLOT(changedSelectedItem(int)));

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *goldLabel = new QLabel("Gold: " + QString::number( player->getGold() ));
        //goldLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // needed to fix problem of having too little vertical space (on Qt Smartphone Simulator at least)
        h_layout->addWidget(goldLabel);

        weightLabel = new QLabel(""); // label set in setWeightLabel()
        //weightLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(weightLabel);
        //this->setWeightLabel(weight);
        this->setWeightLabel();
    }

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        //armButton = new QPushButton("Arm Weapon");
        armButton = new QPushButton(""); // text set in changedSelectedItem()
        h_layout->addWidget(armButton);
        connect(armButton, SIGNAL(clicked()), this, SLOT(clickedArmWeapon()));

        wearButton = new QPushButton(""); // text set in changedSelectedItem()
        h_layout->addWidget(wearButton);
        connect(wearButton, SIGNAL(clicked()), this, SLOT(clickedWearArmour()));

        useButton = new QPushButton(""); // text set in changedSelectedItem()
        h_layout->addWidget(useButton);
        connect(useButton, SIGNAL(clicked()), this, SLOT(clickedUseItem()));

        dropButton = new QPushButton("Drop Item");
        h_layout->addWidget(dropButton);
        connect(dropButton, SIGNAL(clicked()), this, SLOT(clickedDropItem()));
    }

    QPushButton *closeButton = new QPushButton("Continue");
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(clickedCloseSubwindow()));

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
        else if( view_type == VIEWTYPE_MISC && !( item->getType() != ITEMTYPE_GENERAL && item->isMagical() ) ) {
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
        return;
    }
    dropButton->setVisible(true);
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
}

/*void ItemsWindow::setWeightLabel(int weight) {
    this->weightLabel->setText("Weight: " + QString::number(weight));
}*/

void ItemsWindow::setWeightLabel() {
    Character *player = this->playing_gamestate->getPlayer();
    int weight = player->calculateItemsWeight();
    this->weightLabel->setText("Weight: " + QString::number(weight));
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
    if( item->use(this->playing_gamestate, player) ) {
        // item is deleted
        player->takeItem(item);
        item = NULL;
        this->itemIsDeleted(index);
    }
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
    playing_gamestate(playing_gamestate), list(NULL), player_list(NULL), items(items), costs(costs)
{
    playing_gamestate->addWidget(this);

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    Character *player = playing_gamestate->getPlayer();

    QLabel *label = new QLabel("What would you like to buy?");
    label->setWordWrap(true);
    layout->addWidget(label);

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
                if( item->getName() == item2->getName() ) {
                    cost = costs.at(i);
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
    }

    goldLabel = new QLabel("");
    layout->addWidget(goldLabel);
    this->updateGoldLabel();

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *sellButton = new QPushButton("Sell");
        h_layout->addWidget(sellButton);
        connect(sellButton, SIGNAL(clicked()), this, SLOT(clickedSell()));

        QPushButton *buyButton = new QPushButton("Buy");
        h_layout->addWidget(buyButton);
        connect(buyButton, SIGNAL(clicked()), this, SLOT(clickedBuy()));
    }

    QPushButton *closeButton = new QPushButton("Finish Trading");
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(clickedCloseSubwindow()));
}

void TradeWindow::updateGoldLabel() {
    Character *player = playing_gamestate->getPlayer();
    goldLabel->setText("Gold: " + QString::number( player->getGold() ));
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
        this->addPlayerItem(item, cost);
        list->setCurrentRow(-1);
    }
    else {
        LOG("not enough money\n");
        game_g->showInfoDialog("Trade", "You do not have enough money to purchase this item.");
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
        QListWidgetItem *list_item = player_list->takeItem(index);
        delete list_item;
        player_items.erase(player_items.begin() + index);
        player_costs.erase(player_costs.begin() + index);
    }
    else {
        game_g->showInfoDialog("Trade", "This shop doesn't buy that item.");
    }
}

CampaignWindow::CampaignWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate)
{
    playing_gamestate->addWidget(this);

    QFont font = game_g->getFontStd();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    /*QWebView *label = new QWebView();
    QString html = "<html><body><p>You have left the dungeon, and returned to your village to rest. You may also take the time to visit the local shops to buy supplies, sell any wares you have, as well as conducting training to improve your skills.</p></body></html>";
    label->setHtml(html);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(label);*/
    QLabel *label = new QLabel("You have left the dungeon, and returned to your village to rest. You may also take the time to visit the local shops to buy supplies, sell any wares you have, as well as conducting training to improve your skills.");
    label->setFont(game_g->getFontSmall());
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(label);

    for(vector<Shop *>::const_iterator iter = playing_gamestate->shopsBegin(); iter != playing_gamestate->shopsEnd(); ++iter) {
        const Shop *shop = *iter;
        QPushButton *shopButton = new QPushButton(shop->getName().c_str());
        QVariant variant = qVariantFromValue((void *)shop);
        shopButton->setProperty("shop", variant);
        //shopButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(shopButton);
        connect(shopButton, SIGNAL(clicked()), this, SLOT(clickedShop()));
    }

    QPushButton *trainingButton = new QPushButton("Training");
    //trainingButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(trainingButton);
    connect(trainingButton, SIGNAL(clicked()), this, SLOT(clickedTraining()));

    QPushButton *closeButton = new QPushButton(playing_gamestate->getQuest()->isCompleted() ? "Start next Quest" : "Continue your Quest");
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(clickedCloseSubwindow()));
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

void CampaignWindow::clickedTraining() {
    LOG("CampaignWindow::clickedTraining()\n");
}

PlayingGamestate::PlayingGamestate() :
    scene(NULL), view(NULL), gui_overlay(NULL), main_stacked_widget(NULL),
    player(NULL), c_quest_indx(0), c_location(NULL), quest(NULL)
{
    LOG("PlayingGamestate::PlayingGamestate()\n");
    playingGamestate = this;

    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->setEnabled(false);
    game_g->getScreen()->setPaused(true);

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
    //view->grabGesture(Qt::PinchGesture);
    view->setAttribute(Qt::WA_AcceptTouchEvents);
    view->setDragMode(QGraphicsView::ScrollHandDrag);
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
        statsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //statsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(statsButton, SIGNAL(clicked()), this, SLOT(clickedStats()));
        v_layout->addWidget(statsButton);

        QPushButton *itemsButton = new QPushButton("Items");
        itemsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //itemsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(itemsButton, SIGNAL(clicked()), this, SLOT(clickedItems()));
        v_layout->addWidget(itemsButton);

        QPushButton *spellsButton = new QPushButton("Spells");
        spellsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //spellsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        v_layout->addWidget(spellsButton);

        QPushButton *journalButton = new QPushButton("Journal");
        journalButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //journalButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(journalButton, SIGNAL(clicked()), this, SLOT(clickedJournal()));
        v_layout->addWidget(journalButton);

        /*QPushButton *quitButton = new QPushButton("Quit");
        quitButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        v_layout->addWidget(quitButton);
        connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));*/

        QPushButton *pauseButton = new QPushButton("Pause");
        pauseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        connect(pauseButton, SIGNAL(clicked()), game_g->getScreen(), SLOT(togglePaused()));
        v_layout->addWidget(pauseButton);

        QPushButton *restButton = new QPushButton("Rest");
        restButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        connect(restButton, SIGNAL(clicked()), this, SLOT(clickedRest()));
        v_layout->addWidget(restButton);

        QPushButton *optionsButton = new QPushButton("Options");
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
            connect(zoomoutButton, SIGNAL(clicked()), view, SLOT(zoomOut()));
            h_layout->addWidget(zoomoutButton);

            QPushButton *zoominButton = new QPushButton("+");
            connect(zoominButton, SIGNAL(clicked()), view, SLOT(zoomIn()));
            h_layout->addWidget(zoominButton);
        }
    }

    view->showFullScreen();

    gui_overlay = new GUIOverlay(this, view);
    gui_overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    view->setGUIOverlay(gui_overlay);

    /*QVBoxLayout *view_layout = new QVBoxLayout(view);
    view_layout->addSpacing(1);
    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        view_layout->addLayout(h_layout);

        //QPushButton *zoomoutButton = new QPushButton("-", view);
        QPushButton *zoomoutButton = new QPushButton("-");
        h_layout->addWidget(zoomoutButton);
        //zoomoutButton->move(view->width() - zoomoutButton->width() - 8, 32);

        h_layout->addSpacing(1);

    }*/

    gui_overlay->setProgress(0);
    qApp->processEvents();

    // create RPG data

    LOG("load images\n");
    {
        QFile file(":/data/images.xml");
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open xml file");
        }
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                if( reader.name() == "image" ) {
                    QStringRef type_s = reader.attributes().value("type");
                    if( type_s.length() == 0 )
                        throw string("image element has no type attribute or is zero length");
                    QStringRef name_s = reader.attributes().value("name");
                    if( name_s.length() == 0 )
                        throw string("image element has no name attribute or is zero length");
                    QStringRef imagetype_s = reader.attributes().value("imagetype");
                    LOG("image element type: %s name: %s imagetype: %s\n", type_s.toString().toStdString().c_str(), name_s.toString().toStdString().c_str(), imagetype_s.toString().toStdString().c_str());
                    QPixmap pixmap;
                    bool clip = false;
                    int xpos = 0, ypos = 0, width = 0, height = 0;
                    if( imagetype_s.length() == 0 ) {
                        // load file
                        QStringRef filename_s = reader.attributes().value("filename");
                        if( filename_s.length() == 0 )
                            throw string("image element has no filename attribute or is zero length");
                        QString filename = ":/" + filename_s.toString();
                        LOG("    filename: %s\n", filename.toStdString().c_str());
                        QStringRef xpos_s = reader.attributes().value("xpos");
                        QStringRef ypos_s = reader.attributes().value("ypos");
                        QStringRef width_s = reader.attributes().value("width");
                        QStringRef height_s = reader.attributes().value("height");
                        if( xpos_s.length() > 0 || ypos_s.length() > 0 || width_s.length() > 0 || height_s.length() > 0 ) {
                            clip = true;
                            xpos = parseInt(xpos_s.toString());
                            ypos = parseInt(ypos_s.toString());
                            width = parseInt(width_s.toString());
                            height = parseInt(height_s.toString());
                            LOG("    clip to: %d, %d, %d, %d\n", xpos, ypos, width, height);
                        }
                        pixmap = game_g->loadImage(filename.toStdString().c_str(), clip, xpos, ypos, width, height);
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
                        LOG("unknown imagetype: %s\n", imagetype_s.string()->toStdString().c_str());
                        throw string("unknown imagetype");
                    }
                    if( type_s == "item") {
                        this->item_images[name_s.toString().toStdString()] = pixmap;
                    }
                    else if( type_s == "scenery" ) {
                        this->scenery_images[name_s.toString().toStdString()] = pixmap;
                        QStringRef filename_opened_s = reader.attributes().value("filename_opened");
                        if( imagetype_s.length() == 0 && filename_opened_s.length() != 0 ) {
                            // n.b., opened image can only be specified for loading images
                            QString filename_opened = ":/" + filename_opened_s.toString();
                            LOG("load opened image: %s\n", filename_opened.toStdString().c_str());
                            this->scenery_opened_images[name_s.toString().toStdString()] = game_g->loadImage(filename_opened.toStdString().c_str(), clip, xpos, ypos, width, height);
                        }
                    }
                    else if( type_s == "npc" ) {
                        QString name = name_s.toString(); // need to take copy of string reference
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
                                        throw string("unknown animation type");
                                    }
                                    animation_layer_definition.push_back( AnimationLayerDefinition(sub_name_s.toString().toStdString(), sub_start, sub_length, animation_type) );
                                }
                                else {
                                    throw string("unknown xml tag within npc section");
                                }
                            }
                        }
                        //this->animation_layers[name.toStdString()] = AnimationLayer::create(filename.toStdString().c_str(), animation_layer_definition);
                        this->animation_layers[name.toStdString()] = AnimationLayer::create(pixmap, animation_layer_definition);
                    }
                    else {
                        throw string("image element has unknown type attribute");
                    }
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error reading images.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading xml file");
        }
    }

    gui_overlay->setProgress(20);
    qApp->processEvents();

    LOG("load items\n");
    {
        QFile file(":/data/items.xml");
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open xml file");
        }
        enum ItemsXMLType {
            ITEMS_XML_TYPE_NONE = 0,
            ITEMS_XML_TYPE_SHOP = 1
        };
        ItemsXMLType itemsXMLType = ITEMS_XML_TYPE_NONE;
        Shop *shop = NULL;
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                /*qDebug("read xml: %s", reader.name().toString().toStdString().c_str());
                qDebug("    type: %d", reader.tokenType());
                qDebug("    n attributes: %d", reader.attributes().size());*/
                if( reader.name() == "item" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                        throw string("unexpected items xml");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    QStringRef icon_width_s = reader.attributes().value("icon_width");
                    QStringRef weight_s = reader.attributes().value("weight");
                    QStringRef rating_s = reader.attributes().value("rating");
                    QStringRef magical_s = reader.attributes().value("magical");
                    QStringRef use_s = reader.attributes().value("use");
                    QStringRef use_verb_s = reader.attributes().value("use_verb");
                    int weight = parseInt(weight_s.toString());
                    int rating = parseInt(rating_s.toString(), true);
                    if( rating == 0 )
                        rating = 1; // so the default of 0 defaults instead to 1
                    bool magical = parseBool(magical_s.toString(), true);
                    Item *item = new Item(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight);
                    if( icon_width_s.length() > 0 ) {
                        float icon_width = parseFloat(icon_width_s.toString());
                        //LOG("icon_width: %f\n", icon_width);
                        item->setIconWidth(icon_width);
                    }
                    item->setRating(rating);
                    item->setMagical(magical);
                    if( use_s.length() > 0 ) {
                        item->setUse(use_s.toString().toStdString(), use_verb_s.toString().toStdString());
                    }
                    this->addStandardItem( item );
                }
                else if( reader.name() == "weapon" ) {
                    //qDebug("    weapon:");
                    if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                        throw string("unexpected items xml");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    QStringRef weight_s = reader.attributes().value("weight");
                    QStringRef animation_name_s = reader.attributes().value("animation_name");
                    QStringRef two_handed_s = reader.attributes().value("two_handed");
                    QStringRef ranged_s = reader.attributes().value("ranged");
                    QStringRef ammo_s = reader.attributes().value("ammo");
                    /*qDebug("    name: %s", name_s.toString().toStdString().c_str());
                    qDebug("    image_name: %s", image_name_s.toString().toStdString().c_str());
                    qDebug("    animation_name: %s", animation_name_s.toString().toStdString().c_str());
                    qDebug("    weight: %s", weight_s.toString().toStdString().c_str());
                    qDebug("    two_handed_s: %s", two_handed_s.toString().toStdString().c_str());
                    qDebug("    ranged_s: %s", ranged_s.toString().toStdString().c_str());
                    qDebug("    ammo_s: %s", ammo_s.toString().toStdString().c_str());*/
                    int weight = parseInt(weight_s.toString());
                    bool two_handed = parseBool(two_handed_s.toString(), true);
                    bool ranged = parseBool(ranged_s.toString(), true);
                    Weapon *weapon = new Weapon(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, animation_name_s.toString().toStdString());
                    weapon->setTwoHanded(two_handed);
                    weapon->setRanged(ranged);
                    if( ammo_s.length() > 0 ) {
                        weapon->setRequiresAmmo(true, ammo_s.toString().toStdString());
                    }
                    this->addStandardItem( weapon );
                }
                else if( reader.name() == "shield" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                        throw string("unexpected items xml");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    QStringRef weight_s = reader.attributes().value("weight");
                    QStringRef animation_name_s = reader.attributes().value("animation_name");
                    int weight = parseInt(weight_s.toString());
                    Shield *shield = new Shield(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, animation_name_s.toString().toStdString());
                    this->addStandardItem( shield );
                }
                else if( reader.name() == "armour" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                        throw string("unexpected items xml");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    QStringRef weight_s = reader.attributes().value("weight");
                    QStringRef rating_s = reader.attributes().value("rating");
                    int weight = parseInt(weight_s.toString());
                    int rating = parseInt(rating_s.toString());
                    Armour *armour = new Armour(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, rating);
                    this->addStandardItem( armour );
                }
                else if( reader.name() == "ammo" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                        throw string("unexpected items xml");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    QStringRef projectile_image_name_s = reader.attributes().value("projectile_image_name");
                    QStringRef amount_s = reader.attributes().value("amount");
                    int amount = parseInt(amount_s.toString());
                    Ammo *ammo = new Ammo(name_s.toString().toStdString(), image_name_s.toString().toStdString(), projectile_image_name_s.toString().toStdString(), amount);
                    this->addStandardItem( ammo );
                }
                else if( reader.name() == "currency" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                        throw string("unexpected items xml");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    Currency *currency = new Currency(name_s.toString().toStdString(), image_name_s.toString().toStdString());
                    this->addStandardItem( currency );
                }
                else if( reader.name() == "shop" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_NONE ) {
                        throw string("unexpected items xml");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    shop = new Shop(name_s.toString().toStdString());
                    shops.push_back(shop);
                    itemsXMLType = ITEMS_XML_TYPE_SHOP;
                }
                else if( reader.name() == "purchase" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_SHOP ) {
                        throw string("unexpected items xml");
                    }
                    QStringRef template_s = reader.attributes().value("template");
                    QStringRef cost_s = reader.attributes().value("cost");
                    int cost = parseInt(cost_s.toString());
                    Item *item = this->cloneStandardItem(template_s.toString().toStdString());
                    shop->addItem(item, cost);
                }
            }
            else if( reader.isEndElement() ) {
                if( reader.name() == "shop" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_SHOP ) {
                        throw string("unexpected items xml");
                    }
                    shop = NULL;
                    itemsXMLType = ITEMS_XML_TYPE_NONE;
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error reading items.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading xml file");
        }
    }

    gui_overlay->setProgress(30);
    qApp->processEvents();

    this->player = new Character("Player", "", false);
    player->initialiseHealth(100);
    //player->initialiseHealth(5); // test
    player->addItem( this->cloneStandardItem("Long Sword") );
    player->addItem( this->cloneStandardItem("Shield") );
    player->addItem( this->cloneStandardItem("Longbow") );
    player->addItem( this->cloneStandardItem("Leather Armour") );
    player->addItem( this->cloneStandardItem("Arrows") );
    player->addGold( rollDice(2, 6, 10) );

    LOG("load NPCs\n");
    {
        QFile file(":/data/npcs.xml");
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open xml file");
        }
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                if( reader.name() == "npc" ) {
                    QStringRef name_s = reader.attributes().value("name");
                    LOG("found npc template: %s\n", name_s.toString().toStdString().c_str());
                    QStringRef animation_name_s = reader.attributes().value("animation_name");
                    QStringRef health_min_s = reader.attributes().value("health_min");
                    int health_min = parseInt(health_min_s.toString());
                    QStringRef health_max_s = reader.attributes().value("health_max");
                    int health_max = parseInt(health_max_s.toString());
                    QStringRef gold_min_s = reader.attributes().value("gold_min");
                    int gold_min = parseInt(gold_min_s.toString());
                    QStringRef gold_max_s = reader.attributes().value("gold_max");
                    int gold_max = parseInt(gold_max_s.toString());
                    CharacterTemplate *character_template = new CharacterTemplate(animation_name_s.toString().toStdString(), health_min, health_max, gold_min, gold_max);
                    this->character_templates[ name_s.toString().toStdString() ] = character_template;
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error reading npcs.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading xml file");
        }
    }

    gui_overlay->setProgress(40);
    qApp->processEvents();

    LOG("create animation frames\n");
    LOG("load player image\n");
    vector<AnimationLayerDefinition> player_animation_layer_definition;
    player_animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 4, AnimationSet::ANIMATIONTYPE_BOUNCE) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("run", 4, 8, AnimationSet::ANIMATIONTYPE_LOOP) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("attack", 12, 4, AnimationSet::ANIMATIONTYPE_SINGLE) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("ranged", 28, 4, AnimationSet::ANIMATIONTYPE_SINGLE) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("death", 18, 6, AnimationSet::ANIMATIONTYPE_SINGLE) );
    LOG("clothes layer\n");
    int time_s = clock();
    //AnimationLayer *clothes_layer = AnimationLayer::create(":/gfx/textures/isometric_hero/clothes.png");
    this->animation_layers["clothes"] = AnimationLayer::create(":/gfx/textures/isometric_hero/clothes.png", player_animation_layer_definition);
    LOG("time to load: %d\n", clock() - time_s);
    /*LOG("head layer\n");
    string head_layer_filename = ":/gfx/textures/isometric_hero/male_head1.png";
    AnimationLayer *head_layer = new AnimationLayer();*/
    //AnimationLayer *head_layer = AnimationLayer::create(":/gfx/textures/isometric_hero/male_head1.png");
    this->animation_layers["head"] = AnimationLayer::create(":/gfx/textures/isometric_hero/male_head1.png", player_animation_layer_definition);
    LOG("longsword layer\n");
    this->animation_layers["longsword"] = AnimationLayer::create(":/gfx/textures/isometric_hero/longsword.png", player_animation_layer_definition);
    LOG("longbow layer\n");
    this->animation_layers["longbow"] = AnimationLayer::create(":/gfx/textures/isometric_hero/longbow.png", player_animation_layer_definition);
    LOG("shield layer\n");
    this->animation_layers["shield"] = AnimationLayer::create(":/gfx/textures/isometric_hero/shield.png", player_animation_layer_definition);

    gui_overlay->setProgress(60);
    qApp->processEvents();

    gui_overlay->setProgress(70);
    qApp->processEvents();

    LOG("load floor image\n");
    builtin_images["floor"] = game_g->loadImage(":/gfx/textures/floor_paved.png");

    gui_overlay->setProgress(80);
    qApp->processEvents();

    LOG("load wall image\n");
    builtin_images["wall"] = game_g->loadImage(":/gfx/textures/wall.png");

    gui_overlay->setProgress(90);
    qApp->processEvents();

    LOG("load quests\n");

    {
        QFile file(":/data/quests.xml");
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open xml file");
        }
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                if( reader.name() == "quest" ) {
                    QStringRef filename_s = reader.attributes().value("filename");
                    LOG("found quest: %s\n", filename_s.toString().toStdString().c_str());
                    if( filename_s.length() == 0 ) {
                        throw string("quest doesn't have filename info");
                    }
                    QuestInfo quest_info(filename_s.toString().toStdString());
                    this->quest_list.push_back(quest_info);
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error reading quests.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading xml file");
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
    this->clickedCloseSubwindow();
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
    for(map<string, Item *>::iterator iter = this->standard_items.begin(); iter != this->standard_items.end(); ++iter) {
        Item *item = iter->second;
        LOG("about to delete standard item: %d\n", item);
        LOG("    name: %s\n", item->getName().c_str());
        delete item;
    }
    for(map<string, CharacterTemplate *>::iterator iter = this->character_templates.begin(); iter != this->character_templates.end(); ++iter) {
        CharacterTemplate *character_template = iter->second;
        LOG("about to delete character template: %d\n", character_template);
        delete character_template;
    }
    for(vector<Shop *>::iterator iter = shops.begin(); iter != shops.end(); ++iter) {
        Shop *shop = *iter;
        delete shop;
    }
    LOG("done\n");
}

void PlayingGamestate::loadQuest() {
    LOG("PlayingGamestate::loadQuest()\n");

    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->setEnabled(false);
    game_g->getScreen()->setPaused(true);

    const QuestInfo &c_quest_info = this->quest_list.at(c_quest_indx);
    this->quest = new Quest();
    Location *location = new Location();
    this->quest->addLocation(location);

    gui_overlay->setProgress(0);
    qApp->processEvents();

    {
        bool done_player_start = false;
        enum QuestXMLType {
            QUEST_XML_TYPE_NONE = 0,
            //QUEST_XML_TYPE_BOUNDARY = 1,
            QUEST_XML_TYPE_SCENERY = 2,
            QUEST_XML_TYPE_NPC = 3
        };
        QuestXMLType questXMLType = QUEST_XML_TYPE_NONE;
        Polygon2D boundary;
        Scenery *scenery = NULL;
        Character *npc = NULL;
        QString qt_filename = ":/" + QString(c_quest_info.getFilename().c_str());
        LOG("load quest: %s\n", qt_filename.toStdString().c_str());
        QFile file(qt_filename);
        //QFile file(":/data/quest.xml");
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open xml file");
        }
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                //LOG("read start element: %s\n", reader.name().toString().toStdString().c_str());
                if( reader.name() == "info" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        throw string("unexpected quest xml");
                    }
                    QString info = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    LOG("quest info: %s\n", info.toStdString().c_str());
                    quest->setInfo(info.toStdString());
                }
                else if( reader.name() == "floorregion" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        throw string("unexpected quest xml");
                    }
                    QStringRef rect_x_s = reader.attributes().value("rect_x");
                    float rect_x = parseFloat(rect_x_s.toString());
                    QStringRef rect_y_s = reader.attributes().value("rect_y");
                    float rect_y = parseFloat(rect_y_s.toString());
                    QStringRef rect_w_s = reader.attributes().value("rect_w");
                    float rect_w = parseFloat(rect_w_s.toString());
                    QStringRef rect_h_s = reader.attributes().value("rect_h");
                    float rect_h = parseFloat(rect_h_s.toString());
                    //LOG("found floor region: %f, %f, %f, %f\n", rect_x, rect_y, rect_w, rect_h);
                    FloorRegion *floor_region = FloorRegion::createRectangle(rect_x, rect_y, rect_w, rect_h);
                    location->addFloorRegion(floor_region);
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
                    LOG("found boundary point: %f, %f\n", point_x, point_y);
                    boundary.addPoint(Vector2D(point_x, point_y));
                }*/
                else if( reader.name() == "player_start" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        throw string("unexpected quest xml");
                    }
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    location->addCharacter(player, pos_x, pos_y);
                    done_player_start = true;
                }
                else if( reader.name() == "quest_objective" ) {
                    QStringRef type_s = reader.attributes().value("type");
                    QStringRef arg1_s = reader.attributes().value("arg1");
                    QuestObjective *quest_objective = new QuestObjective(type_s.toString().toStdString(), arg1_s.toString().toStdString());
                    this->quest->setQuestObjective(quest_objective);
                }
                else if( reader.name() == "npc" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        throw string("unexpected quest xml");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef template_s = reader.attributes().value("template");
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    CharacterTemplate *character_template = this->character_templates[name_s.toString().toStdString()];
                    if( character_template == NULL ) {
                        LOG("can't find character template: %s\n", template_s.toString().toStdString().c_str());
                        throw string("can't find character template");
                    }
                    npc = new Character(name_s.toString().toStdString(), true, *character_template);
                    location->addCharacter(npc, pos_x, pos_y);
                    questXMLType = QUEST_XML_TYPE_NPC;
                }
                else if( reader.name() == "item" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE && questXMLType != QUEST_XML_TYPE_SCENERY && questXMLType != QUEST_XML_TYPE_NPC ) {
                        throw string("unexpected quest xml");
                    }
                    QStringRef template_s = reader.attributes().value("template");
                    Item *item = this->cloneStandardItem(template_s.toString().toStdString());
                    if( item == NULL ) {
                        LOG("can't find item: %s\n", template_s.toString().toStdString().c_str());
                        throw string("can't find item");
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
                        location->addItem(item, pos_x, pos_y);
                    }
                }
                else if( reader.name() == "gold" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE && questXMLType != QUEST_XML_TYPE_SCENERY && questXMLType != QUEST_XML_TYPE_NPC ) {
                        throw string("unexpected quest xml");
                    }
                    QStringRef amount_s = reader.attributes().value("amount");
                    int amount = parseInt(amount_s.toString());
                    Item *item = this->cloneGoldItem(amount);
                    if( item == NULL ) {
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
                        location->addItem(item, pos_x, pos_y);
                    }
                }
                else if( reader.name() == "scenery" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        throw string("unexpected quest xml");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    QStringRef blocking_s = reader.attributes().value("blocking");
                    bool blocking = parseBool(blocking_s.toString());
                    QStringRef block_visibility_s = reader.attributes().value("block_visibility");
                    bool block_visibility = parseBool(block_visibility_s.toString(), true);
                    QStringRef door_s = reader.attributes().value("door");
                    bool door = parseBool(door_s.toString(), true);
                    QStringRef exit_s = reader.attributes().value("exit");
                    bool exit = parseBool(exit_s.toString(), true);
                    QStringRef locked_s = reader.attributes().value("locked");
                    bool locked = parseBool(locked_s.toString(), true);
                    QStringRef unlock_item_name_s = reader.attributes().value("unlocked_by_template");
                    QStringRef size_w_s = reader.attributes().value("w");
                    float size_w = parseFloat(size_w_s.toString());
                    QStringRef size_h_s = reader.attributes().value("h");
                    float size_h = parseFloat(size_h_s.toString());

                    scenery = new Scenery(name_s.toString().toStdString(), image_name_s.toString().toStdString(), size_w, size_h);
                    location->addScenery(scenery, pos_x, pos_y);
                    scenery->setBlocking(blocking, block_visibility);
                    if( door && exit ) {
                        throw string("scenery can't be both a door and an exit");
                    }
                    scenery->setDoor(door);
                    scenery->setExit(exit);
                    scenery->setLocked(locked);
                    if( unlock_item_name_s.length() > 0 ) {
                        scenery->setUnlockItemName(unlock_item_name_s.toString().toStdString());
                    }
                    questXMLType = QUEST_XML_TYPE_SCENERY;
                }
                else if( reader.name() == "trap" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        throw string("unexpected quest xml");
                    }
                    QStringRef name_s = reader.attributes().value("name");
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    QStringRef size_w_s = reader.attributes().value("w");
                    float size_w = parseFloat(size_w_s.toString());
                    QStringRef size_h_s = reader.attributes().value("h");
                    float size_h = parseFloat(size_h_s.toString());

                    Trap *trap = new Trap(name_s.toString().toStdString(), size_w, size_h);
                    location->addTrap(trap, pos_x, pos_y);
                }
            }
            else if( reader.isEndElement() ) {
                //LOG("read end element: %s\n", reader.name().toString().toStdString().c_str());
                /*if( reader.name() == "boundary" ) {
                    if( questXMLType != QUEST_XML_TYPE_BOUNDARY ) {
                        throw string("unexpected quest xml");
                    }
                    location->addBoundary(boundary);
                    questXMLType = QUEST_XML_TYPE_NONE;
                }
                else */if( reader.name() == "npc" ) {
                    if( questXMLType != QUEST_XML_TYPE_NPC ) {
                        throw string("unexpected quest xml");
                    }
                    npc = NULL;
                    questXMLType = QUEST_XML_TYPE_NONE;
                }
                else if( reader.name() == "scenery" ) {
                    if( questXMLType != QUEST_XML_TYPE_SCENERY ) {
                        throw string("unexpected quest xml");
                    }
                    scenery = NULL;
                    questXMLType = QUEST_XML_TYPE_NONE;
                }
            }
        }
        if( reader.hasError() ) {
            LOG("error reading quest.xml %d: %s", reader.error(), reader.errorString().toStdString().c_str());
            throw string("error reading xml file");
        }
        else if( !done_player_start ) {
            LOG("quest.xml didn't define player_start\n");
            throw string("quest.xml didn't define player_start");
        }
    }

    gui_overlay->setProgress(50);
    qApp->processEvents();

    location->createBoundariesForRegions();
    location->createBoundariesForScenery();
    location->calculateDistanceGraph();

    view->centerOn(player->getPos().x, player->getPos().y);

    location->setListener(this, NULL); // must do after creating the location and its contents, so it doesn't try to add items to the scene, etc
    this->c_location = location;

    gui_overlay->setProgress(80);
    qApp->processEvents();

    // set up the view on the RPG world

    const float offset_y = 0.5f;
    float location_width = 0.0f, location_height = 0.0f;
    location->calculateSize(&location_width, &location_height);
    const float extra_offset_c = 5.0f; // so we can still scroll slightly past the boundary, and also that multitouch works beyond the boundary
    //scene->setSceneRect(0, -offset_y, location_width, location_height + 2*offset_y);
    scene->setSceneRect(-extra_offset_c, -offset_y-extra_offset_c, location_width+2*extra_offset_c, location_height + 2*offset_y + 2*extra_offset_c);
    //view->fitInView(0.0f, 0.0f, location->getWidth(), location->getHeight());
    //int pixels_per_unit = 32;
    //view->scale(pixels_per_unit, pixels_per_unit);
    //view->setScale(pixels_per_unit);
    {
        const float desired_width_c = 10.0f;
        float initial_scale = window->width() / desired_width_c;
        LOG("width: %d\n", window->width());
        LOG("initial_scale: %f\n", initial_scale);
        view->setScale(initial_scale);
    }

    int pixels_per_unit = 64;
    float scale = 1.0f/(float)pixels_per_unit;
    QBrush floor_brush(builtin_images["floor"]);
    floor_brush.setTransform(QTransform::fromScale(scale, scale));
    QBrush wall_brush(builtin_images["wall"]);
    wall_brush.setTransform(QTransform::fromScale(scale, scale));

    for(size_t i=0;i<location->getNFloorRegions();i++) {
        FloorRegion *floor_region = location->getFloorRegion(i);
        QPolygonF polygon;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D point = floor_region->getPoint(j);
            //QPointF qpoint(point.x, point.y + offset_y);
            QPointF qpoint(point.x, point.y);
            polygon.push_back(qpoint);
        }
        QGraphicsPolygonItem *item = scene->addPolygon(polygon, Qt::NoPen, floor_brush);
        floor_region->setUserGfxData(item);
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
            float wall_dist = 0.1f;
            wall_polygon.push_back(QPointF(p0.x, p0.y));
            wall_polygon.push_back(QPointF(p0.x + wall_dist * normal_into_wall.x, p0.y + wall_dist * normal_into_wall.y));
            wall_polygon.push_back(QPointF(p1.x + wall_dist * normal_into_wall.x, p1.y + wall_dist * normal_into_wall.y));
            wall_polygon.push_back(QPointF(p1.x, p1.y));
            QGraphicsPolygonItem *wall_item = new QGraphicsPolygonItem(wall_polygon, item);
            wall_item->setPen(Qt::NoPen);
            wall_item->setBrush(wall_brush);
        }
    }
#ifdef DEBUG_SHOW_PATH
    {
        // DEBUG ONLY
        QPen wall_pen(Qt::red);
        for(size_t i=0;i<location->getNBoundaries();i++) {
            const Polygon2D *boundary = location->getBoundary(i);
            for(size_t j=0;j<boundary->getNPoints();j++) {
                Vector2D p0 = boundary->getPoint(j);
                Vector2D p1 = boundary->getPoint((j+1) % boundary->getNPoints());
                //scene->addLine(p0.x, p0.y + offset_y, p1.x, p1.y + offset_y, wall_pen);
                scene->addLine(p0.x, p0.y, p1.x, p1.y, wall_pen);
            }
        }
    }
    {
        // DEBUG ONLY
        QPen wall_pen(Qt::red);
        const Graph *distance_graph = location->getDistanceGraph();
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

    gui_overlay->setProgress(90);
    qApp->processEvents();

    LOG("add graphics items\n");
    for(set<Item *>::iterator iter = location->itemsBegin(); iter != location->itemsEnd(); ++iter) {
        Item *item = *iter;
        this->locationAddItem(location, item);
    }
    for(set<Scenery *>::iterator iter = location->scenerysBegin(); iter != location->scenerysEnd(); ++iter) {
        Scenery *scenery = *iter;
        this->locationAddScenery(location, scenery);
    }

    gui_overlay->setProgress(95);
    qApp->processEvents();

    for(set<Character *>::iterator iter = location->charactersBegin(); iter != location->charactersEnd(); ++iter) {
        Character *character = *iter;
        AnimatedObject *object = new AnimatedObject();
        this->characterUpdateGraphics(character, object);
        scene->addItem(object);
        object->setPos(character->getX(), character->getY());
        int character_size = object->getSize();
        float character_scale = 2.0f / (float)character_size;
        //object->setTransformOriginPoint(-32.0f*character_scale, -46.0f*character_scale);
        object->setTransformOriginPoint(-32.0f/32.0f, -46.0f/32.0f);
        object->setScale(character_scale);

        character->setListener(this, object);
        //item->setAnimationSet("attack"); // test
    }

    /*{
        TextEffect *text_effect = new TextEffect("Welcome to Erebus", 1000);
        text_effect->setPos( player->getPos().x, player->getPos().y );
        scene->addItem(text_effect);
    }*/
    //this->addTextEffect("Welcome to Erebus", player->getPos(), 2000);

    location->initVisibility(player->getPos());
    for(size_t i=0;i<location->getNFloorRegions();i++) {
        FloorRegion *floor_region = location->getFloorRegion(i);
        this->updateVisibilityForFloorRegion(floor_region);
    }

    gui_overlay->unsetProgress();
    qApp->processEvents();

    window->setEnabled(true);
    game_g->getScreen()->setPaused(false);
    game_g->getScreen()->restartElapsedTimer();

    if( quest->getInfo() != '\0' ) {
        // disabled for now due to window too large on Android bug
        //game_g->showInfoDialog("Quest", quest->getInfo());
        stringstream str;
        str << "<html><body>";
        str << "<h1>Quest</h1>";
        str << "<p>" << quest->getInfo() << "</p>";
        str << "</body></html>";
        this->showInfoWindow(str.str().c_str());

        this->journal_ss << "<p><b>Quest Details</b></p>";
        this->journal_ss << "<p>" << quest->getInfo() << "</p>";
    }

    LOG("View is transformed? %d\n", view->isTransformed());
    LOG("done\n");
}

void PlayingGamestate::locationAddItem(const Location *location, Item *item) {
    if( this->c_location == location ) {
        QGraphicsPixmapItem *object = new QGraphicsPixmapItem();
        item->setUserGfxData(object);
        //object->setPixmap( item->getImage()->getPixmap() );
        //object->setPixmap( this->item_images[item->getName().c_str()] );
        //object->setPixmap( this->item_images[item->getImageName().c_str()] );
        /*map<string, QPixmap>::iterator image_iter = this->item_images.find(item->getImageName().c_str());
        if( image_iter == this->item_images.end() ) {
            LOG("failed to find image for item: %s\n", item->getName().c_str());
            LOG("    image name: %s\n", item->getImageName().c_str());
            throw string("Failed to find item's image");
        }
        object->setPixmap( image_iter->second );*/
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
    if( this->c_location == location ) {
        QGraphicsPixmapItem *object = new QGraphicsPixmapItem();
        scenery->setUserGfxData(object);
        this->locationUpdateScenery(scenery);
        scene->addItem(object);
        object->setPos(scenery->getX(), scenery->getY());
        object->setZValue(object->pos().y());
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
        float scenery_scale_w = scenery->getWidth() / object->pixmap().width();
        float scenery_scale_h = scenery->getHeight() / object->pixmap().height();
        float centre_x = 0.5f*object->pixmap().width();
        float centre_y = 0.5f*object->pixmap().height();
        QTransform transform;
        transform = transform.scale(scenery_scale_w, scenery_scale_h);
        transform = transform.translate(-centre_x, -centre_y);
        object->setTransform(transform);
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
    QGraphicsPixmapItem *object = static_cast<QGraphicsPixmapItem *>(scenery->getUserGfxData());
    if( object != NULL ) {
        bool done = false;
        if( scenery->isOpened() ) {
            map<string, QPixmap>::iterator image_iter = this->scenery_opened_images.find(scenery->getImageName().c_str());
            if( image_iter != this->scenery_opened_images.end() ) {
                done = true;
                object->setPixmap( image_iter->second );
            }
        }
        if( !done ) {
            map<string, QPixmap>::iterator image_iter = this->scenery_images.find(scenery->getImageName().c_str());
            if( image_iter == this->scenery_images.end() ) {
                LOG("failed to find image for scenery: %s\n", scenery->getName().c_str());
                LOG("    image name: %s\n", scenery->getImageName().c_str());
                throw string("Failed to find scenery's image");
            }
            done = true;
            object->setPixmap( image_iter->second );
        }
        object->update();
    }
}

void PlayingGamestate::clickedStats() {
    LOG("clickedStats()\n");
    this->clickedCloseSubwindow();

    new StatsWindow(this);
    game_g->getScreen()->setPaused(true);
}

void PlayingGamestate::clickedItems() {
    LOG("clickedItems()\n");
    this->clickedCloseSubwindow();

    new ItemsWindow(this);
    game_g->getScreen()->setPaused(true);
}

void PlayingGamestate::clickedJournal() {
    LOG("clickedJournal()\n");
    stringstream str;
    str << "<html><body>";
    str << "<h1>Journal</h1>";
    str << this->journal_ss.str();
    str << "</body></html>";
    this->showInfoWindow(str.str().c_str());
}

void PlayingGamestate::clickedOptions() {
    LOG("clickedOptions()\n");
    this->clickedCloseSubwindow();

    game_g->getScreen()->setPaused(true);

    QWidget *subwindow = new QWidget();
    this->addWidget(subwindow);

    QVBoxLayout *layout = new QVBoxLayout();
    subwindow->setLayout(layout);

    QPushButton *quitButton = new QPushButton("Quit game");
    quitButton->setFont(game_g->getFontBig());
    quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(quitButton);
    connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));

    QPushButton *closeButton = new QPushButton("Back to game");
    closeButton->setFont(game_g->getFontBig());
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(clickedCloseSubwindow()));

    //subwindow->showFullScreen();
    //game_g->getScreen()->getMainWindow()->hide();
}

void PlayingGamestate::clickedRest() {
    LOG("clickedRest()\n");
    if( c_location->hasEnemies(this) ) {
        game_g->showInfoDialog("Rest", "You cannot rest here - enemies are nearby.");
        return;
    }
    if( game_g->askQuestionDialog("Rest", "Rest until fully healed?") ) {
        int health_restore_percent = 100 - this->player->getHealthPercent();
        int time = (int)(health_restore_percent*10.0f/100.0f + 0.5f);
        if( time == 0 ) {
            time = 1;
        }
        this->player->restoreHealth();
        stringstream str;
        str << "Rested for " << time << " hour";
        if( time > 1 )
            str << "s";
        this->addTextEffect(str.str(), player->getPos(), 2000);
    }
}

void PlayingGamestate::clickedQuit() {
    LOG("clickedQuit()\n");
    this->quitGame();
}

void PlayingGamestate::showInfoWindow(const char *html) {
    LOG("showInfoWindow()\n");
    this->clickedCloseSubwindow();

    game_g->getScreen()->setPaused(true);

    QWidget *subwindow = new QWidget();
    this->addWidget(subwindow);

    QVBoxLayout *layout = new QVBoxLayout();
    subwindow->setLayout(layout);

    QWebView *label = new QWebView();
    label->setHtml(html);
    layout->addWidget(label);

    QPushButton *closeButton = new QPushButton("Continue");
    closeButton->setFont(game_g->getFontBig());
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(clickedCloseSubwindow()));
}

void PlayingGamestate::clickedCloseSubwindow() {
    LOG("clickedCloseSubwindow\n");
    int n_stacked_widgets = this->main_stacked_widget->count();
    if( n_stacked_widgets > 1 ) {
        QWidget *subwindow = this->main_stacked_widget->widget(n_stacked_widgets-1);
        this->main_stacked_widget->removeWidget(subwindow);
        subwindow->deleteLater();
        if( n_stacked_widgets == 2 ) {
            game_g->getScreen()->setPaused(false);
        }
    }
}

void PlayingGamestate::quitGame() {
    //qApp->quit();

    if( game_g->askQuestionDialog("Quit", "Are you sure you wish to quit?") ) {
        GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
        game_g->pushMessage(game_message);
    }
}

void PlayingGamestate::update() {
    //LOG("update: %d\n", time_ms);

    if( game_g->getScreen()->isPaused() ) {
        return;
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
                if( !c_location->intersectSweptSquareWithBoundaries(&hit_pos, false, player->getPos(), character->getPos(), 0.0f, Location::INTERSECTTYPE_VISIBILITY, NULL) ) {
                    is_visible = true;
                }
            }
            character->setVisible(is_visible);
            AnimatedObject *object = static_cast<AnimatedObject *>(character->getListenerData());
            object->setVisible(is_visible);
        }
    }

    scene->advance();
    gui_overlay->update(); // force the GUI overlay to be updated every frame (otherwise causes drawing problems on Windows at least)

    //vector< set<Character *>::iterator > delete_characters;
    vector<Character *> delete_characters;
    for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        Character *character = *iter;
        if( character->update(this) ) {
            LOG("character is about to die: %s\n", character->getName().c_str());
            //delete_characters.push_back(iter);
            delete_characters.push_back(character);
        }
    }
    /*for(vector< set<Character *>::iterator >::iterator iter2 = delete_characters.begin(); iter2 != delete_characters.end(); ++iter2) {
        set<Character *>::iterator iter = *iter2;*/
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
            game_g->showInfoDialog("Game over", "You have died!");
            GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
            game_g->pushMessage(game_message);
        }
    }
    if( delete_characters.size() > 0 ) {
        if( !this->quest->isCompleted() && this->quest->testIfComplete() ) {
            game_g->showInfoDialog("Game over", "You have completed the quest! Now return to the dungeon exit.");
        }
    }
}

void PlayingGamestate::characterUpdateGraphics(const Character *character, void *user_data) {
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    object->clearAnimationLayers();
    if( character == player ) {
        object->addAnimationLayer( this->animation_layers["clothes"] );
        object->addAnimationLayer( this->animation_layers["head"] );
        if( character->getCurrentWeapon() != NULL ) {
            object->addAnimationLayer( this->animation_layers[ character->getCurrentWeapon()->getAnimationName().c_str() ] );
        }
        if( character->getCurrentShield() != NULL ) {
            object->addAnimationLayer( this->animation_layers[ character->getCurrentShield()->getAnimationName().c_str() ] );
        }
    }
    else {
        object->addAnimationLayer( this->animation_layers[ character->getAnimationName() ] );
    }
}

void PlayingGamestate::characterTurn(const Character *character, void *user_data, Vector2D dir) {
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
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
    object->setDirection(direction);
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
        this->characterTurn(character, user_data, vdir);
    }
    if( character == this->player ) {
        // handle traps
        vector<Trap *> delete_traps;
        for(set<Trap *>::iterator iter = this->c_location->trapsBegin(); iter != this->c_location->trapsEnd(); ++iter) {
            Trap *trap = *iter;
            if( trap->isSetOff(character) ) {
                LOG("character: %s has set of trap at %f, %f\n", character->getName().c_str(), trap->getX(), trap->getY());
                delete_traps.push_back(trap);
                int damage = rollDice(3, 6, 0);
                character->decreaseHealth(this, damage);
                this->addTextEffect("You have set off a trap!\nAn arrow shoots out from the wall and hits you!", player->getPos(), 2000);
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

void PlayingGamestate::characterSetAnimation(const Character *character, void *user_data, string name) {
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    object->setAnimationSet(name);
}

void PlayingGamestate::characterDeath(Character *character, void *user_data) {
    AnimatedObject *object = static_cast<AnimatedObject *>(user_data);
    character->setListener(NULL, NULL);
    delete object;
}

/*void PlayingGamestate::mouseClick(int m_x, int m_y) {
    qDebug("PlayingGamestate::mouseClick(%d, %d)", m_x, m_y);
}*/

void PlayingGamestate::clickedMainView(float scene_x, float scene_y) {
    if( game_g->getScreen()->isPaused() ) {
        game_g->getScreen()->setPaused(false);
    }

    if( player != NULL && !player->isDead() ) {
        //player->setPos(scene_x, scene_y);

        Vector2D dest(scene_x, scene_y);

        bool done = false;
        //const float click_tol_c = 0.0f;
        //const float click_tol_c = 0.5f;
        const float click_tol_npc_c = 0.25f;
        const float click_tol_items_c = 0.0f;
        const float click_tol_scenery_c = 0.0f;

        // search for clicking on an NPC
        float min_dist = 0.0f;
        Character *target_npc = NULL;
        for(set<Character *>::iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
            Character *character = *iter;
            if( character == player )
                continue;
            if( !character->isVisible() )
                continue;
            float dist_from_click = (dest - character->getPos()).magnitude();
            if( dist_from_click <= npc_radius_c + click_tol_npc_c ) {
                if( target_npc == NULL || dist_from_click < min_dist ) {
                    done = true;
                    target_npc = character;
                    min_dist = dist_from_click;
                }
            }
        }
        player->setTargetNPC(target_npc); // n.b., if no NPC selected, we therefore set to NULL

        if( !done ) {
            // search for clicking on an item
            Item *picked_item = NULL;
            for(set<Item *>::iterator iter = c_location->itemsBegin(); iter != c_location->itemsEnd(); ++iter) {
                Item *item = *iter;
                float icon_width = item->getIconWidth();
                float dist_from_click = (dest - item->getPos()).magnitude();
                float dist_from_player = (player->getPos() - item->getPos()).magnitude();
                if( dist_from_click <= sqrt(0.5f) * icon_width + click_tol_items_c && dist_from_player <= npc_radius_c + sqrt(0.5f)*icon_width ) {
                    if( picked_item == NULL || dist_from_click < min_dist ) {
                        done = true;
                        picked_item = item;
                        min_dist = dist_from_click;
                    }
                }
            }
            if( picked_item != NULL ) {
                this->addTextEffect(picked_item->getName(), player->getPos(), 2000);
                player->pickupItem(picked_item);
            }
        }

        Scenery *ignore_scenery = NULL;
        if( !done ) {
            // search for clicking on a scenery
            Scenery *selected_scenery = NULL;
            for(set<Scenery *>::iterator iter = c_location->scenerysBegin(); iter != c_location->scenerysEnd(); ++iter) {
                Scenery *scenery = *iter;
                Vector2D scenery_pos = scenery->getPos();
                float scenery_width = scenery->getWidth();
                float scenery_height = scenery->getHeight();
                float dist_from_click = distFromBox2D(scenery_pos, scenery_width, scenery_height, dest);
                LOG("dist_from_click for scenery %s : %f", scenery->getName().c_str(), dist_from_click);
                /*if( dest.x >= scenery_pos.x - 0.5f * scenery_width - npc_radius_c && dest.x <= scenery_pos.x + 0.5f * scenery_width + npc_radius_c &&
                    dest.y >= scenery_pos.y - 0.5f * scenery_height - npc_radius_c && dest.y <= scenery_pos.y + 0.5f * scenery_height + npc_radius_c ) {*/
                if( dist_from_click <= npc_radius_c ) {
                    // clicked on or near this scenery
                    ignore_scenery = scenery;
                }
                /*if( dest.x >= scenery_pos.x - 0.5f * scenery_width && dest.x <= scenery_pos.x + 0.5f * scenery_width &&
                    dest.y >= scenery_pos.y - 0.5f * scenery_height && dest.y <= scenery_pos.y + 0.5f * scenery_height ) {*/
                if( dist_from_click <= click_tol_scenery_c ) {
                    // clicked on this scenery
                    /*Vector2D player_pos = player->getPos();
                    float player_dist_x = abs(player_pos.x - scenery_pos.x) - 0.5f * scenery_width;
                    float player_dist_y = abs(player_pos.y - scenery_pos.y) - 0.5f * scenery_height;
                    float player_dist = player_dist_x > player_dist_y ? player_dist_x : player_dist_y;*/
                    float player_dist = distFromBox2D(scenery_pos, scenery_width, scenery_height, player->getPos());
                    LOG("    player_dist : %f", player_dist);
                    if( player_dist <= npc_radius_c + 0.5f ) {
                        if( selected_scenery == NULL || dist_from_click < min_dist ) {
                            LOG("    selected!\n");
                            done = true;
                            selected_scenery = scenery;
                            min_dist = dist_from_click;
                        }
                    }
                }
            }

            if( selected_scenery != NULL ) {
                if( selected_scenery->getNItems() > 0 ) {
                    bool all_gold = true;
                    for(set<Item *>::iterator iter = selected_scenery->itemsBegin(); iter != selected_scenery->itemsEnd(); ++iter) {
                        Item *item = *iter;
                        if( item->getType() != ITEMTYPE_CURRENCY ) {
                            all_gold = false;
                        }
                        c_location->addItem(item, player->getX(), player->getY());
                    }
                    selected_scenery->eraseAllItems();
                    this->addTextEffect(all_gold ? "Found some gold!" : "Found some items!", player->getPos(), 2000);
                }

                if( !selected_scenery->isOpened() ) {
                    selected_scenery->setOpened(true);
                }

                if( selected_scenery->isDoor() ) {
                    LOG("clicked on a door\n");
                    bool is_locked = false;
                    if( selected_scenery->isLocked() ) {
                        // can we unlock it?
                        is_locked = true;
                        string unlock_item_name = selected_scenery->getUnlockItemName();
                        if( unlock_item_name.length() > 0 ) {
                            //LOG("search for %s\n", unlock_item_name.c_str());
                            for(set<Item *>::const_iterator iter = player->itemsBegin(); iter != player->itemsEnd() && is_locked; ++iter) {
                                const Item *item = *iter;
                                //LOG("    compare to: %s\n", item->getKey().c_str());
                                if( item->getKey() == unlock_item_name ) {
                                    is_locked = false;
                                }
                            }
                        }
                        if( is_locked ) {
                            this->addTextEffect("The door is locked!", player->getPos(), 2000);
                        }
                        else {
                            this->addTextEffect("You unlock the door.", player->getPos(), 2000);
                            selected_scenery->setLocked(false); // we'll delete the door anyway below, but just to be safe...
                        }
                    }
                    if( !is_locked ) {
                        // open door
                        c_location->removeScenery(selected_scenery);
                        delete selected_scenery;
                        selected_scenery = NULL;
                    }
                }
                else if( selected_scenery->isExit() ) {
                    LOG("clicked on an exit");
                    // exit
                    this->clickedCloseSubwindow(); // just in case
                    new CampaignWindow(this);
                    game_g->getScreen()->setPaused(true);
                    this->player->restoreHealth();
                }
            }

        }

        if( dest != player->getPos() ) {
            player->setDestination(dest.x, dest.y, ignore_scenery);
            if( player->calculateItemsWeight() > player->getCanCarryWeight() ) {
                this->addTextEffect("You are carrying too much weight to move!", player->getPos(), 2000);
            }
        }
    }
}

void PlayingGamestate::updateVisibilityForFloorRegion(FloorRegion *floor_region) {
    QGraphicsPolygonItem *gfx_item = static_cast<QGraphicsPolygonItem *>(floor_region->getUserGfxData());
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
    }
}

void PlayingGamestate::updateVisibility(Vector2D pos) {
    vector<FloorRegion *> update_regions = this->c_location->updateVisibility(pos);
    for(vector<FloorRegion *>::iterator iter = update_regions.begin(); iter != update_regions.end(); ++iter) {
        FloorRegion *floor_region = *iter;
        updateVisibilityForFloorRegion(floor_region);
    }
}

void PlayingGamestate::addWidget(QWidget *widget) {
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);
}

void PlayingGamestate::addTextEffect(string text, Vector2D pos, int duration_ms) {
    TextEffect *text_effect = new TextEffect(this->view, text.c_str(), duration_ms);
    float font_scale = 1.0f/view->getScale();
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

void PlayingGamestate::addStandardItem(Item *item) {
    this->standard_items[item->getKey()] = item;
}

Item *PlayingGamestate::cloneStandardItem(string name) const {
    map<string, Item *>::const_iterator iter = this->standard_items.find(name);
    if( iter == this->standard_items.end() ) {
        LOG("can't clone standard item which doesn't exist: %s\n", name.c_str());
        throw string("Unknown standard item");
    }
    const Item *item = iter->second;
    return item->clone();
}

Currency *PlayingGamestate::cloneGoldItem(int value) const {
    Currency *item = static_cast<Currency *>(this->cloneStandardItem("Gold"));
    item->setValue(value);
    return item;
}

QPixmap &PlayingGamestate::getItemImage(string name) {
    map<string, QPixmap>::iterator image_iter = this->item_images.find(name.c_str());
    if( image_iter == this->item_images.end() ) {
        LOG("failed to find image for item: %s\n", name.c_str());
        LOG("    image name: %s\n", name.c_str());
        throw string("Failed to find item's image");
    }
    return image_iter->second;
}

Game::Game() {
    game_g = this;

    // initialise paths
    QString pathQt (QDesktopServices::storageLocation (QDesktopServices::DataLocation));
    QString nativePath(QDir::toNativeSeparators(pathQt));
    application_path = nativePath.toStdString();
    logfilename = getApplicationFilename("log.txt");
    oldlogfilename = getApplicationFilename("log_old.txt");
    // n.b., not safe to use logging until after copied/removed old log files!
    qDebug("application_path: %s", application_path.c_str());
    qDebug("logfilename: %s", logfilename.c_str());
    qDebug("oldlogfilename: %s", oldlogfilename.c_str());

    remove(oldlogfilename.c_str());
    rename(logfilename.c_str(), oldlogfilename.c_str());
    remove(logfilename.c_str());

    LOG("Initialising Log File...\n");
    LOG("erebus startup\n");
    LOG("Version %d.%d\n", versionMajor, versionMinor);

#ifdef _DEBUG
    LOG("Running in Debug mode\n");
#else
    LOG("Running in Release mode\n");
#endif

#if defined(Q_WS_SIMULATOR)
    LOG("Platform: Qt Smartphone Simulator\n");
#elif defined(_WIN32)
    LOG("Platform: Windows\n");
#elif defined(Q_WS_MAEMO_5)
    // must be before __linux, as Maemo/Meego also defines __linux
    LOG("Platform: Maemo/Meego\n");
#elif __linux
    LOG("Platform: Linux\n");
#elif defined(__APPLE__) && defined(__MACH__)
    LOG("Platform: MacOS X\n");
#elif defined(Q_OS_SYMBIAN)
    LOG("Platform: Symbian\n");
#elif defined(Q_OS_ANDROID)
    LOG("Platform: Android\n")
#else
    LOG("Platform: UNKNOWN\n");
#endif
}

void Game::run() {
    screen = new Screen();

    // setup fonts
    MainWindow *window = game_g->getScreen()->getMainWindow();
    if( mobile_c ) {
        QFont new_font = window->font();
#if defined(Q_OS_ANDROID)
        /*
        // make work better on Android phones with crappy resolution
        // these settings determined by experimenting with emulator...
        int min_size = min(QApplication::desktop()->width(), QApplication::desktop()->height());
        qDebug("current font size: %d", new_font.pointSize());
        qDebug("min_size: %d", min_size);
        if( min_size < 320 ) {
            newFont.setPointSize(new_font.pointSize() - 6);
        }
        else if( min_size < 480 ) {
            newFont.setPointSize(new_font.pointSize() - 4);
        }*/
        this->font_small = QFont(new_font);
        this->font_small.setPointSize(font_small.pointSize() - 4);
        this->font_std = new_font;
        this->font_big = new_font;
#else
        this->font_small = QFont(new_font);
        this->font_small.setPointSize(font_small.pointSize() - 4);
        this->font_std = new_font;
        this->font_big = new_font;
#endif
    }
    else {
        this->font_small = QFont("Verdana", 12);
        this->font_std = QFont("Verdana", 16);
        this->font_big = QFont("Verdana", 48, QFont::Bold);
    }

    gamestate = new OptionsGamestate();

    screen->runMainLoop();

    delete gamestate;
    delete screen;
}

void Game::update() {
    while( !message_queue.empty() ) {
        GameMessage *message = message_queue.front();
        message_queue.pop();
        try {
            switch( message->getGameMessageType() ) {
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING:
            {
                delete gamestate;
                gamestate = NULL;
                PlayingGamestate *playing_gamestate = new PlayingGamestate();
                gamestate = playing_gamestate;
                playing_gamestate->loadQuest();
                this->getScreen()->getMainWindow()->unsetCursor();
                break;
            }
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS:
                delete gamestate;
                gamestate = NULL;
                gamestate = new OptionsGamestate();
                break;
            default:
                ASSERT_LOGGER(false);
            }
        }
        catch(string &error) {
            LOG("exception creating new gamestate: %s\n", error.c_str());
            this->getScreen()->getMainWindow()->unsetCursor();
            this->getScreen()->getMainWindow()->setEnabled(true);
            if( gamestate != NULL ) {
                delete gamestate;
                gamestate = NULL;
            }
            delete message;
            stringstream str;
            str << "Failed to load game data:\n" << error;
            game_g->showErrorDialog(str.str().c_str());
            qApp->quit();
        }
        delete message;
    }

    if( gamestate != NULL ) {
        gamestate->update();
    }
}

/*void Game::mouseClick(int m_x, int m_y) {
    gamestate->mouseClick(m_x, m_y);
}*/

string Game::getApplicationFilename(const char *name) {
    // not safe to use LOG here, as logfile may not have been initialised!
    QString pathQt = QString(application_path.c_str()) + QString("/") + QString(name);
    QString nativePath(QDir::toNativeSeparators(pathQt));
    string filename = nativePath.toStdString();
    qDebug("getApplicationFilename returns: %s", filename.c_str());
    return filename;
}

void Game::log(const char *text, ...) {
    FILE *logfile = fopen(logfilename.c_str(), "at+");
    va_list vlist;
    char buffer[65536] = "";
    va_start(vlist, text);
    vsprintf(buffer,text,vlist);
    if( logfile != NULL )
        fprintf(logfile,buffer);
    //printf(buffer);
    qDebug("%s", buffer);
    va_end(vlist);
    if( logfile != NULL )
        fclose(logfile);
}

QPixmap Game::loadImage(const char *filename, bool clip, int xpos, int ypos, int width, int height) const {
    // need to use QImageReader - QPixmap::load doesn't work on large images on Symbian!
    QImageReader reader(filename);
    if( clip ) {
        reader.setClipRect(QRect(xpos, ypos, width, height));
    }
    QImage image = reader.read();
    if( image.isNull() ) {
        LOG("failed to read image: %s\n", filename);
        LOG("error: %d\n", reader.error());
        LOG("error string: %s\n", reader.errorString().toStdString().c_str());
        stringstream error;
        error << "Failed to load image: " << filename;
        throw error.str();
    }
    QPixmap pixmap = QPixmap::fromImage(image);
    if( pixmap.isNull() ) {
        LOG("failed to convert image to pixmap: %s\n", filename);
        throw string("Failed to convert image to pixmap");
    }
    return pixmap;
}

void Game::showErrorDialog(const char *message) {
    LOG("Game::showErrorDialog: %s\n", message);
    //LOG("1\n");
    this->getScreen()->enableUpdateTimer(false);
    //LOG("2\n");
    QMessageBox::critical(this->getScreen()->getMainWindow(), "Error", message);
    //LOG("3\n");
    this->getScreen()->enableUpdateTimer(true);
    //LOG("4\n");
}

void Game::showInfoDialog(const char *title, const char *message) {
    LOG("Game::showInfoDialog: %s\n", message);
    this->getScreen()->enableUpdateTimer(false);
    QMessageBox::information(this->getScreen()->getMainWindow(), title, message);
    this->getScreen()->enableUpdateTimer(true);
}

bool Game::askQuestionDialog(const char *title, const char *message) {
    LOG("Game::askQuestionWindow: %s\n", message);
    this->getScreen()->enableUpdateTimer(false);
    int res = QMessageBox::question(this->getScreen()->getMainWindow(), title, message, QMessageBox::Yes, QMessageBox::No);
    this->getScreen()->enableUpdateTimer(true);
    LOG("    answer is %d\n", res);
    return res == QMessageBox::Yes;
}
