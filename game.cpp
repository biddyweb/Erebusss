#include "game.h"
#include "optionsgamestate.h"
#include "playinggamestate.h"
#include "qt_screen.h"
#include "qt_utils.h"
#include "logiface.h"

#include <ctime>

#include <QWebFrame>
#include <QXmlStreamReader>

#ifdef _DEBUG
#include <cassert>
#endif

Game *game_g = NULL;

const QString sound_enabled_key_c = "sound_enabled";
const int default_sound_enabled_c = true;

void WebViewEventFilter::setWebView(QWebView *webView) {
    qDebug("setWebView");
    this->webView = webView;
    this->webView->installEventFilter(this);
}

bool WebViewEventFilter::eventFilter(QObject *obj, QEvent *event) {
    switch( event->type() ) {
        case QEvent::MouseButtonPress:
            {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                if( mouseEvent->button() == Qt::LeftButton ) {
                    //filterMouseMove = true;
                    // disallow selection - but we need to allow click and drag for the scroll bars!
                    if( webView != NULL ) {
                        QRect vertRect = webView->page()->mainFrame()->scrollBarGeometry(Qt::Vertical);
                        QRect horizRect = webView->page()->mainFrame()->scrollBarGeometry(Qt::Horizontal);
                        int scrollbar_x = vertRect.left();
                        int scrollbar_y = horizRect.top();
                        if( ( vertRect.isEmpty() || mouseEvent->x() <= scrollbar_x ) && ( horizRect.isEmpty() || mouseEvent->y() <= scrollbar_y ) ) {
                            // disallow mouse dragging
                            filterMouseMove = true;
                            orig_mouse_x = saved_mouse_x = mouseEvent->globalX();
                            orig_mouse_y = saved_mouse_y = mouseEvent->globalY();
                        }
                    }
                }
                break;
            }
        case QEvent::MouseButtonRelease:
            {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                if( mouseEvent->button() == Qt::LeftButton ) {
                    filterMouseMove = false;
                    // if we've clicked and dragged, we don't want to allow clicking links
                    int mouse_x = mouseEvent->globalX();
                    int mouse_y = mouseEvent->globalY();
                    int diff_x = mouse_x - orig_mouse_x;
                    int diff_y = mouse_y - orig_mouse_y;
                    int dist2 = diff_x*diff_x + diff_y*diff_y;
                    const int tolerance = 16;
                    qDebug("drag %d, %d : dist2: %d", diff_x, diff_y, dist2);
                    if( dist2 > tolerance*tolerance ) {
                        // need to allow some tolerance, otherwise hard to click on a touchscreen device!
                        return true;
                    }
                }
               break;
            }
        case QEvent::MouseMove:
            {
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
                    if( webView != NULL ) {
                        // support for swype scrolling
                        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                        int new_mouse_x = mouseEvent->globalX();
                        int new_mouse_y = mouseEvent->globalY();
                        webView->page()->mainFrame()->scroll(saved_mouse_x - new_mouse_x, saved_mouse_y - new_mouse_y);
                        saved_mouse_x = new_mouse_x;
                        saved_mouse_y = new_mouse_y;
                    }
                    return true;
                }
            }
            break;
        default:
            break;
    }
    return false;
}

AnimationSet::AnimationSet(AnimationType animation_type, size_t n_frames, vector<QPixmap> pixmaps) : animation_type(animation_type), n_frames(n_frames), pixmaps(pixmaps) {
    if( pixmaps.size() != N_DIRECTIONS * n_frames ) {
        LOG("AnimationSet error: pixmaps size %d, n_frames %d, N_DIRECTIONS %d\n", pixmaps.size(), n_frames, N_DIRECTIONS);
        throw string("AnimationSet has incorrect pixmaps size");
    }
}

