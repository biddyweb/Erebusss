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

string savegame_root = "savegame_";
string savegame_ext = ".xml";
string savegame_folder = "savegames/";

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

AnimationLayer *AnimationLayer::create(const string &filename, const vector<AnimationLayerDefinition> &animation_layer_definitions) {
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
    if( animation_layer == NULL ) {
        throw string("AnimatedObject::addAnimationLayer received NULL animated_layer");
    }
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

void AnimatedObject::setAnimationSet(const string &name) {
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
    //this->setFont(game_g->getFontStd());
    this->setFont(game_g->getFontSmall());

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

OptionsGamestate::OptionsGamestate() : main_stacked_widget(NULL), load_list(NULL)
{
    LOG("OptionsGamestate::OptionsGamestate()\n");
    optionsGamestate = this;

#ifndef Q_OS_ANDROID
    this->music = NULL;
    if( !mobile_c ) {
        /*music = new Phonon::MediaObject(qApp);
        connect(music, SIGNAL(stateChanged(Phonon::State,Phonon::State)), this, SLOT(mediaStateChanged(Phonon::State,Phonon::State)));
        music->setCurrentSource(Phonon::MediaSource("music/no_more_magic.ogg"));
        Phonon::AudioOutput *audioOutput = new Phonon::AudioOutput(Phonon::GameCategory, qApp);
        Phonon::Path audioPath = Phonon::createPath(music, audioOutput);*/
        music = game_g->loadSound("music/no_more_magic.ogg");
        music->play();
    }
#endif

    MainWindow *window = game_g->getScreen()->getMainWindow();
    QFont font = game_g->getFontBig();
    window->setFont(font);

    this->main_stacked_widget = new QStackedWidget();
    main_stacked_widget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    window->setCentralWidget(main_stacked_widget);

    QWidget *centralWidget = new QWidget(window);
    //centralWidget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    //window->setCentralWidget(centralWidget);
    main_stacked_widget->addWidget(centralWidget);

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
    game_g->initButton(startButton);
    startButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(startButton);
    connect(startButton, SIGNAL(clicked()), this, SLOT(clickedStart()));

    QPushButton *loadButton = new QPushButton("Load game");
    game_g->initButton(loadButton);
    loadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(loadButton);
    connect(loadButton, SIGNAL(clicked()), this, SLOT(clickedLoad()));

#ifndef Q_OS_ANDROID
    // applications don't quit on Android.
    QPushButton *quitButton = new QPushButton("Quit game");
    game_g->initButton(quitButton);
    quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(quitButton);
    connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));
#endif

    window->setEnabled(true); // ensure is enabled - may have been disabled if fail to load game, and return to OptionsGamestate
}

OptionsGamestate::~OptionsGamestate() {
    LOG("OptionsGamestate::~OptionsGamestate()\n");
    /*VI_flush(0); // delete all the gamestate objects, but leave the game level objects (which should be set at persistence level -1)
    VI_GraphicsEnvironment *genv = game_g->getGraphicsEnvironment();
    game_g->getGraphicsEnvironment()->setPanel(NULL); // as the main panel is now destroyed
    */
#ifndef Q_OS_ANDROID
    if( music != NULL ) {
        delete music;
    }
#endif

    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->centralWidget()->deleteLater();
    window->setCentralWidget(NULL);

    optionsGamestate = NULL;
    LOG("deleted OptionsGamestate\n");
}

/*#ifndef Q_OS_ANDROID
void OptionsGamestate::mediaStateChanged(Phonon::State newstate, Phonon::State oldstate) {
    LOG("OptionsGamestate::mediaStateChanged(%d, %d)\n", newstate, oldstate);
    if( newstate == Phonon::ErrorState ) {
        LOG("phonon reports error!: %d\n", music->errorType());
        LOG("error string: %s\n", music->errorString().toStdString().c_str());
    }
}
#endif*/

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
    /*game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);
    GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING_LOAD);
    game_g->pushMessage(game_message);*/

    QWidget *widget = new QWidget();
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);

    QVBoxLayout *layout = new QVBoxLayout();
    widget->setLayout(layout);

    load_list = new ScrollingListWidget();
    if( !mobile_c ) {
        QFont list_font = load_list->font();
        list_font.setPointSize( list_font.pointSize() + 8 );
        load_list->setFont(list_font);
    }
    layout->addWidget(load_list);
    load_list->setSelectionMode(QAbstractItemView::SingleSelection);

    QDir dir( QString(game_g->getApplicationFilename(savegame_folder).c_str()) );
    QStringList filter;
    filter << "*" + QString( savegame_ext.c_str() );
    QStringList files = dir.entryList(filter);
    for(int i=0;i<files.size();i++) {
        const QString &file = files.at(i);
        load_list->addItem(file);
    }

    load_list->setCurrentRow(0);

    QPushButton *loadButton = new QPushButton("Load");
    game_g->initButton(loadButton);
    //loadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(loadButton);
    connect(loadButton, SIGNAL(clicked()), this, SLOT(clickedLoadGame()));

    QPushButton *closeButton = new QPushButton("Cancel");
    game_g->initButton(closeButton);
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeAllSubWindows()));
}

void OptionsGamestate::clickedLoadGame() {
    LOG("OptionsGamestate::clickedLoadGame()\n");
    game_g->getScreen()->getMainWindow()->setCursor(Qt::WaitCursor);
    //GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING_LOAD);
    int index = load_list->currentRow();
    LOG("clicked index %d\n", index);
    if( index == -1 ) {
        return;
    }
    ASSERT_LOGGER(index >= 0 && index < load_list->count() );
    QString filename = this->load_list->item(index)->text();
    LoadGameMessage *game_message = new LoadGameMessage(filename.toStdString());
    game_g->pushMessage(game_message);
}

