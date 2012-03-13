#include "rpg/location.h"
#include "rpg/item.h"

#include "game.h"
#include "qt_screen.h"

#include <ctime>

#include <sstream>
using std::stringstream;

Game *game_g = NULL;
OptionsGamestate *OptionsGamestate::optionsGamestate = NULL;
PlayingGamestate *PlayingGamestate::playingGamestate = NULL;

const int versionMajor = 0;
const int versionMinor = 1;

//const int scene_item_character_key_c = 0;

AnimationSet::AnimationSet(AnimationType animation_type, int n_frames, vector<QPixmap> pixmaps) : animation_type(animation_type), n_frames(n_frames), pixmaps(pixmaps) {
    if( pixmaps.size() != N_DIRECTIONS * n_frames ) {
        LOG("AnimationSet error: pixmaps size %d, n_frames %d, N_DIRECTIONS %d\n", pixmaps.size(), n_frames, N_DIRECTIONS);
        throw string("AnimationSet has incorrect pixmaps size");
    }
    /*this->pixmaps = new QPixmap[N_DIRECTIONS * n_frames];
    for(int i=0;i<N_DIRECTIONS * n_frames;i++) {
        this->pixmaps[i] = pixmaps[i];
    }*/

    /*this->bounding_rect.x = 0.0f;
    this->bounding_rect.y = 0.0f;
    this->bounding_rect.x = 0.0f;
    this->bounding_rect.x = 0.0f;*/
    //this->setFrame(0);
}

AnimationSet::~AnimationSet() {
    //delete this->pixmaps;
}

/*QPixmap *AnimationSet::getFrames(Direction c_direction) {
    return &this->pixmaps[((int)c_direction)*n_frames];
}*/