AnimationSet::~AnimationSet() {
    //qDebug("AnimationSet::~AnimationSet(): animation type %d, n_frames = %d", this->animation_type, this->n_frames);
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

AnimationSet *AnimationSet::create(const QPixmap &image, AnimationType animation_type, int width, int height, int x_offset, size_t n_frames) {
    vector<QPixmap> frames;
    for(int i=0;i<N_DIRECTIONS;i++) {
        for(size_t j=0;j<n_frames;j++) {
            frames.push_back( image.copy(width*(x_offset+j), height*i, width, height));
        }
    }
    AnimationSet *animation_set = new AnimationSet(animation_type, n_frames, frames);
    return animation_set;
}

AnimationLayer::~AnimationLayer() {
    for(map<string, const AnimationSet *>::iterator iter = this->animation_sets.begin(); iter != this->animation_sets.end(); ++iter) {
        const AnimationSet *animation_set = iter->second;
        delete animation_set;
    }
}

AnimationLayer *AnimationLayer::create(const QPixmap &image, const vector<AnimationLayerDefinition> &animation_layer_definitions) {
    if( image.height() % N_DIRECTIONS != 0 ) {
        throw string("image height is not multiple of 8");
    }
    AnimationLayer *layer = new AnimationLayer(image.height() / N_DIRECTIONS);
    /*LOG("AnimationLayer::create: %s\n", filename);
    QPixmap image = game_g->loadImage(filename);*/
    qDebug("    loaded image");
    for(vector<AnimationLayerDefinition>::const_iterator iter = animation_layer_definitions.begin(); iter != animation_layer_definitions.end(); ++iter) {
        const AnimationLayerDefinition animation_layer_definition = *iter;
        AnimationSet *animation_set = AnimationSet::create(image, animation_layer_definition.animation_type, layer->width, layer->height, animation_layer_definition.position, animation_layer_definition.n_frames);
        layer->addAnimationSet(animation_layer_definition.name, animation_set);
    }
    qDebug("    done");
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
    float width = static_cast<float>(this->getWidth());
    float height = static_cast<float>(this->getHeight());
    return QRectF(0.0f, 0.0f, width, height);
}

void AnimatedObject::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {

    //qDebug("paint");
    /*int ms_per_frame = 100;
    int time_elapsed_ms = game_g->getScreen()->getElapsedMS() - animation_time_start_ms;
    int c_frame = ( time_elapsed_ms / ms_per_frame );*/

    //painter->fillRect(0, 0, 128, 128, Qt::SolidPattern); // test
    if( this->is_static_image ) {
        painter->drawPixmap(0, 0, this->static_image);
    }
    else {
        for(vector<const AnimationSet *>::const_iterator iter = c_animation_sets.begin(); iter != c_animation_sets.end(); ++iter) {
            const AnimationSet *c_animation_set = *iter;
            const QPixmap &pixmap = c_animation_set->getFrame(c_direction, c_frame);
            painter->drawPixmap(0, 0, pixmap);
        }
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
    this->clearStaticImage();
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

int AnimatedObject::getWidth() const {
    if( this->is_static_image ) {
        return this->static_image.width();
    }
    else {
        ASSERT_LOGGER( this->animation_layers.size() > 0 );
        const AnimationLayer *animation_layer = this->animation_layers.at(0);
        return animation_layer->getWidth();
    }
}

int AnimatedObject::getHeight() const {
    if( this->is_static_image ) {
        return this->static_image.height();
    }
    else {
        ASSERT_LOGGER( this->animation_layers.size() > 0 );
        const AnimationLayer *animation_layer = this->animation_layers.at(0);
        return animation_layer->getHeight();
    }
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

Game::Game() : settings(NULL), style(NULL), webViewEventFilter(NULL), gamestate(NULL), screen(NULL), sound_enabled(default_sound_enabled_c) {
    game_g = this;

    QCoreApplication::setApplicationName("erebus");
    settings = new QSettings("Mark Harman", "erebus", this);

    style = new QWindowsStyle(); // needed to get the textured buttons (which doesn't work with Windows XP, Symbian or Android styles)

    webViewEventFilter = new WebViewEventFilter(this);

    bool ok = true;

    int sound_enabled_i = settings->value(sound_enabled_key_c, default_sound_enabled_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings sound_enabled not ok, set to default");
        this->sound_enabled = default_sound_enabled_c;
    }
    else {
        this->sound_enabled = sound_enabled_i != 0;
    }

    /*int difficulty_i = settings->value(difficulty_key_c, default_difficulty_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings difficulty not ok, set to default");
        this->difficulty = default_difficulty_c;
    }
    else {
        this->difficulty = (Difficulty)difficulty_i;
    }*/
    /*string difficulty_s = settings->value(difficulty_key_c, getDifficultyString(default_difficulty_c)).toString().toStdString();
    bool done_difficulty = false;
    for(int i=0;i<N_DIFFICULTY && !done_difficulty;i++) {
        Difficulty test_difficulty = (Difficulty)i;
        if( difficulty_s = getDifficultyString(test_difficulty) ) {
            done_difficulty = true;
            this->difficulty = test_difficulty;
        }
    }
    if( !done_difficulty ) {
        qDebug("settings difficulty not ok, set to default");
        this->difficulty = default_difficulty_c;
    }*/

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

Game::~Game() {
    if( style != NULL ) {
        delete style;
    }
    game_g = NULL;
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

/*#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR)
    // crashes on Android?!
    // and doesn't work on Symbian
    // see Game::initButton()
#else*/
    unsigned char filter_max[] = {220, 220, 220};
    unsigned char filter_min[] = {120, 120, 120};
    QPixmap gui_pixmap_buttons = createNoise(256, 256, 16.0f, 16.0f, filter_max, filter_min, NOISEMODE_PERLIN, 4);
    this->gui_brush_buttons.setTexture(gui_pixmap_buttons);
    this->gui_palette.setBrush(QPalette::Button, gui_brush_buttons);
    //this->gui_palette.setBrush(QPalette::WindowText, QBrush(Qt::red)); // needed for Symbian at least
    //this->gui_palette.setBrush(QPalette::WindowText, gui_brush_buttons);
//#endif

    gamestate = new OptionsGamestate();

    screen->runMainLoop();

    delete gamestate;
    gamestate = NULL;
    {
        // hack to prevent the MyApplication::event() from calling screen functions when deactivating
        Screen *local_screen = screen;
        screen = NULL;
        delete local_screen;
    }
}

void Game::initButton(QPushButton *button) const {
/*#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR)
    // crashes on Android?!
    // and doesn't work on Symbian
    // if we change this, remember to change it where we initialise gui_palette, in Game::run().
#else*/
    button->setStyle(style);
    button->setAutoFillBackground(true);
    button->setPalette(this->gui_palette);
//#endif
    //button->setStyleSheet("QPushButton { color : red; }");
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
                StartGameMessage *start_message = static_cast<StartGameMessage *>(message);
                playing_gamestate->setDifficulty(start_message->getDifficulty());
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

/*void Game::log(const char *text, ...) {
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
}*/

void Game::log(const char *text) {
    FILE *logfile = fopen(logfilename.c_str(), "at+");
    if( logfile != NULL )
        fprintf(logfile, text);
    qDebug("%s", text);
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
Sound *Game::loadSound(string filename) const {
    Phonon::MediaObject *mediaObject = new Phonon::MediaObject(qApp);
    //connect(sound, SIGNAL(stateChanged(Phonon::State,Phonon::State)), this, SLOT(mediaStateChanged(Phonon::State,Phonon::State)));
    mediaObject->setCurrentSource(Phonon::MediaSource(filename.c_str()));
    Phonon::AudioOutput *audioOutput = new Phonon::AudioOutput(Phonon::GameCategory, qApp);
    Phonon::Path audioPath = Phonon::createPath(mediaObject, audioOutput);
    Sound *sound = new Sound(mediaObject, audioOutput);
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

void Game::setSoundEnabled(bool sound_enabled) {
    this->sound_enabled = sound_enabled;
    this->settings->setValue(sound_enabled_key_c, sound_enabled ? 1 : 0);
}

string Game::getDifficultyString(Difficulty difficulty) {
    if( difficulty == DIFFICULTY_EASY ) {
        return "Easy";
    }
    else if( difficulty == DIFFICULTY_MEDIUM ) {
        return "Medium";
    }
    else if( difficulty == DIFFICULTY_HARD ) {
        return "Hard";
    }
    ASSERT_LOGGER(false);
    return "";
}

void Game::showErrorDialog(const string &message) {
    LOG("Game::showErrorDialog: %s\n", message.c_str());
    this->getScreen()->enableUpdateTimer(false);
    QMessageBox::critical(this->getScreen()->getMainWindow(), "Error", message.c_str());
    this->getScreen()->enableUpdateTimer(true);
}

/*void Game::showInfoDialog(const string &title, const string &message) {
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
}*/