void OptionsGamestate::closeAllSubWindows() {
    LOG("OptionsGamestate::closeAllSubWindows");
    while( this->main_stacked_widget->count() > 1 ) {
        QWidget *subwindow = this->main_stacked_widget->widget(this->main_stacked_widget->count()-1);
        this->main_stacked_widget->removeWidget(subwindow);
        subwindow->deleteLater();
    }
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
    QPointF zoom_centre = this->mapToScene( this->mapFromGlobal( QCursor::pos() ) );
    if( in ) {
        float n_scale = c_scale * factor_c;
        this->setScale(zoom_centre, n_scale);
    }
    else {
        float n_scale = c_scale / factor_c;
        this->setScale(zoom_centre, n_scale);
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

void MainGraphicsView::setScale(QPointF centre, float c_scale) {
    LOG("MainGraphicsView::setScale((%f, %f), %f)\n", centre.x(), centre.y(), c_scale);
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
    painter.setFont( game_g->getFontSmall() );
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

    html += "<b>Fighting Prowess:</b> " + QString::number(player->getFP()) + "<br/>";
    html += "<b>Bow Skill:</b> " + QString::number(player->getBS()) + "<br/>";
    html += "<b>Strength:</b> " + QString::number(player->getStrength()) + "<br/>";
    html += "<b>Attacks:</b> " + QString::number(player->getAttacks()) + "<br/>";
    html += "<b>Mind:</b> " + QString::number(player->getMind()) + "<br/>";
    html += "<b>Dexterity:</b> " + QString::number(player->getDexterity()) + "<br/>";
    html += "<b>Bravery:</b> " + QString::number(player->getBravery()) + "<br/>";
    html += "<b>Speed:</b> " + QString::number(player->getSpeed()) + "<br/>";

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
    game_g->initButton(closeButton);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeSubWindow()));
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
        game_g->initButton(viewAllButton);
        h_layout->addWidget(viewAllButton);
        connect(viewAllButton, SIGNAL(clicked()), this, SLOT(clickedViewAll()));

        QPushButton *viewWeaponsButton = new QPushButton("Weapons");
        game_g->initButton(viewWeaponsButton);
        h_layout->addWidget(viewWeaponsButton);
        connect(viewWeaponsButton, SIGNAL(clicked()), this, SLOT(clickedViewWeapons()));

        QPushButton *viewAmmoButton = new QPushButton("Ammo");
        game_g->initButton(viewAmmoButton);
        h_layout->addWidget(viewAmmoButton);
        connect(viewAmmoButton, SIGNAL(clicked()), this, SLOT(clickedViewAmmo()));

        QPushButton *viewShieldsButton = new QPushButton("Shields");
        game_g->initButton(viewShieldsButton);
        h_layout->addWidget(viewShieldsButton);
        connect(viewShieldsButton, SIGNAL(clicked()), this, SLOT(clickedViewShields()));

        QPushButton *viewArmourButton = new QPushButton("Armour");
        game_g->initButton(viewArmourButton);
        h_layout->addWidget(viewArmourButton);
        connect(viewArmourButton, SIGNAL(clicked()), this, SLOT(clickedViewArmour()));

        QPushButton *viewMagicButton = new QPushButton("Magic");
        game_g->initButton(viewMagicButton);
        h_layout->addWidget(viewMagicButton);
        connect(viewMagicButton, SIGNAL(clicked()), this, SLOT(clickedViewMagic()));

        QPushButton *viewMiscButton = new QPushButton("Misc");
        game_g->initButton(viewMiscButton);
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

        armButton = new QPushButton(""); // text set in changedSelectedItem()
        game_g->initButton(armButton);
        h_layout->addWidget(armButton);
        connect(armButton, SIGNAL(clicked()), this, SLOT(clickedArmWeapon()));

        wearButton = new QPushButton(""); // text set in changedSelectedItem()
        game_g->initButton(wearButton);
        h_layout->addWidget(wearButton);
        connect(wearButton, SIGNAL(clicked()), this, SLOT(clickedWearArmour()));

        useButton = new QPushButton(""); // text set in changedSelectedItem()
        game_g->initButton(useButton);
        h_layout->addWidget(useButton);
        connect(useButton, SIGNAL(clicked()), this, SLOT(clickedUseItem()));

        dropButton = new QPushButton("Drop Item");
        game_g->initButton(dropButton);
        h_layout->addWidget(dropButton);
        connect(dropButton, SIGNAL(clicked()), this, SLOT(clickedDropItem()));

        infoButton = new QPushButton("Info");
        game_g->initButton(infoButton);
        h_layout->addWidget(infoButton);
        connect(infoButton, SIGNAL(clicked()), this, SLOT(clickedInfo()));
    }

    QPushButton *closeButton = new QPushButton("Continue");
    game_g->initButton(closeButton);
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
}

/*void ItemsWindow::setWeightLabel(int weight) {
    this->weightLabel->setText("Weight: " + QString::number(weight));
}*/

void ItemsWindow::setWeightLabel() {
    Character *player = this->playing_gamestate->getPlayer();
    int weight = player->calculateItemsWeight();
    this->weightLabel->setText("Weight: " + QString::number(weight));
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
    stringstream str;
    str << "<html><body>";
    str << "<h2>" << item->getName() << "</h2>";
    str << "<p>";
    if( item->getType() == ITEMTYPE_WEAPON ) {
        str << "<b>Type:</b> Weapon<br/>";
        const Weapon *weapon = static_cast<const Weapon *>(item);
        int damageX = 0, damageY = 0, damageZ = 0;
        weapon->getDamage(&damageX, &damageY, &damageZ);
        if( damageZ != 0 ) {
            str << "<b>Damage:</b> " << damageX << "D" << damageY << "+" << damageZ << "<br/>";
        }
        else {
            str << "<b>Damage:</b> " << damageX << "D" << damageY << "<br/>";
        }
        str << "<b>Two Handed?:</b> " << (weapon->isTwoHanded() ? "Yes" : "No") << "<br/>";
        str << "<b>Ranged?:</b> " << (weapon->isRanged() ? "Yes" : "No") << "<br/>";
        if( weapon->isRanged() ) {
            str << "<b>Ammo:</b> " << weapon->getAmmoKey() << "<br/>";
        }
    }
    else if( item->getType() == ITEMTYPE_SHIELD ) {
        str << "<b>Type:</b> Shield<br/>";
        //const Shield *shield = static_cast<const Shield *>(item);
    }
    else if( item->getType() == ITEMTYPE_ARMOUR ) {
        str << "<b>Type:</b> Armour<br/>";
        //const Armour *armour = static_cast<const Armour *>(item);
    }
    else if( item->getType() == ITEMTYPE_AMMO ) {
        str << "<b>Type:</b> Ammo<br/>";
        const Ammo *ammo = static_cast<const Ammo *>(item);
        str << "<b>Amount:</b> " << ammo->getAmount() << "<br/>";
    }
    str << "<b>Weight: </b>" << item->getWeight() << "<br/>";
    str << "<b>Magical: </b>" << (item->isMagical() ? "Yes" : "No") << "<br/>";
    if( item->getRating() > 0 ) {
        str << "<b>Rating:</b> " << item->getRating() << "<br/>";
    }
    str << "</p>";
    if( item->getDescription().length() > 0 ) {
        str << item->getDescription();
    }
    str << "</body></html>";
    playing_gamestate->showInfoWindow(str.str());
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
    playing_gamestate(playing_gamestate), goldLabel(NULL), list(NULL), items(items), costs(costs), player_list(NULL)
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
        game_g->initButton(sellButton);
        h_layout->addWidget(sellButton);
        connect(sellButton, SIGNAL(clicked()), this, SLOT(clickedSell()));

        QPushButton *buyButton = new QPushButton("Buy");
        game_g->initButton(buyButton);
        h_layout->addWidget(buyButton);
        connect(buyButton, SIGNAL(clicked()), this, SLOT(clickedBuy()));
    }

    QPushButton *closeButton = new QPushButton("Finish Trading");
    game_g->initButton(closeButton);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeSubWindow()));
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
            game_g->initButton(shopButton);
            QVariant variant = qVariantFromValue((void *)shop);
            shopButton->setProperty("shop", variant);
            //shopButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            layout->addWidget(shopButton);
            connect(shopButton, SIGNAL(clicked()), this, SLOT(clickedShop()));
        }

        QPushButton *trainingButton = new QPushButton("Training");
        game_g->initButton(trainingButton);
        //trainingButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(trainingButton);
        connect(trainingButton, SIGNAL(clicked()), this, SLOT(clickedTraining()));

        close_text = playing_gamestate->getQuest()->isCompleted() ? "Start next Quest" : "Continue your Quest";
    }

    QPushButton *closeButton = new QPushButton(close_text);
    game_g->initButton(closeButton);
    //closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(clickedClose()));
}

void CampaignWindow::clickedClose() {
    LOG("CampaignWindow::clickedClose()\n");
    if( playing_gamestate->getQuest()->isCompleted() && playing_gamestate->isLastQuest() ) {
        GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
        game_g->pushMessage(game_message);
    }
    else {
        this->playing_gamestate->closeSubWindow();
    }
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
    game_g->showInfoDialog("Training", "This feature is not yet implemented");
}