const QPixmap &AnimationSet::getFrame(Direction c_direction, int c_frame) const {
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

AnimationSet *AnimationSet::create(QPixmap image, AnimationType animation_type, int x_offset, int n_frames) {
    vector<QPixmap> frames;
    for(int i=0;i<N_DIRECTIONS;i++) {
        for(int j=0;j<n_frames;j++) {
            //frames.push_back( game_g->loadImage(filename, true, 64*(x_offset+j), 64*i, 64, 64) );
            frames.push_back( image.copy(64*(x_offset+j), 64*i, 64, 64));
        }
    }
    AnimationSet *animation_set = new AnimationSet(animation_type, n_frames, frames);
    return animation_set;
}

/*AnimationLayer *AnimationLayer::create(const char *filename) {
    AnimationLayer *layer = new AnimationLayer();
    QPixmap image = game_g->loadImage(filename);
    {
        AnimationSet *animation_set_idle = AnimationSet::create(image, 0, 4);
        layer->addAnimationSet("", animation_set_idle);
        AnimationSet *animation_set_run = AnimationSet::create(image, 4, 8);
        layer->addAnimationSet("run", animation_set_run);
        AnimationSet *animation_set_attack = AnimationSet::create(image, 12, 4);
        layer->addAnimationSet("attack", animation_set_attack);
        AnimationSet *animation_set_death = AnimationSet::create(image, 18, 6);
        layer->addAnimationSet("death", animation_set_death);
    }
    return layer;
}*/

AnimationLayer *AnimationLayer::create(const char *filename, const vector<AnimationLayerDefinition> &animation_layer_definitions) {
    AnimationLayer *layer = new AnimationLayer();
    QPixmap image = game_g->loadImage(filename);
    for(vector<AnimationLayerDefinition>::const_iterator iter = animation_layer_definitions.begin(); iter != animation_layer_definitions.end(); ++iter) {
        const AnimationLayerDefinition animation_layer_definition = *iter;
        AnimationSet *animation_set = AnimationSet::create(image, animation_layer_definition.animation_type, animation_layer_definition.position, animation_layer_definition.n_frames);
        layer->addAnimationSet(animation_layer_definition.name, animation_set);
    }
    return layer;
}

AnimatedObject::AnimatedObject() : /*animation_layer(NULL), c_animation_set(NULL),*/ c_direction(DIRECTION_E), c_frame(0), animation_time_start_ms(0) {
    //this->setFrame(0);
    for(vector<const AnimationSet *>::const_iterator iter = c_animation_sets.begin(); iter != c_animation_sets.end(); ++iter) {
        const AnimationSet *c_animation_set = *iter;
        delete c_animation_set;
    }
}

AnimatedObject::~AnimatedObject() {
    //this->setPixmap(NULL); // just in case of ownership issues??
}

#if 0
void AnimatedObject::setFrame(int c_frame) {
    //this->c_frame = c_frame;
    //qDebug("set frame %d", c_frame);
    //this->setPixmap(this->pixmaps[((int)c_direction)*n_frames + c_frame]);
    /*if( this->c_animation_set != NULL ) {
        const QPixmap &pixmap = c_animation_set->getFrame(c_direction, c_frame);
        this->setPixmap(pixmap);
    }
    else {
        this->setPixmap(NULL);
    }*/
    /*if( this->c_animation_sets.size() > 0 ) {
        for(vector<const AnimationSet *>::const_iterator iter = c_animation_sets.begin(); iter != c_animation_sets.end(); ++iter) {
            const AnimationSet *c_animation_set = *iter;
            const QPixmap &pixmap = c_animation_set->getFrame(c_direction, c_frame);
            this->setPixmap(pixmap);
            break;
        }
    }
    else {
        this->setPixmap(NULL);
    }*/
}
#endif

/*void AnimationSet::update() {
    int next_frame = (c_frame + 1) % n_frames;
    this->setFrame(next_frame);
}*/

void AnimatedObject::advance(int phase) {
    //qDebug("AnimatedObject::advance() phase %d", phase);
    if( phase == 1 ) {
        int ms_per_frame = 100;
        int time_elapsed_ms = game_g->getScreen()->getElapsedMS() - animation_time_start_ms;
        /*int c_frame = ( time_elapsed_ms / ms_per_frame );
        this->setFrame(c_frame);*/
        int n_frame = ( time_elapsed_ms / ms_per_frame );
        if( n_frame != c_frame ) {
            c_frame = n_frame;
            this->update();
        }
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

/*void AnimatedObject::setAnimationLayer(AnimationLayer *animation_layer) {
    this->animation_layer = animation_layer;
    this->c_animation_set = animation_layer->getAnimationSet("");
    this->setFrame(0);
}*/

void AnimatedObject::addAnimationLayer(AnimationLayer *animation_layer) {
    this->animation_layers.push_back(animation_layer);
    const AnimationSet *c_animation_set = animation_layer->getAnimationSet("");
    this->c_animation_sets.push_back(c_animation_set);
    //this->setFrame(0);
    this->update();
}

void AnimatedObject::setAnimationSet(string name) {
    /*const AnimationSet *new_animation_set = animation_layer->getAnimationSet(name);
    if( new_animation_set != c_animation_set ) {
        c_animation_set = new_animation_set;
        animation_time_start_ms = 0;
        this->setFrame(0);
    }*/
    //qDebug("set animation set: %s", name.c_str());
    this->c_animation_sets.clear();
    for(vector<AnimationLayer *>::const_iterator iter = animation_layers.begin(); iter != animation_layers.end(); ++iter) {
        const AnimationLayer *animation_layer = *iter;
        const AnimationSet *c_animation_set = animation_layer->getAnimationSet(name);
        if( c_animation_set == NULL ) {
            qDebug("unknown animation set: %s", name.c_str());
            throw string("Unknown animation set");
        }
        this->c_animation_sets.push_back(c_animation_set);
    }
    animation_time_start_ms = 0;
    //this->setFrame(0);
    this->c_frame = 0;
    this->update();
}

void AnimatedObject::setDirection(Direction c_direction) {
    if( this->c_direction != c_direction ) {
        this->c_direction = c_direction;
        //this->setFrame(0);
        this->update();
    }
}

OptionsGamestate::OptionsGamestate()
{
    optionsGamestate = this;

    MainWindow *window = game_g->getScreen()->getMainWindow();
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
#else
    QFont font("Verdana", 48, QFont::Bold);
    window->setFont(font);
#endif

    QWidget *centralWidget = new QWidget(window);
    centralWidget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    window->setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout();
    centralWidget->setLayout(layout);

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

    QPushButton *quitButton = new QPushButton("Quit game");
    quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(quitButton);
    connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));
    //this->initButton(prevButton);

}

OptionsGamestate::~OptionsGamestate() {
    /*VI_flush(0); // delete all the gamestate objects, but leave the game level objects (which should be set at persistence level -1)
    VI_GraphicsEnvironment *genv = game_g->getGraphicsEnvironment();
    game_g->getGraphicsEnvironment()->setPanel(NULL); // as the main panel is now destroyed
    */
    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->centralWidget()->deleteLater();
    window->setCentralWidget(NULL);

    optionsGamestate = NULL;
    qDebug("deleted OptionsGamestate");
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
        const int drag_tol_c = 16;
        // on a touchscreen phone, it's very hard to press and release without causing a drag, so need to allow some tolerance!
        //if( m_x == this->mouse_down_x && m_y == this->mouse_down_y ) {
        if( xdist <= drag_tol_c && ydist <= drag_tol_c ) {
            QPointF m_scene = this->mapToScene(m_x, m_y);
            qDebug("clicked: %f, %f", m_scene.x(), m_scene.y());
            playing_gamestate->clickedMainView(m_scene.x(), m_scene.y());
        }
        // else, this was a drag operation
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void MainGraphicsView::resizeEvent(QResizeEvent *event) {
    qDebug("MainGraphicsView resized to: %d, %d", event->size().width(), event->size().height());
    if( this->gui_overlay != NULL ) {
        //this->gui_overlay->setFixedSize(event->size());
        this->gui_overlay->resize(event->size());
        // needed as resizing moves the position, for some reason!
        //qDebug("### %d, %d", scene()->sceneRect().x(), scene()->sceneRect().y());
        //this->gui_overlay_item->setPos( this->scene()->sceneRect().topLeft() );
    }
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
    /*QBrush brush(QColor(255, 0, 0, 255));
    painter.fillRect(QRectF(QPointF(0, 0), this->size()), brush);*/
    //qDebug("%d, %d\n", view->rect().width(), view->rect().height());
    /*painter.setPen(Qt::green);
    painter.drawText(16, 16, "test blah blah blah 123");*/
    if( playing_gamestate->getPlayer() != NULL ) {
        int bar_y = 32;
        int text_y = bar_y - 4;
        const Character *player = playing_gamestate->getPlayer();
        painter.setPen(Qt::white);
        painter.drawText(16, text_y, player->getName().c_str());
        float fraction = ((float)player->getHealthPercent()) / (float)100.0f;
        this->drawBar(painter, 16, bar_y, 100, 16, fraction, Qt::darkGreen);
        if( player->getTargetNPC() != NULL ) {
            const Character *enemy = player->getTargetNPC();
            //qDebug("enemy: %d", enemy);
            //qDebug("name: %s", enemy->getName().c_str());
            painter.setPen(Qt::white);
            painter.drawText(132, text_y, enemy->getName().c_str());
            fraction = ((float)enemy->getHealthPercent()) / (float)100.0f;
            this->drawBar(painter, 132, bar_y, 100, 16, fraction, Qt::darkRed);
        }
    }
}

void GUIOverlay::drawBar(QPainter &painter, int x, int y, int width, int height, float fraction, QColor color) {
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

PlayingGamestate::PlayingGamestate() : scene(NULL), view(NULL), gui_overlay(NULL), player(NULL), location(NULL)
{
    LOG("PlayingGamestate::PlayingGamestate()\n");
    playingGamestate = this;

    // create RPG data
    //this->items.insert( new Weapon("Long Sword", "longsword.png") );
    this->addStandardItem( new Weapon("Long Sword", "longsword.png") );

    // create RPG world
    LOG("create RPG world\n");

    location = new Location();

    this->player = new Character("Player", "", false);
    player->initialiseHealth(100);
    //player->initialiseHealth(5);
    player->addItem( this->cloneStandardItem("Long Sword") );
    location->addCharacter(player, 2.0f, 2.0f);

    Character *enemy = new Character("Goblin", "goblin", true);
    enemy->initialiseHealth(5);
    location->addCharacter(enemy, 4.0f, 4.0f);

    location->addItem( this->cloneStandardItem("Long Sword"), 4.0f, 4.0f );

    FloorRegion *floor_regions = NULL;
    floor_regions = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
    location->addFloorRegion(floor_regions);
    floor_regions = FloorRegion::createRectangle(5.0f, 2.0f, 5.0f, 1.0f);
    location->addFloorRegion(floor_regions);
    floor_regions = FloorRegion::createRectangle(10.0f, 2.0f, 1.0f, 5.0f);
    location->addFloorRegion(floor_regions);
    floor_regions = FloorRegion::createRectangle(10.0f, 7.0f, 10.0f, 10.0f);
    location->addFloorRegion(floor_regions);
    floor_regions = FloorRegion::createRectangle(20.0f, 9.0f, 5.0f, 2.0f);
    location->addFloorRegion(floor_regions);

    {
        Polygon2D boundary;
        boundary.addPoint(Vector2D(0.0f, 0.0f));
        boundary.addPoint(Vector2D(0.0f, 5.0f));
        boundary.addPoint(Vector2D(5.0f, 5.0f));
        boundary.addPoint(Vector2D(5.0f, 3.0f));
        boundary.addPoint(Vector2D(10.0f, 3.0f));
        boundary.addPoint(Vector2D(10.0f, 17.0f));
        boundary.addPoint(Vector2D(20.0f, 17.0f));
        boundary.addPoint(Vector2D(20.0f, 11.0f));
        boundary.addPoint(Vector2D(25.0f, 11.0f));
        boundary.addPoint(Vector2D(25.0f, 9.0f));
        boundary.addPoint(Vector2D(20.0f, 9.0f));
        boundary.addPoint(Vector2D(20.0f, 7.0f));
        boundary.addPoint(Vector2D(11.0f, 7.0f));
        boundary.addPoint(Vector2D(11.0f, 2.0f));
        boundary.addPoint(Vector2D(5.0f, 2.0f));
        boundary.addPoint(Vector2D(5.0f, 0.0f));
        location->addBoundary(boundary);
    }

    // create UI
    LOG("create UI\n");
    MainWindow *window = game_g->getScreen()->getMainWindow();
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
#else
    window->setFont(qApp->font());
#endif

    /*QWidget *centralWidget = new QWidget(window);
    centralWidget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    window->setCentralWidget(centralWidget);*/

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
    view->setDragMode(QGraphicsView::ScrollHandDrag);
    view->setCacheMode(QGraphicsView::CacheBackground);

    // set up the view on the RPG world

    int pixels_per_unit = 64;
    //int pixels_per_unit = 32;
    float scale = 1.0f/(float)pixels_per_unit;
    //scene->setSceneRect(0, 0, location->getWidth(), location->getHeight());
    const float offset_y = 0.5f;
    float location_width = 0.0f, location_height = 0.0f;
    location->calculateSize(&location_width, &location_height);
    //location_height += offset_y;
    scene->setSceneRect(0, -offset_y, location_width, location_height);
    //view->fitInView(0.0f, 0.0f, location->getWidth(), location->getHeight());
    //int pixels_per_unit = 32;
    view->scale(pixels_per_unit, pixels_per_unit);

    LOG("load floor image\n");
    QPixmap floor_image = game_g->loadImage(":/gfx/textures/floor_paved.png");
    QBrush floor_brush(floor_image);
    floor_brush.setTransform(QTransform::fromScale(scale, scale));
    for(size_t i=0;i<location->getNFloorRegions();i++) {
        const FloorRegion *floor_region = location->getFloorRegion(i);
        QPolygonF polygon;
        for(size_t j=0;j<floor_region->getNPoints();j++) {
            Vector2D point = floor_region->getPoint(j);
            //QPointF qpoint(point.x, point.y + offset_y);
            QPointF qpoint(point.x, point.y);
            polygon.push_back(qpoint);
        }
        //QBrush floor_brush(Qt::white);
        scene->addPolygon(polygon, Qt::NoPen, floor_brush);
    }
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
    {
        vector<Vector2D> path_way_points = location->calculatePathWayPoints();
        for(vector<Vector2D>::const_iterator iter = path_way_points.begin(); iter != path_way_points.end(); ++iter) {
            Vector2D path_way_point = *iter;
            const float radius = 0.05f;
            scene->addEllipse(path_way_point.x - radius, path_way_point.y - radius, 2.0f*radius, 2.0f*radius, wall_pen);
        }
    }

    LOG("create animation frames\n");
    LOG("load player image\n");
    vector<AnimationLayerDefinition> player_animation_layer_definition;
    player_animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 4, AnimationSet::ANIMATIONTYPE_BOUNCE) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("run", 4, 8, AnimationSet::ANIMATIONTYPE_LOOP) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("attack", 12, 4, AnimationSet::ANIMATIONTYPE_SINGLE) );
    player_animation_layer_definition.push_back( AnimationLayerDefinition("death", 18, 6, AnimationSet::ANIMATIONTYPE_SINGLE) );
    LOG("clothes layer\n");
    int time_s = clock();
    //AnimationLayer *clothes_layer = AnimationLayer::create(":/gfx/textures/isometric_hero/clothes.png");
    this->animation_layers["clothes"] = AnimationLayer::create(":/gfx/textures/isometric_hero/clothes.png", player_animation_layer_definition);
    qDebug("time to load: %d", clock() - time_s);
    /*LOG("head layer\n");
    string head_layer_filename = ":/gfx/textures/isometric_hero/male_head1.png";
    AnimationLayer *head_layer = new AnimationLayer();*/
    //AnimationLayer *head_layer = AnimationLayer::create(":/gfx/textures/isometric_hero/male_head1.png");
    this->animation_layers["head"] = AnimationLayer::create(":/gfx/textures/isometric_hero/male_head1.png", player_animation_layer_definition);
    LOG("longsword layer");
    this->animation_layers["longsword"] = AnimationLayer::create(":/gfx/textures/isometric_hero/longsword.png", player_animation_layer_definition);

    float player_scale = 1.0f/32.0f; // 32 pixels for 1 metre

    LOG("load goblin image\n");
    //AnimationLayer *goblin_layer = AnimationLayer::create(":/gfx/textures/goblin.png");
    vector<AnimationLayerDefinition> goblin_animation_layer_definition;
    goblin_animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 4, AnimationSet::ANIMATIONTYPE_BOUNCE) );
    goblin_animation_layer_definition.push_back( AnimationLayerDefinition("run", 4, 8, AnimationSet::ANIMATIONTYPE_LOOP) );
    goblin_animation_layer_definition.push_back( AnimationLayerDefinition("attack", 20, 3, AnimationSet::ANIMATIONTYPE_SINGLE) );
    goblin_animation_layer_definition.push_back( AnimationLayerDefinition("death", 34, 6, AnimationSet::ANIMATIONTYPE_SINGLE) );
    this->animation_layers["goblin"] = AnimationLayer::create(":/gfx/textures/goblin.png", goblin_animation_layer_definition);

    LOG("add graphics items\n");
    for(set<Item *>::iterator iter = location->itemsBegin(); iter != location->itemsEnd(); ++iter) {
        Item *item = *iter;
        QGraphicsPixmapItem *graphics_item = new QGraphicsPixmapItem();
        QPixmap image = game_g->loadImage(":/gfx/textures/items/longsword.png", true, 160, 48, 32, 16);
        graphics_item->setPixmap(image);
        scene->addItem(graphics_item);
        graphics_item->setPos(item->getX(), item->getY());
        graphics_item->setScale(player_scale);
    }
    for(set<Character *>::iterator iter = location->charactersBegin(); iter != location->charactersEnd(); ++iter) {
        Character *character = *iter;
        AnimatedObject *item = new AnimatedObject();
        if( character == player ) {
            item->addAnimationLayer( this->animation_layers["clothes"] );
            item->addAnimationLayer( this->animation_layers["head"] );
            if( character->getCurrentWeapon() != NULL ) {
                item->addAnimationLayer( this->animation_layers["longsword"] );
            }
        }
        else {
            item->addAnimationLayer( this->animation_layers[ character->getAnimationName() ] );
        }
        scene->addItem(item);
        //item->setPos(character->getX(), character->getY() + offset_y);
        item->setPos(character->getX(), character->getY());
        item->setTransformOriginPoint(-32.0f*player_scale, -46.0f*player_scale);
        item->setScale(player_scale);

        character->setListener(this, item);
        //item->setAnimationSet("attack"); // test
    }
    /*QGraphicsTextItem *text_item = scene->addText("Blah");
    text_item->setDefaultTextColor(Qt::white);*/

    LOG("done\n");

    QWidget *centralWidget = new QWidget(window);
    centralWidget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    window->setCentralWidget(centralWidget);

    QHBoxLayout *layout = new QHBoxLayout();
    centralWidget->setLayout(layout);

    {
        QVBoxLayout *v_layout = new QVBoxLayout();
        layout->addLayout(v_layout);

        QPushButton *statsButton = new QPushButton("Stats");
        statsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //statsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        v_layout->addWidget(statsButton);

        QPushButton *itemsButton = new QPushButton("Items");
        itemsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //itemsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        v_layout->addWidget(itemsButton);

        QPushButton *spellsButton = new QPushButton("Spells");
        spellsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //spellsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        v_layout->addWidget(spellsButton);

        QPushButton *journalButton = new QPushButton("Journal");
        journalButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //journalButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        v_layout->addWidget(journalButton);

        QPushButton *optionsButton = new QPushButton("Options");
        optionsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //optionsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        v_layout->addWidget(optionsButton);

        QPushButton *quitButton = new QPushButton("Quit");
        quitButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        //quitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        v_layout->addWidget(quitButton);
        connect(quitButton, SIGNAL(clicked()), this, SLOT(clickedQuit()));
    }

    layout->addWidget(view);

    view->showFullScreen();

    /*{
        QPushButton *button = new QPushButton("Test", view);
        QGraphicsProxyWidget *item = scene->addWidget(button);
        item->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    }*/
    {
        gui_overlay = new GUIOverlay(this, view);
        gui_overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
        view->setGUIOverlay(gui_overlay);
    }
    /*{
        StatusBar *statusBar = new StatusBar();
        statusBar->setFixedSize(64, 16);
        QGraphicsProxyWidget *statusBarItem = scene->addWidget(statusBar);
        //statusBarItem->setPos();
        statusBarItem->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        statusBarItem->setPos(0, -offset_y);
    }*/

    LOG("Is transformed? %d\n", view->isTransformed());
}

