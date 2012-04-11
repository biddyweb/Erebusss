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

const float player_scale = 1.0f/32.0f; // 32 pixels for 1 metre
//const float item_scale = 1.0f/64.0f; // 64 pixels for 1 metre
const float item_width = 1.0f;
//const float scenery_width = item_width;
const float font_scale = 1.0f/64.0f;

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

AnimatedObject::AnimatedObject() : /*animation_layer(NULL), c_animation_set(NULL),*/
    set_c_animation_name(false), c_direction(DIRECTION_E), c_frame(0), animation_time_start_ms(0)
{
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
        //int time_elapsed_ms = game_g->getScreen()->getElapsedMS() - animation_time_start_ms;
        int time_elapsed_ms = game_g->getScreen()->getGameTimeTotalMS() - animation_time_start_ms;
        /*int c_frame = ( time_elapsed_ms / ms_per_frame );
        this->setFrame(c_frame);*/
        int n_frame = ( time_elapsed_ms / ms_per_frame );
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

void AnimatedObject::clearAnimationLayers() {
    this->animation_layers.clear();
    this->c_animation_sets.clear();
}

void AnimatedObject::setAnimationSet(string name) {
    /*const AnimationSet *new_animation_set = animation_layer->getAnimationSet(name);
    if( new_animation_set != c_animation_set ) {
        c_animation_set = new_animation_set;
        animation_time_start_ms = 0;
        this->setFrame(0);
    }*/
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

TextEffect::TextEffect(const QString &text, int duration_ms) :
    QGraphicsTextItem(text), time_expire(0) {
    this->setDefaultTextColor(Qt::white);
    this->time_expire = game_g->getScreen()->getGameTimeTotalMS() + duration_ms;
}

void TextEffect::advance(int phase) {
    if( phase == 0 ) {
        if( game_g->getScreen()->getGameTimeTotalMS() >= time_expire ) {
            this->scene()->removeItem(this);
            this->deleteLater();
        }
    }
}

OptionsGamestate::OptionsGamestate()
{
    LOG("OptionsGamestate::OptionsGamestate()\n");
    optionsGamestate = this;

    MainWindow *window = game_g->getScreen()->getMainWindow();
/*#if defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
#else
    QFont font("Verdana", 48, QFont::Bold);
    window->setFont(font);
#endif*/
    QFont font = game_g->getFontBig();
    window->setFont(font);

    QWidget *centralWidget = new QWidget(window);
    centralWidget->setContextMenuPolicy(Qt::NoContextMenu); // explicitly forbid usage of context menu so actions item is not shown menu
    window->setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout();
    centralWidget->setLayout(layout);

    QLabel *titleLabel = new QLabel("erebus");
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
    // applications don't quite on Android.
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
            LOG("clicked: %f, %f\n", m_scene.x(), m_scene.y());
            playing_gamestate->clickedMainView(m_scene.x(), m_scene.y());
        }
        // else, this was a drag operation
    }

    QGraphicsView::mouseReleaseEvent(event);
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

    if( this->display_progress ) {
        const int x_off = 16;
        const int hgt = 64;
        this->drawBar(painter, x_off, this->height()/2 - hgt/2, this->width() - 2*x_off, hgt, ((float)this->progress_percent)/100.0f, Qt::darkRed);
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

ItemsWindow::ItemsWindow(PlayingGamestate *playing_gamestate) :
    playing_gamestate(playing_gamestate), list(NULL),
    dropButton(NULL), armButton(NULL), wearButton(NULL), useButton(NULL),
    view_type(VIEWTYPE_ALL)
{

    playing_gamestate->addWidget(this);
    Character *player = playing_gamestate->getPlayer();

    /*QFont font = game_g->getScreen()->getMainWindow()->font();
    font.setPointSize( font.pointSize() + 6 );
    this->setFont(font);*/

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

        QPushButton *viewShieldsButton = new QPushButton("Shields");
        h_layout->addWidget(viewShieldsButton);
        connect(viewShieldsButton, SIGNAL(clicked()), this, SLOT(clickedViewShields()));

        QPushButton *viewArmourButton = new QPushButton("Armour");
        h_layout->addWidget(viewArmourButton);
        connect(viewArmourButton, SIGNAL(clicked()), this, SLOT(clickedViewArmour()));

        QPushButton *viewAmmoButton = new QPushButton("Ammo");
        h_layout->addWidget(viewAmmoButton);
        connect(viewAmmoButton, SIGNAL(clicked()), this, SLOT(clickedViewAmmo()));

        QPushButton *viewMagicButton = new QPushButton("Magic");
        h_layout->addWidget(viewMagicButton);
        connect(viewMagicButton, SIGNAL(clicked()), this, SLOT(clickedViewMagic()));
    }

    list = new QListWidget();
    {
        QFont list_font = list->font();
        list_font.setPointSize( list_font.pointSize() + 8 );
        list->setFont(list_font);
    }
    layout->addWidget(list);
    list->setSelectionMode(QAbstractItemView::SingleSelection);

    /*int weight = 0;
    for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
        Item *item = *iter;
        weight += item->getWeight();
        QString item_str = this->getItemString(item);
        list->addItem( item_str );
        list_items.push_back(item);
    }*/
    this->refreshList();

    connect(list, SIGNAL(currentRowChanged(int)), this, SLOT(changedSelectedItem(int)));

    {
        QHBoxLayout *h_layout = new QHBoxLayout();
        layout->addLayout(h_layout);

        QLabel *goldLabel = new QLabel("Gold: " + QString::number( player->getGold() ));
        goldLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // needed to fix problem of having too little vertical space (on Qt Smartphone Simulator at least)
        h_layout->addWidget(goldLabel);

        weightLabel = new QLabel(""); // label set in setWeightLabel()
        weightLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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

    QPushButton *closeButton = new QPushButton("Close");
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
        QString item_str = this->getItemString(item);
        list->addItem( item_str );
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


void ItemsWindow::clickedViewShields() {
    this->changeView(VIEWTYPE_SHIELDS);
}

void ItemsWindow::clickedViewArmour() {
    this->changeView(VIEWTYPE_ARMOUR);
}

void ItemsWindow::clickedViewAmmo() {
    this->changeView(VIEWTYPE_AMMO);
}

void ItemsWindow::clickedViewMagic() {
    this->changeView(VIEWTYPE_MAGIC);
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

PlayingGamestate::PlayingGamestate() :
    scene(NULL), view(NULL), gui_overlay(NULL), /*mainwindow(NULL),*/ subwindow(NULL), main_stacked_widget(NULL),
    player(NULL), location(NULL)
{
    LOG("PlayingGamestate::PlayingGamestate()\n");
    playingGamestate = this;

    game_g->getScreen()->setPaused(true);

    // create UI
    LOG("create UI\n");
    MainWindow *window = game_g->getScreen()->getMainWindow();
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
#else
    window->setFont(qApp->font());
#endif

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

    layout->addWidget(view);

    view->showFullScreen();

    gui_overlay = new GUIOverlay(this, view);
    gui_overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    view->setGUIOverlay(gui_overlay);

    gui_overlay->setProgress(0);
    qApp->processEvents();

    // create RPG data
    LOG("create RPG data\n");

    Item *item = NULL;
    Weapon *weapon = NULL;

    this->item_images["longsword"] = game_g->loadImage(":/gfx/textures/items/longsword.png");
    this->addStandardItem( new Weapon("Long Sword", "longsword", 14, "longsword") );

    this->item_images["longbow"] = game_g->loadImage(":/gfx/textures/items/longbow.png");
    this->addStandardItem( weapon = new Weapon("Longbow", "longbow", 5, "longbow") );
    weapon->setTwoHanded(true);
    weapon->setRanged(true);
    weapon->setRequiresAmmo(true, "Arrows");

    this->item_images["shield"] = game_g->loadImage(":/gfx/textures/items/shield.png");
    this->addStandardItem( new Shield("Shield", "shield", 36, "shield") );

    this->item_images["leather_armour"] = game_g->loadImage(":/gfx/textures/items/leather_armor.png");
    this->addStandardItem( new Armour("Leather Armour", "leather_armour", 100, 2));

    this->item_images["arrow"] = game_g->loadImage(":/gfx/textures/items/arrow.png", true, 0, 16, 64, 32);
    this->addStandardItem( new Ammo("Arrows", "arrow", "arrow", 20) );
    //this->addStandardItem( new Ammo("Arrows", "arrow", "arrow", 3) );

    this->item_images["potion_red"] = game_g->loadImage(":/gfx/textures/items/potion_red.png");
    this->addStandardItem( item = new Item("Potion of Healing", "potion_red", 1) );
    item->setUse(ITEMUSE_POTION_HEALING);
    item->setRating(1);
    item->setMagical(true);

    this->item_images["gold"] = game_g->loadImage(":/gfx/textures/items/gold.png");
    this->addStandardItem( new Currency("Gold", "gold"));

    gui_overlay->setProgress(10);

    {
        QPixmap containers = game_g->loadImage(":/gfx/textures/scenery/containers.png");
        /*this->scenery_images["chest"] = containers.copy(0, 0, 64, 64);
        this->scenery_opened_images["chest"] = containers.copy(0, 64, 64, 64);*/
        this->scenery_images["chest"] = containers.copy(6, 2, 52, 52);
        this->scenery_opened_images["chest"] = containers.copy(6, 66, 52, 52);
        this->scenery_images["barrel"] = containers.copy(64, 0, 64, 64);
        this->scenery_opened_images["barrel"] = containers.copy(64, 64, 64, 64);
        this->scenery_images["crate"] = containers.copy(128, 0, 64, 64);
        this->scenery_opened_images["crate"] = containers.copy(128, 64, 64, 64);
    }

    gui_overlay->setProgress(20);
    qApp->processEvents();

    this->player = new Character("Player", "", false);
    player->initialiseHealth(100);
    //player->initialiseHealth(5); // test
    player->addItem( this->cloneStandardItem("Long Sword") );
    player->addItem( this->cloneStandardItem("Shield") );
    player->addItem( this->cloneStandardItem("Longbow") );
    player->addItem( this->cloneStandardItem("Leather Armour") );
    player->addItem( this->cloneStandardItem("Arrows") );

    // create RPG world
    LOG("create RPG world\n");

    location = new Location();

    {
        Character *enemy = new Character("Goblin", "goblin", true);
        enemy->initialiseHealth(5);
        enemy->addGold(8);
        //enemy->setHostile(false); // test
        //enemy->addItem( this->cloneStandardItem("Leather Armour") ); // test
        location->addCharacter(enemy, 4.0f, 4.0f);
    }
    {
        Character *enemy = new Character("Goblin", "goblin", true);
        enemy->initialiseHealth(6);
        enemy->addGold(9);
        location->addCharacter(enemy, 16.0f, 14.0f);
    }

    //location->addItem( this->cloneStandardItem("Long Sword"), 4.0f, 4.0f );
    location->addItem( this->cloneStandardItem("Longbow"), 4.0f, 4.0f );
    location->addItem( this->cloneStandardItem("Shield"), 4.0f, 3.0f );
    location->addItem( this->cloneStandardItem("Leather Armour"), 2.0f, 4.0f );
    /*{
        Currency *item = static_cast<Currency *>(this->cloneStandardItem("Gold"));
        LOG("gold location item: %d\n", item);
        item->setValue(5);
        location->addItem(item, 1.0f, 1.0f);
    }*/
    location->addItem( this->cloneGoldItem(5), 1.0f, 1.0f );
    location->addItem( this->cloneStandardItem("Arrows"), 2.0f, 1.0f );
    location->addItem( this->cloneStandardItem("Potion of Healing"), 3.0f, 4.0f );

    Scenery *scenery = NULL;
    location->addScenery( scenery = new Scenery("Chest", "chest"), 2.0f, 2.0f );
    scenery->setBlocking(true);
    scenery->setSize(0.8f, 0.8f);
    scenery->addItem( this->cloneStandardItem("Potion of Healing") );
    scenery->addItem( this->cloneStandardItem("Longbow") );

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
    location->createBoundariesForScenery();
    location->calculateDistanceGraph();

    location->addCharacter(player, 3.0f, 2.0f);

    location->setListener(this, NULL); // must do after creating the location and its contents, so it doesn't try to add items to the scene, etc

    gui_overlay->setProgress(30);
    qApp->processEvents();

    // set up the view on the RPG world

    int pixels_per_unit = 64;
    //int pixels_per_unit = 32;
    float scale = 1.0f/(float)pixels_per_unit;
    //scene->setSceneRect(0, 0, location->getWidth(), location->getHeight());
    const float offset_y = 0.5f;
    float location_width = 0.0f, location_height = 0.0f;
    location->calculateSize(&location_width, &location_height);
    scene->setSceneRect(0, -offset_y, location_width, location_height + 2*offset_y);
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
    /*{
        vector<Vector2D> path_way_points = location->calculatePathWayPoints();
        for(vector<Vector2D>::const_iterator iter = path_way_points.begin(); iter != path_way_points.end(); ++iter) {
            Vector2D path_way_point = *iter;
            const float radius = 0.05f;
            scene->addEllipse(path_way_point.x - radius, path_way_point.y - radius, 2.0f*radius, 2.0f*radius, wall_pen);
        }
    }*/
    {
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
    gui_overlay->setProgress(50);
    qApp->processEvents();
    /*LOG("head layer\n");
    string head_layer_filename = ":/gfx/textures/isometric_hero/male_head1.png";
    AnimationLayer *head_layer = new AnimationLayer();*/
    //AnimationLayer *head_layer = AnimationLayer::create(":/gfx/textures/isometric_hero/male_head1.png");
    this->animation_layers["head"] = AnimationLayer::create(":/gfx/textures/isometric_hero/male_head1.png", player_animation_layer_definition);
    gui_overlay->setProgress(60);
    qApp->processEvents();
    LOG("longsword layer\n");
    this->animation_layers["longsword"] = AnimationLayer::create(":/gfx/textures/isometric_hero/longsword.png", player_animation_layer_definition);
    LOG("longbow layer\n");
    this->animation_layers["longbow"] = AnimationLayer::create(":/gfx/textures/isometric_hero/longbow.png", player_animation_layer_definition);
    LOG("shield layer\n");
    this->animation_layers["shield"] = AnimationLayer::create(":/gfx/textures/isometric_hero/shield.png", player_animation_layer_definition);
    gui_overlay->setProgress(70);
    qApp->processEvents();

    LOG("load goblin image\n");
    //AnimationLayer *goblin_layer = AnimationLayer::create(":/gfx/textures/goblin.png");
    vector<AnimationLayerDefinition> goblin_animation_layer_definition;
    goblin_animation_layer_definition.push_back( AnimationLayerDefinition("", 0, 4, AnimationSet::ANIMATIONTYPE_BOUNCE) );
    goblin_animation_layer_definition.push_back( AnimationLayerDefinition("run", 4, 8, AnimationSet::ANIMATIONTYPE_LOOP) );
    goblin_animation_layer_definition.push_back( AnimationLayerDefinition("attack", 20, 3, AnimationSet::ANIMATIONTYPE_SINGLE) );
    goblin_animation_layer_definition.push_back( AnimationLayerDefinition("ranged", 24, 4, AnimationSet::ANIMATIONTYPE_SINGLE) );
    goblin_animation_layer_definition.push_back( AnimationLayerDefinition("death", 34, 6, AnimationSet::ANIMATIONTYPE_SINGLE) );
    this->animation_layers["goblin"] = AnimationLayer::create(":/gfx/textures/goblin.png", goblin_animation_layer_definition);

    gui_overlay->setProgress(80);
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

    gui_overlay->setProgress(90);
    qApp->processEvents();

    for(set<Character *>::iterator iter = location->charactersBegin(); iter != location->charactersEnd(); ++iter) {
        Character *character = *iter;
        AnimatedObject *object = new AnimatedObject();
        /*if( character == player ) {
            object->addAnimationLayer( this->animation_layers["clothes"] );
            object->addAnimationLayer( this->animation_layers["head"] );
            if( character->getCurrentWeapon() != NULL ) {
                object->addAnimationLayer( this->animation_layers[ character->getCurrentWeapon()->getAnimationFilename().c_str() ] );
            }
        }
        else {
            object->addAnimationLayer( this->animation_layers[ character->getAnimationName() ] );
        }*/
        this->characterUpdateGraphics(character, object);
        scene->addItem(object);
        object->setPos(character->getX(), character->getY());
        object->setTransformOriginPoint(-32.0f*player_scale, -46.0f*player_scale);
        object->setScale(player_scale);

        character->setListener(this, object);
        //item->setAnimationSet("attack"); // test
    }

    /*{
        TextEffect *text_effect = new TextEffect("Welcome to Erebus", 1000);
        text_effect->setPos( player->getPos().x, player->getPos().y );
        scene->addItem(text_effect);
    }*/
    this->addTextEffect("Welcome to Erebus", player->getPos(), 2000);

    gui_overlay->unsetProgress();
    qApp->processEvents();

    game_g->getScreen()->setPaused(false);
    game_g->getScreen()->restartElapsedTimer();
    //this->paused = false;
    //this->saved_elapsed_time_ms = game_g->getScreen()->getElapsedMS();
    LOG("View is transformed? %d\n", view->isTransformed());
    LOG("done\n");
}

PlayingGamestate::~PlayingGamestate() {
    LOG("PlayingGamestate::~PlayingGamestate()\n");
    this->clickedCloseSubwindow();
    MainWindow *window = game_g->getScreen()->getMainWindow();
    window->centralWidget()->deleteLater();
    window->setCentralWidget(NULL);

    playingGamestate = NULL;

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
    LOG("done\n");
}

void PlayingGamestate::locationAddItem(const Location *location, Item *item) {
    if( this->location == location ) {
        QGraphicsPixmapItem *object = new QGraphicsPixmapItem();
        item->setUserGfxData(object);
        //object->setPixmap( item->getImage()->getPixmap() );
        //object->setPixmap( this->item_images[item->getName().c_str()] );
        //object->setPixmap( this->item_images[item->getImageName().c_str()] );
        map<string, QPixmap>::iterator image_iter = this->item_images.find(item->getImageName().c_str());
        if( image_iter == this->item_images.end() ) {
            LOG("failed to find image for item: %s\n", item->getName().c_str());
            LOG("    image name: %s\n", item->getImageName().c_str());
            throw string("Failed to find item's image");
        }
        object->setPixmap( image_iter->second );
        scene->addItem(object);
        object->setPos(item->getX(), item->getY());
        //object->setTransformOriginPoint(-32.0f*item_scale, -16.0f*item_scale);
        float item_scale = item_width / object->pixmap().width();
        object->setTransformOriginPoint(-0.5f*object->pixmap().width()*item_scale, -0.5f*object->pixmap().height()*item_scale);
        //object->setTransformOriginPoint(-0.5f*item_width, -0.5f*(object->pixmap().height()*item_width)/(float)object->pixmap().width());
        object->setScale(item_scale);
    }
}

void PlayingGamestate::locationRemoveItem(const Location *location, Item *item) {
    if( this->location == location ) {
        QGraphicsPixmapItem *object = static_cast<QGraphicsPixmapItem *>(item->getUserGfxData());
        item->setUserGfxData(NULL);
        scene->removeItem(object);
        delete object;
    }
}

void PlayingGamestate::locationAddScenery(const Location *location, Scenery *scenery) {
    if( this->location == location ) {
        QGraphicsPixmapItem *object = new QGraphicsPixmapItem();
        scenery->setUserGfxData(object);
        this->locationUpdateScenery(scenery);
        scene->addItem(object);
        object->setPos(scenery->getX(), scenery->getY());
        object->setZValue(object->pos().y());
        //float scenery_scale = scenery_width / object->pixmap().width();
        // n.b., aspect-ratio of scenery should match that of the corresponding image for this scenery!
        float scenery_scale = scenery->getWidth() / object->pixmap().width();
        object->setTransformOriginPoint(-0.5f*object->pixmap().width()*scenery_scale, -0.5f*object->pixmap().height()*scenery_scale);
        object->setScale(scenery_scale);
    }
}

void PlayingGamestate::locationRemoveScenery(const Location *location, Scenery *scenery) {
    if( this->location == location ) {
        QGraphicsPixmapItem *object = static_cast<QGraphicsPixmapItem *>(scenery->getUserGfxData());
        scenery->setUserGfxData(NULL);
        scene->removeItem(object);
        delete object;
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

void PlayingGamestate::clickedItems() {
    LOG("clickedItems()\n");
    this->clickedCloseSubwindow();

    subwindow = new ItemsWindow(this);
    game_g->getScreen()->setPaused(true);

    /*
    //subwindow = new QWidget(this->view);
    subwindow = new QWidget();
    //subwindow = new QWidget(game_g->getScreen()->getMainWindow());
    //game_g->getScreen()->getMainWindow()->setCentralWidget(subwindow);

    QVBoxLayout *layout = new QVBoxLayout();
    subwindow->setLayout(layout);

    list = new QListWidget();
    layout->addWidget(list);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    QFont font = game_g->getScreen()->getMainWindow()->font();
    font.setPointSize( font.pointSize() + 6 );
    list->setFont(font);

    list_items.clear();
    for(set<Item *>::iterator iter = player->itemsBegin(); iter != player->itemsEnd(); ++iter) {
        Item *item = *iter;
        QString item_str = item->getName().c_str();
        if( player->getCurrentWeapon() == item ) {
            item_str += " [Current Weapon]";
        }
        list->addItem( item_str );
        list_items.push_back(item);
    }

    {
        QVBoxLayout *h_layout = new QVBoxLayout();
        layout->addLayout(h_layout);

        QPushButton *dropButton = new QPushButton("Drop Item");
        layout->addWidget(dropButton);
        connect(dropButton, SIGNAL(clicked()), this, SLOT(clickedDropItem()));
    }

    QPushButton *closeButton = new QPushButton("Close");
    layout->addWidget(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(clickedCloseSubwindow()));
    */

    //subwindow->showFullScreen();
    //game_g->getScreen()->getMainWindow()->hide();
    /*mainwindow->setParent(NULL); // stop it being deleted!
    game_g->getScreen()->getMainWindow()->setCentralWidget(subwindow);
    game_g->getScreen()->getMainWindow()->update();*/
    //this->main_stacked_widget->addWidget(subwindow);
    //this->main_stacked_widget->setCurrentWidget(subwindow);
}

void PlayingGamestate::clickedOptions() {
    LOG("clickedOptions()\n");
    this->clickedCloseSubwindow();

    subwindow = new QWidget();
    //subwindow = new QWidget( game_g->getScreen()->getMainWindow() );
    //mainwindow->setParent(NULL); // stop it being deleted!
    //game_g->getScreen()->getMainWindow()->setCentralWidget(subwindow);
    this->main_stacked_widget->addWidget(subwindow);
    this->main_stacked_widget->setCurrentWidget(subwindow);
    game_g->getScreen()->setPaused(true);

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
    if( location->hasEnemies(this) ) {
        game_g->showInfoWindow("Rest", "You cannot rest here - enemies are nearby");
        return;
    }
    if( game_g->askQuestionWindow("Rest", "Rest until fully healed?") ) {
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

void PlayingGamestate::clickedCloseSubwindow() {
    LOG("clickedCloseSubwindow\n");
    if( subwindow != NULL ) {
        //game_g->getScreen()->getMainWindow()->setCentralWidget(mainwindow);
        //game_g->getScreen()->getMainWindow()->show();
        this->main_stacked_widget->setCurrentIndex(0);
        game_g->getScreen()->setPaused(false);
        //subwindow->close();
        //delete subwindow;
        subwindow->deleteLater();
        subwindow = NULL;
    }
}

void PlayingGamestate::quitGame() {
    //qApp->quit();

    if( game_g->askQuestionWindow("Quit", "Are you sure you wish to quit?") ) {
        GameMessage *game_message = new GameMessage(GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS);
        game_g->pushMessage(game_message);
    }
}

void PlayingGamestate::update() {
    //LOG("update: %d\n", time_ms);

    if( game_g->getScreen()->isPaused() ) {
        return;
    }

    scene->advance();
    gui_overlay->update(); // force the GUI overlay to be updated every frame (otherwise causes drawing problems on Windows at least)

    vector< set<Character *>::iterator > delete_characters;
    for(set<Character *>::iterator iter = location->charactersBegin(); iter != location->charactersEnd(); ++iter) {
        Character *character = *iter;
        if( character->update(this) ) {
            LOG("character is about to die: %s\n", character->getName().c_str());
            delete_characters.push_back(iter);
        }
    }
    for(vector< set<Character *>::iterator >::iterator iter2 = delete_characters.begin(); iter2 != delete_characters.end(); ++iter2) {
        set<Character *>::iterator iter = *iter2;
        Character *character = *iter;
        LOG("character has died: %s\n", character->getName().c_str());
        location->removeCharacter(character);

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

void PlayingGamestate::characterMoved(const Character *character, void *user_data) {
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

        // search for clicking on an NPC
        float min_dist = 0.0f;
        Character *target_npc = NULL;
        for(set<Character *>::iterator iter = location->charactersBegin(); iter != location->charactersEnd(); ++iter) {
            Character *character = *iter;
            if( character == player )
                continue;
            float dist = (dest - character->getPos()).magnitude();
            if( dist <= npc_radius_c ) {
                if( target_npc == NULL || dist < min_dist ) {
                    done = true;
                    target_npc = character;
                    min_dist = dist;
                }
            }
        }
        player->setTargetNPC(target_npc); // n.b., if no NPC selected, we therefore set to NULL

        if( !done ) {
            // search for clicking on an item
            Item *picked_item = NULL;
            for(set<Item *>::iterator iter = location->itemsBegin(); iter != location->itemsEnd(); ++iter) {
                Item *item = *iter;
                float dist_from_click = (dest - item->getPos()).magnitude();
                float dist_from_player = (player->getPos() - item->getPos()).magnitude();
                if( dist_from_click <= 0.5f && dist_from_player <= npc_radius_c ) {
                    if( picked_item == NULL || dist_from_click < min_dist ) {
                        done = true;
                        picked_item = item;
                        min_dist = dist_from_click;
                    }
                }
            }
            if( picked_item != NULL ) {
                /*TextEffect *text_effect = new TextEffect(picked_item->getName().c_str(), 2000);
                text_effect->setPos( player->getPos().x - 0.5*font_scale*text_effect->boundingRect().width(), player->getPos().y - 1.0f );
                text_effect->setScale(font_scale);
                text_effect->setZValue(text_effect->pos().y() + 1.0f);
                scene->addItem(text_effect);*/
                this->addTextEffect(picked_item->getName(), player->getPos(), 2000);

                player->pickupItem(picked_item);
            }
        }

        Scenery *ignore_scenery = NULL;
        if( !done ) {
            // search for clicking on a scenery
            Scenery *selected_scenery = NULL;
            for(set<Scenery *>::iterator iter = location->scenerysBegin(); iter != location->scenerysEnd(); ++iter) {
                Scenery *scenery = *iter;
                Vector2D scenery_pos = scenery->getPos();
                float scenery_width = scenery->getWidth();
                float scenery_height = scenery->getHeight();
                if( dest.x >= scenery_pos.x - 0.5f * scenery_width - npc_radius_c && dest.x <= scenery_pos.x + 0.5f * scenery_width + npc_radius_c &&
                    dest.y >= scenery_pos.y - 0.5f * scenery_height - npc_radius_c && dest.y <= scenery_pos.y + 0.5f * scenery_height + npc_radius_c ) {
                    // clicked on or near this scenery
                    ignore_scenery = scenery;
                    if( dest.x >= scenery_pos.x - 0.5f * scenery_width && dest.x <= scenery_pos.x + 0.5f * scenery_width &&
                        dest.y >= scenery_pos.y - 0.5f * scenery_height && dest.y <= scenery_pos.y + 0.5f * scenery_height ) {
                        // clicked on this scenery
                        Vector2D player_pos = player->getPos();
                        float player_dist_x = abs(player_pos.x - scenery_pos.x) - 0.5f * scenery_width;
                        float player_dist_y = abs(player_pos.y - scenery_pos.y) - 0.5f * scenery_height;
                        float player_dist = player_dist_x > player_dist_y ? player_dist_x : player_dist_y;
                        if( player_dist <= npc_radius_c + 0.5f ) {
                            if( selected_scenery == NULL ) {
                                done = true;
                                selected_scenery = scenery;
                                min_dist = 0.0f;
                            }
                        }
                    }
                }
            }

            if( selected_scenery != NULL ) {
                if( selected_scenery->getNItems() > 0 ) {
                    for(set<Item *>::iterator iter = selected_scenery->itemsBegin(); iter != selected_scenery->itemsEnd(); ++iter) {
                        Item *item = *iter;
                        location->addItem(item, player->getX(), player->getY());
                    }
                    selected_scenery->eraseAllItems();
                    this->addTextEffect("Found some items!", player->getPos(), 2000);
                }
                if( !selected_scenery->isOpened() ) {
                    selected_scenery->setOpened(true);
                }
            }
        }

        if( dest != player->getPos() ) {
            /*Vector2D hit_pos;
            bool hit = location->intersectSweptSquareWithBoundariesAndNPCs(player, &hit_pos, player->getPos(), dest, npc_radius_c);
            if( hit ) {
                LOG("hit at: %f, %f\n", hit_pos.x, hit_pos.y);
                dest = hit_pos;
            }*/
            player->setDestination(dest.x, dest.y, ignore_scenery);
            if( player->calculateItemsWeight() > player->getCanCarryWeight() ) {
                this->addTextEffect("You are carrying too much weight to move!", player->getPos(), 2000);
            }
        }
    }
}

void PlayingGamestate::addWidget(QWidget *widget) {
    this->main_stacked_widget->addWidget(widget);
    this->main_stacked_widget->setCurrentWidget(widget);
}

void PlayingGamestate::addTextEffect(string text, Vector2D pos, int duration_ms) {
    TextEffect *text_effect = new TextEffect(text.c_str(), duration_ms);
    text_effect->setPos( pos.x - 0.5*font_scale*text_effect->boundingRect().width(), pos.y - 1.0f );
    /*QFont font = text_effect->font();
    font.setPointSize(1);
    text_effect->setFont(font);*/
    text_effect->setScale(font_scale);
    text_effect->setZValue(text_effect->pos().y() + 1.0f);
    scene->addItem(text_effect);
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
#else
    LOG("Platform: UNKNOWN\n");
#endif
}

void Game::run() {
    screen = new Screen();

    // setup fonts
    MainWindow *window = game_g->getScreen()->getMainWindow();
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
    this->font_std = window->font();
    this->font_big = window->font();
#else
    this->font_std = window->font();
    this->font_big = QFont("Verdana", 48, QFont::Bold);
#endif

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
                delete gamestate;
                gamestate = NULL;
                gamestate = new PlayingGamestate();
                this->getScreen()->getMainWindow()->unsetCursor();
                break;
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_OPTIONS:
                delete gamestate;
                gamestate = NULL;
                gamestate = new OptionsGamestate();
                break;
            default:
                Q_ASSERT(false);
            }
        }
        catch(string &error) {
            LOG("exception creating new gamestate: %s\n", error.c_str());
            this->getScreen()->getMainWindow()->unsetCursor();
            if( gamestate != NULL ) {
                delete gamestate;
                gamestate = NULL;
            }
            delete message;
            stringstream message;
            message << "Failed to load game data:\n" << error;
            game_g->showErrorWindow(message.str().c_str());
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

void Game::showErrorWindow(const char *message) {
    this->getScreen()->enableUpdateTimer(false);
    QMessageBox::critical(this->getScreen()->getMainWindow(), "Error", message);
    this->getScreen()->enableUpdateTimer(true);
}

void Game::showInfoWindow(const char *title, const char *message) {
    this->getScreen()->enableUpdateTimer(false);
    QMessageBox::information(this->getScreen()->getMainWindow(), title, message);
    this->getScreen()->enableUpdateTimer(true);
}

bool Game::askQuestionWindow(const char *title, const char *message) {
    LOG("Game::askQuestionWindow: %s\n", message);
    //this->getScreen()->getMainWindow()->blockSignals(true);
    this->getScreen()->enableUpdateTimer(false);
    //int res = QMessageBox::question(this->getScreen()->getMainWindow(), title, message, QMessageBox::Yes, QMessageBox::No);
    int res = QMessageBox::question(this->getScreen()->getMainWindow(), title, message, QMessageBox::Yes, QMessageBox::No);
    //this->getScreen()->getMainWindow()->blockSignals(false);
    this->getScreen()->enableUpdateTimer(true);
    LOG("    answer is %d\n", res);
    return res == QMessageBox::Yes;
}