SaveGameWindow::SaveGameWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate), list(NULL), edit(NULL)
{
    playing_gamestate->addWidget(this);

    QFont font = game_g->getFontBig();
    this->setFont(font);

    QVBoxLayout *layout = new QVBoxLayout();
    this->setLayout(layout);

    list = new ScrollingListWidget();
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
        //saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(saveButton);
        connect(saveButton, SIGNAL(clicked()), this, SLOT(clickedSave()));

        QPushButton *deleteButton = new QPushButton("Delete File");
        game_g->initButton(deleteButton);
        //deleteButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        h_layout->addWidget(deleteButton);
        connect(deleteButton, SIGNAL(clicked()), this, SLOT(clickedDelete()));
    }

    QPushButton *closeButton = new QPushButton("Cancel");
    game_g->initButton(closeButton);
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
        filename += QString::number(date.year());
        filename += "-";
        filename += QString::number(date.month());
        filename += "-";
        filename += QString::number(date.daysInMonth());
        filename += "_";
        QTime time = date_time.time();
        filename += QString::number(time.hour());
        filename += "-";
        filename += QString::number(time.minute());
        filename += "-";
        filename += QString::number(time.second());
        //filename += date_time.toString();

        QWidget *subwindow = new QWidget();
        playing_gamestate->addWidget(subwindow);

        QVBoxLayout *layout = new QVBoxLayout();
        subwindow->setLayout(layout);

        QLabel *label = new QLabel("Choose a filename:");
        layout->addWidget(label);

        this->edit = new QLineEdit(filename);
        // disallow: \ / : * ? " < > |
        QRegExp rx("[^\\\\/:*?\"<>|]*");
        QValidator *validator = new QRegExpValidator(rx, this);
        this->edit->setValidator(validator);
        layout->addWidget(edit);
        this->edit->setFocus();
        connect(this->edit, SIGNAL(returnPressed()), this, SLOT(clickedSaveNew()));

        QPushButton *saveButton = new QPushButton("Save game");
        game_g->initButton(saveButton);
        saveButton->setFont(game_g->getFontBig());
        saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(saveButton);
        connect(saveButton, SIGNAL(clicked()), this, SLOT(clickedSaveNew()));

        QPushButton *closeButton = new QPushButton("Cancel");
        game_g->initButton(closeButton);
        closeButton->setFont(game_g->getFontBig());
        closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(closeButton);
        connect(closeButton, SIGNAL(clicked()), playing_gamestate, SLOT(closeAllSubWindows()));
    }
    else {
        QString filename = list->item(index)->text();
        LOG("save as existing file: %s\n", filename.toStdString().c_str());
        if( playing_gamestate->saveGame(filename.toStdString()) ) {
            game_g->showInfoDialog("Saved Game", "The game has been successfully saved.");
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
        if( game_g->askQuestionDialog("Delete Save Game", "Are you sure you wish to delete save game: " + filename.toStdString() + "?") ) {
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
        if( !game_g->askQuestionDialog("Save Game", "Are you sure you wish to overwrite an existing save game file?") ) {
            LOG("user says to not save\n");
            ok = false;
        }
    }
    if( ok ) {
        if( playing_gamestate->saveGame(filename.toStdString()) ) {
            game_g->showInfoDialog("Saved Game", "The game has been successfully saved.");
        }
        else {
            game_g->showErrorDialog("Failed to save game!");
        }
    }
    playing_gamestate->closeAllSubWindows();
}

PlayingGamestate::PlayingGamestate(bool is_savegame) :
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
        game_g->initButton(statsButton);
        statsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //statsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(statsButton, SIGNAL(clicked()), this, SLOT(clickedStats()));
        v_layout->addWidget(statsButton);

        QPushButton *itemsButton = new QPushButton("Items");
        game_g->initButton(itemsButton);
        itemsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //itemsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(itemsButton, SIGNAL(clicked()), this, SLOT(clickedItems()));
        v_layout->addWidget(itemsButton);

        QPushButton *spellsButton = new QPushButton("Spells");
        game_g->initButton(spellsButton);
        spellsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //spellsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        v_layout->addWidget(spellsButton);

        QPushButton *journalButton = new QPushButton("Journal");
        game_g->initButton(journalButton);
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
        pauseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        connect(pauseButton, SIGNAL(clicked()), game_g->getScreen(), SLOT(togglePaused()));
        v_layout->addWidget(pauseButton);

        QPushButton *restButton = new QPushButton("Rest");
        game_g->initButton(restButton);
        restButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        connect(restButton, SIGNAL(clicked()), this, SLOT(clickedRest()));
        v_layout->addWidget(restButton);

        QPushButton *optionsButton = new QPushButton("Options");
        game_g->initButton(optionsButton);
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
            connect(zoomoutButton, SIGNAL(clicked()), view, SLOT(zoomOut()));
            h_layout->addWidget(zoomoutButton);

            QPushButton *zoominButton = new QPushButton("+");
            game_g->initButton(zoominButton);
            connect(zoominButton, SIGNAL(clicked()), view, SLOT(zoomIn()));
            h_layout->addWidget(zoominButton);
        }
    }

    view->showFullScreen();

    gui_overlay = new GUIOverlay(this, view);
    gui_overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    view->setGUIOverlay(gui_overlay);

    gui_overlay->setProgress(0);
    qApp->processEvents();

    // create RPG data

    if( !is_savegame ) {
        LOG("create player\n");
        this->player = new Character("Warrior", "", false);
        this->player->setProfile(8, 7, 8, 1, 6, 7, 8, 2.75f);
        player->initialiseHealth(60);
        player->addGold( rollDice(2, 6, 10) );
    }

    LOG("load images\n");
    {
        QFile file(":/data/images.xml");
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
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
                    QStringRef name_s = reader.attributes().value("name");
                    if( name_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("image element has no name attribute or is zero length");
                    }
                    QStringRef imagetype_s = reader.attributes().value("imagetype");
                    LOG("image element type: %s name: %s imagetype: %s\n", type_s.toString().toStdString().c_str(), name_s.toString().toStdString().c_str(), imagetype_s.toString().toStdString().c_str());
                    QPixmap pixmap;
                    bool clip = false;
                    int xpos = 0, ypos = 0, width = 0, height = 0;
                    if( imagetype_s.length() == 0 ) {
                        // load file
                        QStringRef filename_s = reader.attributes().value("filename");
                        if( filename_s.length() == 0 ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("image element has no filename attribute or is zero length");
                        }
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
                        LOG("error at line %d\n", reader.lineNumber());
                        LOG("image element has unknown imagetype: %s\n", imagetype_s.string()->toStdString().c_str());
                        throw string("image element has unknown imagetype");
                    }

                    if( type_s == "generic") {
                        this->builtin_images[name_s.toString().toStdString()] = pixmap;
                    }
                    else if( type_s == "item") {
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
                                        LOG("error at line %d\n", reader.lineNumber());
                                        throw string("npc has unknown animation type");
                                    }
                                    animation_layer_definition.push_back( AnimationLayerDefinition(sub_name_s.toString().toStdString(), sub_start, sub_length, animation_type) );
                                }
                                else {
                                    LOG("error at line %d\n", reader.lineNumber());
                                    throw string("unknown xml tag within npc section");
                                }
                            }
                        }
                        //this->animation_layers[name.toStdString()] = AnimationLayer::create(filename.toStdString().c_str(), animation_layer_definition);
                        this->animation_layers[name.toStdString()] = AnimationLayer::create(pixmap, animation_layer_definition);
                    }
                    else {
                        LOG("error at line %d\n", reader.lineNumber());
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
        QFile file(":/data/items.xml");
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
                }
                else if( reader.name() == "player_default_item" ) {
                    if( itemsXMLType != ITEMS_XML_TYPE_PLAYER_DEFAULT ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected items xml: player_default_item element wasn't expected here");
                    }
                    if( !is_savegame ) {
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
        QFile file(":/data/npcs.xml");
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
                    LOG("found npc template: %s\n", name_s.toString().toStdString().c_str());
                    QStringRef animation_name_s = reader.attributes().value("animation_name");
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
                    CharacterTemplate *character_template = new CharacterTemplate(animation_name_s.toString().toStdString(), FP, BS, S, A, M, D, B, Sp, health_min, health_max, gold_min, gold_max);
                    QStringRef natural_damageX_s = reader.attributes().value("natural_damageX");
                    QStringRef natural_damageY_s = reader.attributes().value("natural_damageY");
                    QStringRef natural_damageZ_s = reader.attributes().value("natural_damageZ");
                    if( natural_damageX_s.length() > 0 || natural_damageY_s.length() > 0 || natural_damageZ_s.length() > 0 ) {
                        int natural_damageX = parseInt(natural_damageX_s.toString());
                        int natural_damageY = parseInt(natural_damageY_s.toString());
                        int natural_damageZ = parseInt(natural_damageZ_s.toString());
                        character_template->setNaturalDamage(natural_damageX, natural_damageY, natural_damageZ);
                    }
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

    //LOG("load floor image\n");
    //builtin_images["floor"] = game_g->loadImage(":/gfx/textures/floor_paved.png");
    //builtin_images["floor"] = game_g->loadImage(":/gfx/textures/floor_dirt.png");
    //builtin_images["floor"] = game_g->loadImage(":/gfx/textures/floor_rock.png");
    /*unsigned char filter_max[] = {80, 40, 20};
    unsigned char filter_min[] = {40, 20, 10};
    builtin_images["floor"] = createNoise(128, 128, 16.0f, 16.0f, filter_max, filter_min, NOISEMODE_PERLIN, 4);*/

    gui_overlay->setProgress(65);
    qApp->processEvents();

    //LOG("load wall image\n");
    //builtin_images["wall"] = game_g->loadImage(":/gfx/textures/wall.png");

    gui_overlay->setProgress(70);
    qApp->processEvents();

    LOG("load sound effects\n");
#ifndef Q_OS_ANDROID
    //if( !mobile_c )
    {
        /*
        //this->sound_effects["background"] = game_g->loadSound("music/discordance.ogg");
        this->sound_effects["background"] = game_g->loadSound(":/sound/door.wav"); // test
        //connect(this->sound_effects["background"], SIGNAL(finished()), this, SLOT(playBackgroundMusic())); // for looping
        connect(this->sound_effects["background"], SIGNAL(aboutToFinish()), this, SLOT(playBackgroundMusic())); // for looping
        this->sound_effects["background"]->play();
        */

        this->sound_effects["click"] = game_g->loadSound(":/sound/click_short.wav");
        this->sound_effects["coin"] = game_g->loadSound(":/sound/coin.wav");
        this->sound_effects["container"] = game_g->loadSound(":/sound/container.wav");
        this->sound_effects["door"] = game_g->loadSound(":/sound/door.wav");
        this->sound_effects["drink"] = game_g->loadSound(":/sound/bubble2.wav");
        this->sound_effects["lock"] = game_g->loadSound(":/sound/lock.wav");
        this->sound_effects["swing"] = game_g->loadSound(":/sound/swing2.wav");
        this->sound_effects["turn_page"] = game_g->loadSound(":/sound/turn_page.wav");
        this->sound_effects["weapon_unsheath"] = game_g->loadSound(":/sound/sword-unsheathe5.wav");
        this->sound_effects["wear_armour"] = game_g->loadSound(":/sound/chainmail1.wav");
    }
#endif

    gui_overlay->setProgress(90);
    qApp->processEvents();

    LOG("load quests\n");

    {
        QFile file(":/data/quests.xml");
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
                    LOG("found quest: %s\n", filename_s.toString().toStdString().c_str());
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
#ifndef Q_OS_ANDROID
    for(map<string, Phonon::MediaObject *>::iterator iter = this->sound_effects.begin(); iter != this->sound_effects.end(); ++iter) {
        Phonon::MediaObject *sound = iter->second;
        delete sound;
    }
#endif
    LOG("done\n");
}

void PlayingGamestate::playBackgroundMusic() {
    // needed for looping
    qDebug("PlayingGamestate::playBackgroundMusic()");
#ifndef Q_OS_ANDROID
    this->sound_effects["background"]->stop();
    this->sound_effects["background"]->play();
#endif
}

Item *PlayingGamestate::parseXMLItem(QXmlStreamReader &reader) {
    Item *item = NULL;
    if( reader.name() == "item" ) {
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
        item = new Item(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight);
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
        // must be done last
        QString description = reader.readElementText(QXmlStreamReader::IncludeChildElements);
        item->setDescription(description.toStdString());
    }
    else if( reader.name() == "weapon" ) {
        //qDebug("    weapon:");
        QStringRef name_s = reader.attributes().value("name");
        QStringRef image_name_s = reader.attributes().value("image_name");
        QStringRef weight_s = reader.attributes().value("weight");
        QStringRef animation_name_s = reader.attributes().value("animation_name");
        QStringRef two_handed_s = reader.attributes().value("two_handed");
        QStringRef ranged_s = reader.attributes().value("ranged");
        QStringRef ammo_s = reader.attributes().value("ammo");
        QStringRef damageX_s = reader.attributes().value("damageX");
        QStringRef damageY_s = reader.attributes().value("damageY");
        QStringRef damageZ_s = reader.attributes().value("damageZ");
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
        int damageX = parseInt(damageX_s.toString());
        int damageY = parseInt(damageY_s.toString());
        int damageZ = parseInt(damageZ_s.toString());
        Weapon *weapon = new Weapon(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, animation_name_s.toString().toStdString(), damageX, damageY, damageZ);
        item = weapon;
        weapon->setTwoHanded(two_handed);
        weapon->setRanged(ranged);
        if( ammo_s.length() > 0 ) {
            weapon->setRequiresAmmo(true, ammo_s.toString().toStdString());
        }
        // must be done last
        QString description = reader.readElementText(QXmlStreamReader::IncludeChildElements);
        weapon->setDescription(description.toStdString());
    }
    else if( reader.name() == "shield" ) {
        QStringRef name_s = reader.attributes().value("name");
        QStringRef image_name_s = reader.attributes().value("image_name");
        QStringRef weight_s = reader.attributes().value("weight");
        QStringRef animation_name_s = reader.attributes().value("animation_name");
        int weight = parseInt(weight_s.toString());
        Shield *shield = new Shield(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, animation_name_s.toString().toStdString());
        item = shield;
        // must be done last
        QString description = reader.readElementText(QXmlStreamReader::IncludeChildElements);
        shield->setDescription(description.toStdString());
    }
    else if( reader.name() == "armour" ) {
        //qDebug("    armour:");
        QStringRef name_s = reader.attributes().value("name");
        QStringRef image_name_s = reader.attributes().value("image_name");
        QStringRef weight_s = reader.attributes().value("weight");
        QStringRef rating_s = reader.attributes().value("rating");
        int weight = parseInt(weight_s.toString());
        int rating = parseInt(rating_s.toString());
        //qDebug("    name: %s", name_s.toString().toStdString().c_str());
        Armour *armour = new Armour(name_s.toString().toStdString(), image_name_s.toString().toStdString(), weight, rating);
        item = armour;
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
        Ammo *ammo = new Ammo(name_s.toString().toStdString(), image_name_s.toString().toStdString(), projectile_image_name_s.toString().toStdString(), amount);
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
    return item;
}

void PlayingGamestate::loadQuest(string filename, bool is_savegame) {
    LOG("PlayingGamestate::loadQuest(%s)\n", filename.c_str());
    // filename should be full path

    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->setEnabled(false);
    game_g->getScreen()->setPaused(true);

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
            QUEST_XML_TYPE_NPC = 3,
            QUEST_XML_TYPE_FLOORREGION = 4
        };
        QuestXMLType questXMLType = QUEST_XML_TYPE_NONE;
        Polygon2D boundary;
        Scenery *scenery = NULL;
        Character *npc = NULL;
        FloorRegion *floor_region = NULL;
        /*const QuestInfo &c_quest_info = this->quest_list.at(c_quest_indx);
        QString qt_filename = ":/" + QString(c_quest_info.getFilename().c_str());
        LOG("load quest: %s\n", qt_filename.toStdString().c_str());
        QFile file(qt_filename);*/
        QFile file(filename.c_str());
        if( !file.open(QFile::ReadOnly | QFile::Text) ) {
            throw string("Failed to open quest xml file");
        }
        QXmlStreamReader reader(&file);
        while( !reader.atEnd() && !reader.hasError() ) {
            reader.readNext();
            if( reader.isStartElement() )
            {
                qDebug("read start element: %s", reader.name().toString().toStdString().c_str());
                if( reader.name() == "info" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: info element wasn't expected here");
                    }
                    QString info = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    LOG("quest info: %s\n", info.toStdString().c_str());
                    quest->setInfo(info.toStdString());
                }
                else if( reader.name() == "journal" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: journal element wasn't expected here");
                    }
                    QString encoded = reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    QByteArray journal = QByteArray::fromPercentEncoding(encoded.toLatin1());
                    this->journal_ss.clear();
                    this->journal_ss << journal.data();
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
                    location->setFloorImageName(image_name_s.toString().toStdString());
                }
                else if( reader.name() == "wall" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: wall element wasn't expected here");
                    }
                    QStringRef image_name_s = reader.attributes().value("image_name");
                    if( image_name_s.length() == 0 ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: wall element has no image_name attribute");
                    }
                    location->setWallImageName(image_name_s.toString().toStdString());
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
                        //LOG("found floor region: %f, %f, %f, %f\n", rect_x, rect_y, rect_w, rect_h);
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
                    LOG("found boundary point: %f, %f\n", point_x, point_y);
                    boundary.addPoint(Vector2D(point_x, point_y));
                }*/
                else if( reader.name() == "player_start" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: player_start element wasn't expected here");
                    }
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    if( player == NULL ) {
                        throw string("encountered player_start element, but player not yet defined");
                    }
                    location->addCharacter(player, pos_x, pos_y);
                    done_player_start = true;
                }
                else if( reader.name() == "quest_objective" ) {
                    QStringRef type_s = reader.attributes().value("type");
                    QStringRef arg1_s = reader.attributes().value("arg1");
                    QuestObjective *quest_objective = new QuestObjective(type_s.toString().toStdString(), arg1_s.toString().toStdString());
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
                    if( template_s.length() > 0 ) {
                        if( reader.name() == "player" ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("didn't expect player element to load by template");
                        }
                        // load from template
                        CharacterTemplate *character_template = this->character_templates[name_s.toString().toStdString()];
                        if( character_template == NULL ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            LOG("can't find character template: %s\n", template_s.toString().toStdString().c_str());
                            throw string("can't find character template");
                        }
                        npc = new Character(name_s.toString().toStdString(), true, *character_template);
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
                            QStringRef is_hostile_s = reader.attributes().value("is_hostile");
                            bool is_hostile = is_hostile_s.length() == 0 ? true : parseBool(is_hostile_s.toString());
                            npc = new Character(name_s.toString().toStdString(), animation_name_s.toString().toStdString(), is_hostile);
                        }
                        QStringRef is_dead_s = reader.attributes().value("is_dead");
                        npc->setDead( parseBool(is_dead_s.toString()) );
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
                            int natural_damageY = parseInt(natural_damageX_s.toString());
                            int natural_damageZ = parseInt(natural_damageX_s.toString());
                            npc->setNaturalDamage(natural_damageX, natural_damageY, natural_damageZ);
                        }
                        QStringRef gold_s = reader.attributes().value("gold");
                        npc->addGold( parseInt( gold_s.toString()) );
                    }
                    location->addCharacter(npc, pos_x, pos_y);
                    questXMLType = QUEST_XML_TYPE_NPC;
                }
                else if( reader.name() == "item" || reader.name() == "weapon" || reader.name() == "shield" || reader.name() == "armour" || reader.name() == "ammo" || reader.name() == "currency" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE && questXMLType != QUEST_XML_TYPE_SCENERY && questXMLType != QUEST_XML_TYPE_NPC ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: item element wasn't expected here");
                    }
                    QStringRef template_s = reader.attributes().value("template");
                    Item *item = NULL;
                    if( template_s.length() > 0 ) {
                        // load from template
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
                        item = parseXMLItem(reader);
                        if( item == NULL ) {
                            LOG("error at line %d\n", reader.lineNumber());
                            throw string("unknown item element");
                        }
                    }
                    if( questXMLType == QUEST_XML_TYPE_SCENERY ) {
                        scenery->addItem(item);
                    }
                    else if( questXMLType == QUEST_XML_TYPE_NPC ) {
                        QStringRef current_weapon_s = reader.attributes().value("current_weapon");
                        bool current_weapon = parseBool(current_weapon_s.toString(), true);
                        QStringRef current_shield_s = reader.attributes().value("current_shield");
                        bool current_shield = parseBool(current_shield_s.toString(), true);
                        QStringRef current_armour_s = reader.attributes().value("current_armour");
                        bool current_armour = parseBool(current_armour_s.toString(), true);
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
                        QStringRef pos_x_s = reader.attributes().value("x");
                        float pos_x = parseFloat(pos_x_s.toString());
                        QStringRef pos_y_s = reader.attributes().value("y");
                        float pos_y = parseFloat(pos_y_s.toString());
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
                    QStringRef blocking_s = reader.attributes().value("blocking");
                    bool blocking = parseBool(blocking_s.toString(), true);
                    QStringRef block_visibility_s = reader.attributes().value("block_visibility");
                    bool block_visibility = parseBool(block_visibility_s.toString(), true);
                    QStringRef door_s = reader.attributes().value("door");
                    bool door = parseBool(door_s.toString(), true);
                    QStringRef exit_s = reader.attributes().value("exit");
                    bool exit = parseBool(exit_s.toString(), true);
                    QStringRef locked_s = reader.attributes().value("locked");
                    bool locked = parseBool(locked_s.toString(), true);
                    QStringRef unlock_item_name_s = reader.attributes().value("unlocked_by_template");
                    float size_w = 0.0f, size_h = 0.0f;
                    QStringRef size_s = reader.attributes().value("size");
                    if( size_s.length() > 0 ) {
                        float size = parseFloat(size_s.toString());
                        QPixmap image = this->scenery_images[image_name_s.toString().toStdString()];
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

                    scenery = new Scenery(name_s.toString().toStdString(), image_name_s.toString().toStdString(), size_w, size_h);
                    location->addScenery(scenery, pos_x, pos_y);
                    scenery->setBlocking(blocking, block_visibility);
                    if( door && exit ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("scenery can't be both a door and an exit");
                    }
                    scenery->setDoor(door);
                    scenery->setExit(exit);
                    scenery->setLocked(locked);
                    if( unlock_item_name_s.length() > 0 ) {
                        scenery->setUnlockItemName(unlock_item_name_s.toString().toStdString());
                    }
                    map<string, QPixmap>::iterator image_iter = this->scenery_opened_images.find(scenery->getImageName());
                    if( image_iter != this->scenery_opened_images.end() ) {
                        scenery->setCanBeOpened(true);
                    }
                    questXMLType = QUEST_XML_TYPE_SCENERY;
                }
                else if( reader.name() == "trap" ) {
                    if( questXMLType != QUEST_XML_TYPE_NONE ) {
                        LOG("error at line %d\n", reader.lineNumber());
                        throw string("unexpected quest xml: trap element wasn't expected here");
                    }
                    QStringRef type_s = reader.attributes().value("type");
                    QStringRef pos_x_s = reader.attributes().value("x");
                    float pos_x = parseFloat(pos_x_s.toString());
                    QStringRef pos_y_s = reader.attributes().value("y");
                    float pos_y = parseFloat(pos_y_s.toString());
                    QStringRef size_w_s = reader.attributes().value("w");
                    float size_w = parseFloat(size_w_s.toString());
                    QStringRef size_h_s = reader.attributes().value("h");
                    float size_h = parseFloat(size_h_s.toString());
                    Trap *trap = new Trap(type_s.toString().toStdString(), size_w, size_h);
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
                else */if( reader.name() == "npc" || reader.name() == "player" ) {
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
                    location->addFloorRegion(floor_region);
                    floor_region = NULL;
                    questXMLType = QUEST_XML_TYPE_NONE;
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
    //QBrush floor_brush(builtin_images["floor"]);
    QBrush floor_brush(builtin_images[location->getFloorImageName()]);
    floor_brush.setTransform(QTransform::fromScale(scale, scale));
    //QBrush wall_brush(builtin_images["wall"]);
    QBrush wall_brush(builtin_images[location->getWallImageName()]);
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

    LOG("add graphics for items\n");
    for(set<Item *>::iterator iter = location->itemsBegin(); iter != location->itemsEnd(); ++iter) {
        Item *item = *iter;
        this->locationAddItem(location, item);
    }
    LOG("add graphics for scenery");
    for(set<Scenery *>::iterator iter = location->scenerysBegin(); iter != location->scenerysEnd(); ++iter) {
        Scenery *scenery = *iter;
        this->locationAddScenery(location, scenery);
    }

    gui_overlay->setProgress(95);
    qApp->processEvents();

    LOG("add graphics for characters");
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

    LOG("init visibility\n");
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

    if( !is_savegame && quest->getInfo().length() > 0 ) {
        stringstream str;
        str << "<html><body>";
        str << "<h1>Quest</h1>";
        str << "<p>" << quest->getInfo() << "</p>";
        str << "</body></html>";
        this->showInfoWindow(str.str());

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
            map<string, QPixmap>::iterator image_iter = this->scenery_opened_images.find(scenery->getImageName());
            if( image_iter != this->scenery_opened_images.end() ) {
                done = true;
                object->setPixmap( image_iter->second );
            }
        }
        if( !done ) {
            map<string, QPixmap>::iterator image_iter = this->scenery_images.find(scenery->getImageName());
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
    quitButton->setFont(game_g->getFontBig());
    quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(quitButton);
    connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));

    QPushButton *saveButton = new QPushButton("Save game");
    game_g->initButton(saveButton);
    saveButton->setFont(game_g->getFontBig());
    saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(saveButton);
    connect(saveButton, SIGNAL(clicked()), this, SLOT(clickedSave()));

    QPushButton *closeButton = new QPushButton("Back to game");
    game_g->initButton(closeButton);
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

void PlayingGamestate::clickedSave() {
    LOG("PlayingGamestate::clickedSave()\n");
    if( c_location->hasEnemies(this) ) {
        game_g->showInfoDialog("Save", "You cannot save here - enemies are nearby.");
        return;
    }
    new SaveGameWindow(this);
}

void PlayingGamestate::clickedQuit() {
    LOG("clickedQuit()\n");
    this->quitGame();
}

void PlayingGamestate::showInfoWindow(const string &html) {
    LOG("showInfoWindow()\n");

    game_g->getScreen()->setPaused(true);

    QWidget *subwindow = new QWidget();
    this->addWidget(subwindow);

    QVBoxLayout *layout = new QVBoxLayout();
    subwindow->setLayout(layout);

    QWebView *label = new QWebView();
    label->setHtml(html.c_str());
    layout->addWidget(label);

    QPushButton *closeButton = new QPushButton("Continue");
    game_g->initButton(closeButton);
    closeButton->setFont(game_g->getFontBig());
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeSubWindow()));
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

    // scroll
    if( !mobile_c ) {
        int time_ms = game_g->getScreen()->getGameTimeFrameMS();
        float speed = (4.0f * time_ms)/1000.0f;
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
        }
        else if( m_x >= screen_width-1 ) {
            centre.setX( centre.x() + speed );
            this->view->centerOn(centre);
        }
        if( m_y <= 0 ) {
            centre.setY( centre.y() - speed );
            this->view->centerOn(centre);
        }
        else if( m_y >= screen_height-1 ) {
            centre.setY( centre.y() + speed );
            this->view->centerOn(centre);
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
    if( object == NULL ) {
        throw string("character has NULL graphics object");
    }
    object->clearAnimationLayers();
    if( character == player ) {
        object->addAnimationLayer( this->animation_layers["clothes"] );
        object->addAnimationLayer( this->animation_layers["head"] );
        if( character->getCurrentWeapon() != NULL ) {
            object->addAnimationLayer( this->animation_layers[ character->getCurrentWeapon()->getAnimationName() ] );
        }
        if( character->getCurrentShield() != NULL ) {
            object->addAnimationLayer( this->animation_layers[ character->getCurrentShield()->getAnimationName() ] );
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
                int rollD = rollDice(2, 6, 0);
                LOG("character: %s has set of trap at %f, %f roll %d\n", character->getName().c_str(), trap->getX(), trap->getY(), rollD);
                this->playSound("click");
                delete_traps.push_back(trap);
                if( rollD <= character->getDexterity() ) {
                    this->addTextEffect("You have set off a trap!\nAn arrow shoots out from the wall,\nbut you manage to avoid it!", player->getPos(), 2000);
                }
                else {
                    this->addTextEffect("You have set off a trap!\nAn arrow shoots out from the wall and hits you!", player->getPos(), 2000);
                    int damage = rollDice(2, 12, -1);
                    character->decreaseHealth(this, damage, true, false);
                    if( character->isDead() ) {
                        break;
                    }
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

void PlayingGamestate::characterSetAnimation(const Character *character, void *user_data, const string &name) {
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
                if( picked_item->getType() == ITEMTYPE_CURRENCY ) {
                    this->playSound("coin");
                }
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
                //LOG("dist_from_click for scenery %s : %f", scenery->getName().c_str(), dist_from_click);
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
                    //LOG("    player_dist : %f", player_dist);
                    if( player_dist <= npc_radius_c + 0.5f ) {
                        if( selected_scenery == NULL || dist_from_click < min_dist ) {
                            //LOG("    selected!\n");
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

                if( selected_scenery->canBeOpened() && !selected_scenery->isOpened() ) {
                    this->playSound("container");
                    selected_scenery->setOpened(true);
                }

                if( selected_scenery->isDoor() ) {
                    qDebug("clicked on a door");
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
                            this->playSound("lock");
                            this->addTextEffect("The door is locked!", player->getPos(), 2000);
                        }
                        else {
                            this->addTextEffect("You unlock the door.", player->getPos(), 2000);
                            selected_scenery->setLocked(false); // we'll delete the door anyway below, but just to be safe...
                        }
                    }
                    if( !is_locked ) {
                        // open door
                        this->playSound("door");
                        c_location->removeScenery(selected_scenery);
                        delete selected_scenery;
                        selected_scenery = NULL;
                    }
                }
                else if( selected_scenery->isExit() ) {
                    LOG("clicked on an exit");
                    // exit
                    this->closeSubWindow(); // just in case
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
    fprintf(file, " rating=\"%d\"", item->getRating());
    fprintf(file, " magical=\"%s\"", item->isMagical() ? "true": "false");
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
    fprintf(file, "/>\n");
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

    const int savegame_version = 1;

    fprintf(file, "<?xml version=\"1.0\" ?>\n");
    fprintf(file, "<savegame major=\"%d\" minor=\"%d\" savegame_version=\"%d\">\n", versionMajor, versionMinor, savegame_version);
    fprintf(file, "\n");
    fprintf(file, "<current_quest name=\"%s\"/>\n", this->quest_list.at(this->c_quest_indx).getFilename().c_str());
    fprintf(file, "\n");

    LOG("save locations\n");
    for(size_t i=0;i<c_location->getNFloorRegions();i++) {
        const FloorRegion *floor_region = c_location->getFloorRegion(i);
        fprintf(file, "<floorregion shape=\"polygon\" visible=\"%s\">\n", floor_region->isVisible() ? "true" : "false");
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D point = floor_region->getPoint(j);
            fprintf(file, "    <floorregion_point x=\"%f\" y=\"%f\"/>\n", point.x, point.y);
        }
        fprintf(file, "</floorregion>\n");
    }
    fprintf(file, "\n");

    LOG("save player and npcs\n");
    for(set<Character *>::const_iterator iter = c_location->charactersBegin(); iter != c_location->charactersEnd(); ++iter) {
        const Character *character = *iter;
        if( player == character )
            fprintf(file, "<player");
        else {
            fprintf(file, "<npc");
            fprintf(file, " is_hostile=\"%s\"", character->isHostile() ? "true": "false");
            fprintf(file, " animation_name=\"%s\"", character->getAnimationName().c_str());
        }
        fprintf(file, " name=\"%s\"", character->getName().c_str());
        fprintf(file, " is_dead=\"%s\"", character->isDead() ? "true": "false");
        fprintf(file, " x=\"%f\" y=\"%f\"", character->getX(), character->getY());
        fprintf(file, " health=\"%d\"", character->getHealth());
        fprintf(file, " max_health=\"%d\"", character->getMaxHealth());
        fprintf(file, " FP=\"%d\"", character->getFP());
        fprintf(file, " BS=\"%d\"", character->getBS());
        fprintf(file, " S=\"%d\"", character->getStrength());
        fprintf(file, " A=\"%d\"", character->getAttacks());
        fprintf(file, " M=\"%d\"", character->getMind());
        fprintf(file, " D=\"%d\"", character->getDexterity());
        fprintf(file, " B=\"%d\"", character->getBravery());
        fprintf(file, " Sp=\"%f\"", character->getSpeed());
        int natural_damageX = 0, natural_damageY = 0, natural_damageZ = 0;
        character->getNaturalDamage(&natural_damageX, &natural_damageY, &natural_damageZ);
        fprintf(file, " natural_damageX=\"%d\"", natural_damageX);
        fprintf(file, " natural_damageY=\"%d\"", natural_damageY);
        fprintf(file, " natural_damageZ=\"%d\"", natural_damageZ);
        fprintf(file, " gold=\"%d\"", character->getGold());
        fprintf(file, ">\n");
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

    LOG("save player additional info\n");
    fprintf(file, "<player_start x=\"%f\" y=\"%f\"/>\n", player->getX(), player->getY());
    fprintf(file, "\n");

    LOG("save scenery\n");
    for(set<Scenery *>::const_iterator iter = c_location->scenerysBegin(); iter != c_location->scenerysEnd(); ++iter) {
        const Scenery *scenery = *iter;
        fprintf(file, "<scenery");
        fprintf(file, " name=\"%s\"", scenery->getName().c_str());
        fprintf(file, " image_name=\"%s\"", scenery->getImageName().c_str());
        fprintf(file, " x=\"%f\" y=\"%f\"", scenery->getX(), scenery->getY());
        fprintf(file, " w=\"%f\" h=\"%f\"", scenery->getWidth(), scenery->getHeight());
        fprintf(file, " blocking=\"%s\"", scenery->isBlocking() ? "true": "false");
        fprintf(file, " block_visibility=\"%s\"", scenery->blocksVisibility() ? "true": "false");
        fprintf(file, " exit=\"%s\"", scenery->isExit() ? "true": "false");
        fprintf(file, " door=\"%s\"", scenery->isDoor() ? "true": "false");
        fprintf(file, " locked=\"%s\"", scenery->isLocked() ? "true": "false");
        fprintf(file, " unlocked_by_template=\"%s\"", scenery->getUnlockItemName().c_str());
        fprintf(file, ">");
        for(set<Item *>::const_iterator iter2 = scenery->itemsBegin(); iter2 != scenery->itemsEnd(); ++iter2) {
            const Item *item = *iter2;
            this->saveItem(file, item);
        }
        fprintf(file, "</scenery>");
    }
    fprintf(file, "\n");

    LOG("save items\n");
    for(set<Item *>::const_iterator iter = c_location->itemsBegin(); iter != c_location->itemsEnd(); ++iter) {
        const Item *item = *iter;
        this->saveItem(file, item);
    }
    fprintf(file, "\n");

    LOG("save traps\n");
    for(set<Trap *>::const_iterator iter = c_location->trapsBegin(); iter != c_location->trapsEnd(); ++iter) {
        const Trap *trap = *iter;
        fprintf(file, "<trap");
        fprintf(file, " type=\"%s\"", trap->getType().c_str());
        fprintf(file, " x=\"%f\" y=\"%f\"", trap->getX(), trap->getY());
        fprintf(file, " w=\"%f\" h=\"%f\"", trap->getWidth(), trap->getHeight());
        fprintf(file, " />");
    }
    fprintf(file, "\n");

    const QuestObjective *quest_objective = this->getQuest()->getQuestObjective();
    if( quest_objective != NULL ) {
        LOG("save quest objective\n");
        fprintf(file, "<quest_objective");
        fprintf(file, " type=\"%s\"", quest_objective->getType().c_str());
        fprintf(file, " arg1=\"%s\"", quest_objective->getArg1().c_str());
        fprintf(file, " />");
    }
    fprintf(file, "\n");

    LOG("save current quest info\n");
    fprintf(file, "<quest_info complete=\"%s\"/>\n", this->getQuest()->isCompleted() ? "true" : "false");
    fprintf(file, "\n");

    LOG("save journal\n");
    fprintf(file, "<journal>\n");
    QByteArray encoded = QUrl::toPercentEncoding(this->journal_ss.str().c_str(), QByteArray(), "<>");
    fprintf(file, "%s", encoded.data());
    fprintf(file, "</journal>\n");
    fprintf(file, "\n");

    fprintf(file, "</savegame>\n");

    fclose(file);

    return true;
}

void PlayingGamestate::addWidget(QWidget *widget) {
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);
}

void PlayingGamestate::addTextEffect(const string &text, Vector2D pos, int duration_ms) {
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

void PlayingGamestate::playSound(const string &sound_effect) {
#ifndef Q_OS_ANDROID
    qDebug("play sound: %s\n", sound_effect.c_str());
    Phonon::MediaObject *sound = this->sound_effects[sound_effect];
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
#endif
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

Game::Game() : gamestate(NULL), screen(NULL) {
    game_g = this;

    QCoreApplication::setApplicationName("erebus");

    // initialise paths
    // n.b., not safe to use logging until after copied/removed old log files!
    QString pathQt = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    qDebug("user data location path: %s", pathQt.toStdString().c_str());
    if( !QDir(pathQt).exists() ) {
        QDir().mkpath(pathQt);
    }
    //QString pathQt = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + QString("/erebus");
    QString nativePath(QDir::toNativeSeparators(pathQt));
    application_path = nativePath.toStdString();
    logfilename = getApplicationFilename("log.txt");
    oldlogfilename = getApplicationFilename("log_old.txt");
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

    QString savegame_path = QString(getApplicationFilename(savegame_folder).c_str());
    if( !QDir(savegame_path).exists() ) {
        LOG("create savegame_path: %s\n", savegame_path.toStdString().c_str());
        QDir().mkpath(savegame_path);
        if( !QDir(savegame_path).exists() ) {
            LOG("failed to create savegame_path!\n");
        }
    }
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
        this->font_small.setPointSize(font_small.pointSize() - 2);
        this->font_std = new_font;
        this->font_big = new_font;
#endif
    }
    else {
        this->font_small = QFont("Verdana", 12);
        this->font_std = QFont("Verdana", 16);
        this->font_big = QFont("Verdana", 48, QFont::Bold);
    }

#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR)
    // crashes on Android?!
    // and doesn't work on Symbian
    // see Game::initButton()
#else
    unsigned char filter_max[] = {220, 220, 220};
    unsigned char filter_min[] = {120, 120, 120};
    QPixmap gui_pixmap_buttons = createNoise(256, 256, 16.0f, 16.0f, filter_max, filter_min, NOISEMODE_PERLIN, 4);
    this->gui_brush_buttons.setTexture(gui_pixmap_buttons);
    this->gui_palette.setBrush(QPalette::Button, gui_brush_buttons);
#endif

    gamestate = new OptionsGamestate();

    screen->runMainLoop();

    delete gamestate;
    delete screen;
}

void Game::initButton(QPushButton *button) const {
#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR)
    // crashes on Android?!
    // and doesn't work on Symbian
    // if we change this, remember to change it where we initialise gui_palette, in Game::run().
#else
    button->setAutoFillBackground(true);
    button->setPalette(this->gui_palette);
#endif
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
                PlayingGamestate *playing_gamestate = new PlayingGamestate(false);
                gamestate = playing_gamestate;
                const QuestInfo &c_quest_info = playing_gamestate->getCQuestInfo();
                string qt_filename = ":/" + c_quest_info.getFilename();
                playing_gamestate->loadQuest(qt_filename, false);
                this->getScreen()->getMainWindow()->unsetCursor();
                break;
            }
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING_LOAD:
            {
                delete gamestate;
                gamestate = NULL;
                PlayingGamestate *playing_gamestate = new PlayingGamestate(true);
                gamestate = playing_gamestate;
                try {
                    LoadGameMessage *load_message = static_cast<LoadGameMessage *>(message);
                    string full_filename = this->getApplicationFilename(savegame_folder + load_message->getFilename());
                    playing_gamestate->loadQuest(full_filename, true);
                    this->getScreen()->getMainWindow()->unsetCursor();
                }
                catch(string &error) {
                    LOG("exception creating new gamestate when loading: %s\n", error.c_str());
                    this->getScreen()->getMainWindow()->unsetCursor();
                    stringstream str;
                    str << "Failed to load save game file:\n" << error;
                    game_g->showErrorDialog(str.str());
                    delete gamestate;
                    gamestate = NULL;
                    gamestate = new OptionsGamestate();
                }
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
            if( gamestate != NULL ) {
                delete gamestate;
                gamestate = NULL;
            }
            stringstream str;
            str << "Failed to load game data:\n" << error;
            game_g->showErrorDialog(str.str());
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

string Game::getApplicationFilename(const string &name) {
    // not safe to use LOG here, as logfile may not have been initialised!
    QString pathQt = QString(application_path.c_str()) + QString("/") + QString(name.c_str());
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

QPixmap Game::loadImage(const string &filename, bool clip, int xpos, int ypos, int width, int height) const {
    // need to use QImageReader - QPixmap::load doesn't work on large images on Symbian!
    QImageReader reader(filename.c_str());
    if( clip ) {
        reader.setClipRect(QRect(xpos, ypos, width, height));
    }
    QImage image = reader.read();
    if( image.isNull() ) {
        LOG("failed to read image: %s\n", filename.c_str());
        LOG("error: %d\n", reader.error());
        LOG("error string: %s\n", reader.errorString().toStdString().c_str());
        stringstream error;
        error << "Failed to load image: " << filename;
        throw error.str();
    }
    QPixmap pixmap = QPixmap::fromImage(image);
    if( pixmap.isNull() ) {
        LOG("failed to convert image to pixmap: %s\n", filename.c_str());
        throw string("Failed to convert image to pixmap");
    }
    return pixmap;
}

#ifndef Q_OS_ANDROID
Phonon::MediaObject *Game::loadSound(string filename) const {
    Phonon::MediaObject *sound = new Phonon::MediaObject(qApp);
    //connect(sound, SIGNAL(stateChanged(Phonon::State,Phonon::State)), this, SLOT(mediaStateChanged(Phonon::State,Phonon::State)));
    sound->setCurrentSource(Phonon::MediaSource(filename.c_str()));
    Phonon::AudioOutput *audioOutput = new Phonon::AudioOutput(Phonon::GameCategory, qApp);
    Phonon::Path audioPath = Phonon::createPath(sound, audioOutput);
    return sound;
}

/*void Game::mediaStateChanged(Phonon::State newstate, Phonon::State oldstate) const {
    qDebug("Game::mediaStateChanged(%d, %d)", newstate, oldstate);
    if( newstate == Phonon::ErrorState ) {
        Phonon::MediaObject *sound = static_cast<Phonon::MediaObject *>(this->sender());
        qDebug("phonon reports error!");
        if( sound != NULL ) {
            qDebug("    error code: %d", sound->errorType());
            qDebug("    error string: %s", sound->errorString().toStdString().c_str());
        }
    }
}*/
#endif

void Game::showErrorDialog(const string &message) {
    LOG("Game::showErrorDialog: %s\n", message.c_str());
    this->getScreen()->enableUpdateTimer(false);
    QMessageBox::critical(this->getScreen()->getMainWindow(), "Error", message.c_str());
    this->getScreen()->enableUpdateTimer(true);
}

void Game::showInfoDialog(const string &title, const string &message) {
    LOG("Game::showInfoDialog: %s\n", message.c_str());
    this->getScreen()->enableUpdateTimer(false);
    QMessageBox::information(this->getScreen()->getMainWindow(), title.c_str(), message.c_str());
    this->getScreen()->enableUpdateTimer(true);
}

bool Game::askQuestionDialog(const string &title, const string &message) {
    LOG("Game::askQuestionWindow: %s\n", message.c_str());
    this->getScreen()->enableUpdateTimer(false);
    int res = QMessageBox::question(this->getScreen()->getMainWindow(), title.c_str(), message.c_str(), QMessageBox::Yes, QMessageBox::No);
    this->getScreen()->enableUpdateTimer(true);
    LOG("    answer is %d\n", res);
    return res == QMessageBox::Yes;
}