PlayingGamestate::~PlayingGamestate() {
    playingGamestate = NULL;

    for(map<string, AnimationLayer *>::iterator iter = this->animation_layers.begin(); iter != this->animation_layers.end(); ++iter) {
        AnimationLayer *animation_layer = (*iter).second;
        delete animation_layer;
    }
    for(map<string, Item *>::iterator iter = this->standard_items.begin(); iter != this->standard_items.end(); ++iter) {
        Item *item = iter->second;
        delete item;
    }
}

void PlayingGamestate::clickedQuit() {
    qDebug("clickedQuit()");
    this->quitGame();
}

void PlayingGamestate::quitGame() {
    //qApp->quit();

    if( game_g->askQuestionWindow("Quit", "Are you sure you wish to quit?") ) {
        GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
        game_g->pushMessage(game_message);
    }
}

void PlayingGamestate::update(int time_ms) {
    //qDebug("update");

    scene->advance();
    gui_overlay->update(); // force the GUI overlay to be updated every frame (otherwise causes drawing problems on Windows at least)

    vector< set<Character *>::iterator > delete_characters;
    for(set<Character *>::iterator iter = location->charactersBegin(); iter != location->charactersEnd(); ++iter) {
        Character *character = *iter;
        if( character->update(this, time_ms) ) {
            qDebug("character is about to die: %s", character->getName().c_str());
            delete_characters.push_back(iter);
        }
    }
    for(vector< set<Character *>::iterator >::iterator iter2 = delete_characters.begin(); iter2 != delete_characters.end(); ++iter2) {
        set<Character *>::iterator iter = *iter2;
        Character *character = *iter;
        qDebug("character has died: %s", character->getName().c_str());
        location->charactersErase(iter);

        for(set<Character *>::iterator iter3 = location->charactersBegin(); iter3 != location->charactersEnd(); ++iter3) {
            Character *ch = *iter3;
            if( ch->getTargetNPC() == character ) {
                ch->setTargetNPC(NULL);
            }
        }

        delete character; // also removes character from the QGraphicsScene, via the listeners
        if( character == this->player ) {
            this->player = NULL;
            game_g->showInfoWindow("Game over", "You have died!");
            GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
            game_g->pushMessage(game_message);
        }
    }

    // update the view
    /*for(set<QGraphicsItem *>::iterator iter = this->graphicsitems_characters.begin(); iter != this->graphicsitems_characters.end(); ++iter) {
        QGraphicsItem *item = *iter;
        //const Character *character = qgraphicsitem_cast<const Character *>(item->data(scene_item_character_key_c));
        const Character *character = static_cast<const Character *>(item->data(scene_item_character_key_c).value<void *>());
        //qDebug("character at: %f, %f", character->getX(), character->getY());
    }*/
}

void PlayingGamestate::characterTurn(const Character *character, void *user_data, Vector2D dir) {
    AnimatedObject *item = static_cast<AnimatedObject *>(user_data);
    float angle = atan2(dir.y, dir.x);
    if( angle < 0.0f )
        angle += 2.0*M_PI;
    angle /= 2.0*M_PI;
    float turn = angle*((int)N_DIRECTIONS) + 0.5f;
    int turn_i = (int)turn;
    /*qDebug("angle %f", angle);
    qDebug("turn %f", turn);
    qDebug("turn_i %d", turn_i);*/
    turn_i += 4; // 0 is west
    Direction direction = (Direction)(turn_i % (int)N_DIRECTIONS);
    item->setDirection(direction);
}

void PlayingGamestate::characterMoved(const Character *character, void *user_data) {
    //QGraphicsItem *item = static_cast<QGraphicsItem *>(user_data);
    AnimatedObject *item = static_cast<AnimatedObject *>(user_data);
    QPointF old_pos = item->pos();
    QPointF new_pos(character->getX(), character->getY());
    if( new_pos != old_pos ) {
        item->setPos(new_pos);
        /*Direction direction = DIRECTION_W;
        if( new_pos.x() > old_pos.x() ) {
            direction = DIRECTION_E;
        }*/
        QPointF dir = new_pos - old_pos;
        Vector2D vdir(dir.x(), dir.y());
        this->characterTurn(character, user_data, vdir);
    }
}

void PlayingGamestate::characterSetAnimation(const Character *character, void *user_data, string name) {
    AnimatedObject *item = static_cast<AnimatedObject *>(user_data);
    item->setAnimationSet(name);
}

void PlayingGamestate::characterDeath(Character *character, void *user_data) {
    AnimatedObject *item = static_cast<AnimatedObject *>(user_data);
    character->setListener(NULL, NULL);
    delete item;
}

/*void PlayingGamestate::mouseClick(int m_x, int m_y) {
    qDebug("PlayingGamestate::mouseClick(%d, %d)", m_x, m_y);
}*/

void PlayingGamestate::clickedMainView(float scene_x, float scene_y) {
    if( player != NULL && !player->isDead() ) {
        //player->setPos(scene_x, scene_y);

        Vector2D dest(scene_x, scene_y);

        // search for clicking on NPC
        float min_dist = 0.0f;
        Character *target_npc = NULL;
        for(set<Character *>::iterator iter = location->charactersBegin(); iter != location->charactersEnd(); ++iter) {
            Character *character = *iter;
            if( character == player )
                continue;
            double dist = (dest - character->getPos()).magnitude();
            if( dist <= character->getRadius() ) {
                if( target_npc == NULL || dist < min_dist ) {
                    target_npc = character;
                    min_dist = dist;
                }
            }
        }
        player->setTargetNPC(target_npc); // n.b., if no NPC selected, we therefore set to NULL

        if( dest != player->getPos() ) {
            Vector2D hit_pos;
            bool hit = location->intersectSweptSquareWithBoundaries(player, &hit_pos, player->getPos(), dest, player->getRadius());
            if( hit ) {
                qDebug("hit at: %f, %f", hit_pos.x, hit_pos.y);
                dest = hit_pos;
            }
            player->setDestination(dest.x, dest.y);
        }
    }
}

void PlayingGamestate::addStandardItem(Item *item) {
    this->standard_items[item->getName()] = item;
}

Item *PlayingGamestate::cloneStandardItem(string name) {
    map<string, Item *>::const_iterator iter = this->standard_items.find(name);
    if( iter == this->standard_items.end() ) {
        LOG("can't clone standard item which doesn't exist: %s\n", name.c_str());
        throw string("Unknown standard item");
    }
    const Item *item = iter->second;
    return item->clone();
}

Game::Game() {
    game_g = this;

    // initialise paths
    QString pathQt (QDesktopServices::storageLocation (QDesktopServices::DataLocation));
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
#else
    LOG("Platform: UNKNOWN\n");
#endif
}

void Game::run() {
    screen = new Screen();

    gamestate = new OptionsGamestate();

    screen->runMainLoop();

    delete gamestate;
    delete screen;
}

void Game::update(int time_ms) {
    while( !message_queue.empty() ) {
        GameMessage *message = message_queue.front();
        message_queue.pop();
        try {
            switch( message->getGameMessageType() ) {
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING:
                delete gamestate;
                gamestate = new PlayingGamestate();
                this->getScreen()->getMainWindow()->unsetCursor();
                break;
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS:
                delete gamestate;
                gamestate = new OptionsGamestate();
                break;
            default:
                Q_ASSERT(false);
            }
        }
        catch(string &error) {
            this->getScreen()->getMainWindow()->unsetCursor();
            if( gamestate != NULL ) {
                delete gamestate;
                gamestate = NULL;
            }
            delete message;
            LOG("exception creating new gamestate: %s\n", error.c_str());
            stringstream message;
            message << "Failed to load game data:\n" << error;
            game_g->showErrorWindow(message.str().c_str());
            qApp->quit();
        }
        delete message;
    }

    if( gamestate != NULL ) {
        gamestate->update(time_ms);
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
    //qDebug("###: %s", buffer);
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

void Game::showErrorWindow(const char *message) {
    this->getScreen()->enableTimers(false);
    QMessageBox::critical(this->getScreen()->getMainWindow(), "Error", message);
    this->getScreen()->enableTimers(true);
}

void Game::showInfoWindow(const char *title, const char *message) {
    this->getScreen()->enableTimers(false);
    QMessageBox::information(this->getScreen()->getMainWindow(), title, message);
    this->getScreen()->enableTimers(true);
}

bool Game::askQuestionWindow(const char *title, const char *message) {
    //LOG("Game::askQuestionWindow: %s\n", message);
    qDebug("Game::askQuestionWindow: %s", message);
    //this->getScreen()->getMainWindow()->blockSignals(true);
    this->getScreen()->enableTimers(false);
    int res = QMessageBox::question(this->getScreen()->getMainWindow(), title, message, QMessageBox::Yes, QMessageBox::No);
    //this->getScreen()->getMainWindow()->blockSignals(false);
    this->getScreen()->enableTimers(true);
    //LOG("    answer is %d", res);
    qDebug("    answer is %d", res);
    return res == QMessageBox::Yes;
}
