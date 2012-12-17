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
const QString lighting_enabled_key_c = "lighting_enabled";
const int default_lighting_enabled_c = true;

void WebViewEventFilter::setWebView(QWebView *webView) {
    qDebug("setWebView");
    this->webView = webView;
    this->webView->installEventFilter(this);
}

// returns true to filter the event
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
                        // support for swipe scrolling
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
        case QEvent::MouseButtonDblClick:
            return true;
            break;
        default:
            break;
    }
    return false;
}

AnimationSet::AnimationSet(AnimationType animation_type, unsigned int n_dimensions, size_t n_frames, vector<QPixmap> pixmaps) : animation_type(animation_type), n_dimensions(n_dimensions), n_frames(n_frames), pixmaps(pixmaps) {
    if( pixmaps.size() != n_dimensions * n_frames ) {
        LOG("AnimationSet error: pixmaps size %d, n_frames %d, n_dimensions %d\n", pixmaps.size(), n_frames, n_dimensions);
        throw string("AnimationSet has incorrect pixmaps size");
    }
}

AnimationSet::~AnimationSet() {
    //qDebug("AnimationSet::~AnimationSet(): animation type %d, n_frames = %d", this->animation_type, this->n_frames);
}

const QPixmap &AnimationSet::getFrame(unsigned int c_dimension, size_t c_frame) const {
    //qDebug("animation type: %d", this->animation_type);
    //LOG("%d : type %d, frame %d\n", this, this->animation_type, c_frame);
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
    //LOG("    >>> %d\n", c_frame);

    //qDebug("get frame %d", c_frame);
    return this->pixmaps[c_dimension*n_frames + c_frame];
}

AnimationSet *AnimationSet::create(const QPixmap &image, AnimationType animation_type, int stride_x, int stride_y, int x_offset, unsigned int n_dimensions, size_t n_frames, int icon_off_x, int icon_off_y, int icon_width, int icon_height) {
    if( icon_width == 0 )
        icon_width = stride_x;
    if( icon_height == 0 )
        icon_height = stride_y;
    //qDebug("### %d x %d\n", icon_width, icon_height);
    vector<QPixmap> frames;
    for(unsigned int i=0;i<n_dimensions;i++) {
        for(size_t j=0;j<n_frames;j++) {
            frames.push_back( image.copy(stride_x*(x_offset+j) + icon_off_x, stride_y*i + icon_off_y, icon_width, icon_height));
        }
    }
    AnimationSet *animation_set = new AnimationSet(animation_type, n_dimensions, n_frames, frames);
    return animation_set;
}

AnimationLayer::~AnimationLayer() {
    for(map<string, const AnimationSet *>::iterator iter = this->animation_sets.begin(); iter != this->animation_sets.end(); ++iter) {
        const AnimationSet *animation_set = iter->second;
        delete animation_set;
    }
}

AnimationLayer *AnimationLayer::create(const QPixmap &image, const vector<AnimationLayerDefinition> &animation_layer_definitions, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions) {
    if( image.height() % n_dimensions != 0 ) {
        throw string("image height is not multiple of n_dimensions");
    }
    //int stride_x = expected_stride_x;
    //int stride_y = image.height() / n_dimensions;
    //if( expected_stride_y != stride_y ) {
    if( expected_total_width != image.width() ) {
        //float ratio = ((float)stride_y)/(float)expected_stride_y;
        float ratio = ((float)image.width())/(float)expected_total_width;
        width *= ratio;
        height *= ratio;
        off_x *= ratio;
        off_y *= ratio;
        stride_x *= ratio;
        stride_y *= ratio;
        qDebug("ratio: %f\n", ratio);
    }
    AnimationLayer *layer = new AnimationLayer(width, height);
    /*LOG("AnimationLayer::create: %s\n", filename);
    QPixmap image = game_g->loadImage(filename);*/
    qDebug("    loaded image");
    for(vector<AnimationLayerDefinition>::const_iterator iter = animation_layer_definitions.begin(); iter != animation_layer_definitions.end(); ++iter) {
        const AnimationLayerDefinition animation_layer_definition = *iter;
        AnimationSet *animation_set = AnimationSet::create(image, animation_layer_definition.animation_type, stride_x, stride_y, animation_layer_definition.position, n_dimensions, animation_layer_definition.n_frames, off_x, off_y, width, height);
        layer->addAnimationSet(animation_layer_definition.name, animation_set);
    }
    qDebug("    done");
    return layer;
}

AnimationLayer *AnimationLayer::create(const string &filename, const vector<AnimationLayerDefinition> &animation_layer_definitions, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions) {
    QPixmap image = game_g->loadImage(filename.c_str());
    //return create(image, animation_layer_definitions, off_x, off_y, width, height, expected_stride_x, expected_stride_y, n_dimensions);
    return create(image, animation_layer_definitions, off_x, off_y, width, height,stride_x, stride_y, expected_total_width, n_dimensions);
}

AnimatedObject::AnimatedObject() : /*animation_layer(NULL), c_animation_set(NULL),*/
    set_c_animation_name(false), c_dimension(0), c_frame(0), animation_time_start_ms(0), bounce(false)
{
}

AnimatedObject::~AnimatedObject() {
}

void AnimatedObject::advance(int phase) {
    //qDebug("AnimatedObject::advance() phase %d", phase);
    if( phase == 1 ) {
        const int ms_per_frame = 100;
        //const int ms_per_frame = 1000;
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
    //painter->fillRect(0, 0, this->getWidth(), this->getHeight(), Qt::SolidPattern); // test
    int off_y = 0;
    if( this->bounce ) {
        const int scale = 4;
        int time_elapsed_ms = game_g->getScreen()->getGameTimeTotalMS() - animation_time_start_ms;
        off_y = (int)(sin( (((float)time_elapsed_ms)*2.0*M_PI)/1000.0f ) * scale);
        off_y -= scale/2;
    }

    if( this->is_static_image ) {
        painter->drawPixmap(0, off_y, this->static_image);
    }
    else {
        for(vector<const AnimationSet *>::const_iterator iter = c_animation_sets.begin(); iter != c_animation_sets.end(); ++iter) {
            const AnimationSet *c_animation_set = *iter;
            const QPixmap &pixmap = c_animation_set->getFrame(c_dimension, c_frame);
            painter->drawPixmap(0, off_y, pixmap);
        }
    }
}

void AnimatedObject::addAnimationLayer(AnimationLayer *animation_layer) {
    if( animation_layer == NULL ) {
        throw string("AnimatedObject::addAnimationLayer received NULL animated_layer");
    }
    this->animation_layers.push_back(animation_layer);
    const AnimationSet *c_animation_set = animation_layer->getAnimationSet(this->set_c_animation_name ? this->c_animation_name : "");
    this->c_animation_sets.push_back(c_animation_set);
    //this->setFrame(0);
    this->update();
}

void AnimatedObject::clearAnimationLayers() {
    this->animation_layers.clear();
    this->c_animation_sets.clear();
    this->clearStaticImage();
}

void AnimatedObject::setAnimationSet(const string &name, bool force_restart) {
    bool is_new = false;
    if( !this->set_c_animation_name || this->c_animation_name != name ) {
        is_new = true;
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
    /*else {
        // useful as a warning, in case we didn't intend to do this
        qDebug("reset animation set: %s", name.c_str());
    }*/

    if( is_new || force_restart ) {
        animation_time_start_ms = game_g->getScreen()->getGameTimeTotalMS();
        //this->setFrame(0);
        this->c_frame = 0;
        this->update();

        this->set_c_animation_name = true;
        this->c_animation_name = name;
    }
}

void AnimatedObject::setDimension(unsigned int c_dimension) {
    if( this->c_dimension != c_dimension ) {
        this->c_dimension = c_dimension;
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

Particle::Particle() : xpos(0.0f), ypos(0.0f), birth_time(0) {
    this->birth_time = game_g->getScreen()->getGameTimeTotalMS();
}

void ParticleSystem::advance(int phase) {
    //qDebug("AnimatedObject::advance() phase %d", phase);
    if( phase == 1 ) {
        this->update();
    }
}

void ParticleSystem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    qDebug("draw %d particles", particles.size());
    for(vector<Particle>::const_iterator iter = particles.begin(); iter != particles.end(); ++iter) {
        painter->drawPixmap(iter->getX() - this->pixmap.width()*0.5f, iter->getY() - this->pixmap.height()*0.5f, this->pixmap);
    }
}

QRectF ParticleSystem::boundingRect() const {
    qDebug("ParticleSystem::boundingRect()");
    /*for(vector<Particle>::const_iterator iter = particles.begin(); iter != particles.end(); ++iter) {
    }*/
    return QRectF(-100, -100, 200, 200);
}

SmokeParticleSystem::SmokeParticleSystem(const QPixmap &pixmap) : ParticleSystem(pixmap),
    birth_rate(0.0f), life_exp(1500), last_emit_time(0) {
    this->last_emit_time = game_g->getScreen()->getGameTimeTotalMS();
}

void SmokeParticleSystem::setBirthRate(float birth_rate) {
    this->birth_rate = birth_rate;
}

void SmokeParticleSystem::update() {
    // expire old particles
    int time_now = game_g->getScreen()->getGameTimeTotalMS();
    for(int i=particles.size()-1;i>=0;i--) { // count backwards in case of deletion
            if( time_now >= particles.at(i).getBirthTime() + life_exp ) {
                    // for performance, we reorder and reduce the length by 1 (as the order of the particles shouldn't matter)
                    particles[i] = particles[particles.size()-1];
                    particles.resize(particles.size()-1);
            }
    }

    int real_loop_time = game_g->getScreen()->getGameTimeFrameMS();
    //LOG("%d\n", real_loop_time);
    // update particles
    for(int i=particles.size()-1;i>=0;i--) { // count backwards in case of deletion
            //const float xspeed = 0.01f;
            const float xspeed = 0.015f;
            //const float yspeed = 0.05f;
            const float yspeed = 0.03f;
            float xpos = particles.at(i).getX();
            float ypos = particles.at(i).getY();
            float ydiff = real_loop_time * yspeed;
            ypos -= ydiff;
            float xdiff = real_loop_time * xspeed;
            if( rand() % 2 == 0 ) {
                    xdiff = - xdiff;
            }
            xpos += xdiff;
            /*if( ypos < 0 ) {
                    // kill
                    // for performance, we reorder and reduce the length by 1 (as the order of the particles shouldn't matter)
                    particles[i] = particles[particles.size()-1];
                    particles.resize(particles.size()-1);
                    //LOG("resize to %d\n", particles.size());
            }
            else*/ {
                    particles.at(i).setPos(xpos, ypos);
            }
    }

    // emit new particles
    int accumulated_time = game_g->getScreen()->getGameTimeTotalMS() - this->last_emit_time;
    int new_particles = (int)(this->birth_rate * accumulated_time);
    new_particles = std::min(1, new_particles); // helps make rate more steady
    this->last_emit_time += (int)(1.0f/birth_rate * new_particles);
    if( new_particles > 0 ) {
            //LOG("%d new particles (total will be %d)\n", new_particles, particles.size() + new_particles);
            for(int i=0;i<new_particles;i++) {
                    Particle particle;
                    particles.push_back(particle);
            }
    }
}

ScrollingListWidget::ScrollingListWidget() : QListWidget(), saved_x(0), saved_y(0) {
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    this->setStyleSheet("color: black; background-color: white"); // workaround for Android color bug, also needed for Symbian
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

Game::Game() : settings(NULL), style(NULL), webViewEventFilter(NULL), gamestate(NULL), screen(NULL), sound_enabled(default_sound_enabled_c), lighting_enabled(default_lighting_enabled_c) {
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

    int lighting_enabled_i = settings->value(lighting_enabled_key_c, default_lighting_enabled_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings lighting_enabled not ok, set to default");
        this->lighting_enabled = default_lighting_enabled_c;
    }
    else {
        this->lighting_enabled = lighting_enabled_i != 0;
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
#elif defined(Q_OS_ANDROID)
    // must be before __linux, as Android also defines __linux
    LOG("Platform: Android\n");
#elif __linux
    LOG("Platform: Linux\n");
#elif defined(__APPLE__) && defined(__MACH__)
    LOG("Platform: MacOS X\n");
#elif defined(Q_OS_SYMBIAN)
    LOG("Platform: Symbian\n");
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

    this->createPlayerNames();
}

Game::~Game() {
#ifndef Q_OS_ANDROID
    for(map<string, Sound *>::iterator iter = this->sound_effects.begin(); iter != this->sound_effects.end(); ++iter) {
        Sound *sound = iter->second;
        delete sound;
    }
#endif

    if( style != NULL ) {
        delete style;
    }
    game_g = NULL;
}

void Game::run() {
    screen = new Screen();

    // setup fonts
    MainWindow *window = game_g->getScreen()->getMainWindow();
    {
        QPalette palette = window->palette();
        palette.setColor(QPalette::Window, Qt::black);
        palette.setColor(QPalette::WindowText, Qt::gray);
        window->setPalette(palette);
    }
    QWebSettings *web_settings = QWebSettings::globalSettings();
    int screen_w = QApplication::desktop()->width();
    int screen_h = QApplication::desktop()->height();
    LOG("resolution %d x %d\n", screen_w, screen_h);
    if( mobile_c ) {
        QFont new_font = window->font();
#if defined(Q_OS_ANDROID)
        qDebug("setting up fonts for Android");
        this->font_scene = new_font;
        this->font_small = QFont(new_font);
        this->font_small.setPointSize(font_small.pointSize() + 8);
        this->font_std = QFont(new_font);
        this->font_std.setPointSize(font_std.pointSize() + 10);
        this->font_big = QFont(new_font);
        this->font_big.setPointSize(font_big.pointSize() + 14);

        web_settings->setFontFamily(QWebSettings::StandardFont, font_std.family());
        web_settings->setFontSize(QWebSettings::DefaultFontSize, font_std.pointSize() + 20);
        web_settings->setFontSize(QWebSettings::DefaultFixedFontSize, font_std.pointSize() + 20);
#else
        qDebug("setting up fonts for non-Android mobile");
        if( screen_w == 640 && screen_h == 480 ) {
            // hackery for Nokia E6 - almost all Symbian^1 phones or later are 640x360, but things don't look right with the E6, which is 640x480
            LOG("Nokia E6 font hackery\n");
            this->font_scene = QFont(new_font);
            this->font_scene.setPointSize(font_scene.pointSize() - 2);
        }
        else {
            this->font_scene = new_font;
        }
        this->font_small = QFont(new_font);
        this->font_small.setPointSize(font_small.pointSize() - 2);
        this->font_std = new_font;
        this->font_big = new_font;

        // leave web font as default
#endif
    }
    else {
        qDebug("setting up fonts for non-mobile");
        this->font_scene = QFont("Verdana", 12);
        this->font_small = QFont("Verdana", 12);
        this->font_std = QFont("Verdana", 16);
        this->font_big = QFont("Verdana", 48, QFont::Bold);

        web_settings->setFontFamily(QWebSettings::StandardFont, font_std.family());
        web_settings->setFontSize(QWebSettings::DefaultFontSize, font_std.pointSize());
        web_settings->setFontSize(QWebSettings::DefaultFixedFontSize, font_std.pointSize());
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
    this->gui_palette.setColor(QPalette::ButtonText, Qt::black);
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

enum TestID {
    TEST_PATHFINDING_0 = 0,
    TEST_PATHFINDING_1 = 1,
    TEST_PATHFINDING_2 = 2,
    TEST_PATHFINDING_3 = 3,
    TEST_PATHFINDING_4 = 4,
    TEST_POINTINPOLYGON_0 = 5,
    TEST_POINTINPOLYGON_1 = 6,
    TEST_POINTINPOLYGON_2 = 7,
    TEST_POINTINPOLYGON_3 = 8,
    TEST_POINTINPOLYGON_4 = 9,
    TEST_POINTINPOLYGON_5 = 10,
    TEST_POINTINPOLYGON_6 = 11,
    TEST_POINTINPOLYGON_7 = 12,
    TEST_POINTINPOLYGON_8 = 13,
    TEST_POINTINPOLYGON_9 = 14,
    TEST_POINTINPOLYGON_10 = 15,
    TEST_POINTINPOLYGON_11 = 16,
    N_TESTS = 17
};

/**
  TEST_PATHFINDING_0 - find a path
  TEST_PATHFINDING_1 - find a path
  TEST_PATHFINDING_2 - can't find a path as no route available
  TEST_PATHFINDING_3 - can't find a path, as start isn't in valid floor region
  TEST_PATHFINDING_4 - can't find a path, as destination isn't in valid floor region
  TEST_POINTINPOLYGON_0 - tests that a point is inside a convex polygon (triangle)
  TEST_POINTINPOLYGON_1 - tests that a point is on a convex polygon (triangle)
  TEST_POINTINPOLYGON_2 - tests that a point is not inside a convex polygon (triangle)
  TEST_POINTINPOLYGON_3 - tests that a point is inside a convex polygon (quadrilateral)
  TEST_POINTINPOLYGON_4 - tests that a point is on a convex polygon (quadrilateral)
  TEST_POINTINPOLYGON_5 - tests that a point is not inside a convex polygon (quadrilateral)
  TEST_POINTINPOLYGON_6 - tests that a point is inside a concave polygon (quadrilateral)
  TEST_POINTINPOLYGON_7 - tests that a point is on a concave polygon (quadrilateral)
  TEST_POINTINPOLYGON_8 - tests that a point is not inside a concave polygon (quadrilateral)
  TEST_POINTINPOLYGON_9 - tests that a point is inside a concave polygon (L-shape)
  TEST_POINTINPOLYGON_10 - tests that a point is on a concave polygon (L-shape)
  TEST_POINTINPOLYGON_11 - tests that a point is not inside a concave polygon (L-shape)
  */

void Game::runTest(const string &filename, int test_id) {
    LOG(">>> Run Test: %d\n", test_id);
    ASSERT_LOGGER(test_id >= 0);
    ASSERT_LOGGER(test_id < N_TESTS);

    FILE *testfile = fopen(filename.c_str(), "at+");
    if( testfile == NULL ) {
        LOG("### FAILED to open/create %s\n", filename.c_str());
        return;
    }
    fprintf(testfile, "%d,", test_id);

    bool ok = true;

    try {
        if( test_id == TEST_PATHFINDING_0 || test_id == TEST_PATHFINDING_1 || test_id == TEST_PATHFINDING_2 || test_id == TEST_PATHFINDING_3 || test_id == TEST_PATHFINDING_4 ) {
            Location location("");

            {
                FloorRegion *floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
            }
            if( test_id != TEST_PATHFINDING_2 )
            {
                FloorRegion *floor_region = FloorRegion::createRectangle(10.0f, 1.0f, 4.0f, 3.0f);
                location.addFloorRegion(floor_region);
            }
            {
                FloorRegion *floor_region = FloorRegion::createRectangle(5.0f, 3.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
            }

            location.createBoundariesForRegions();
            location.createBoundariesForScenery();
            location.calculateDistanceGraph();

            Vector2D src, dest;
            int expected_points = 0;
            if( test_id == TEST_PATHFINDING_0 || test_id == TEST_PATHFINDING_2 ) {
                src = Vector2D(1.0f, 1.0f);
                dest = Vector2D(13.0f, 2.0f);
                expected_points = test_id == TEST_PATHFINDING_0 ? 3 : 0;
            }
            else if( test_id == TEST_PATHFINDING_1 ) {
                src = Vector2D(4.0f, 4.0f);
                dest = Vector2D(13.0f, 2.0f);
                expected_points = 3;
            }
            else if( test_id == TEST_PATHFINDING_3 ) {
                src = Vector2D(1.0f, 5.0f);
                dest = Vector2D(13.0f, 2.0f);
                expected_points = 0;
            }
            else if( test_id == TEST_PATHFINDING_4 ) {
                src = Vector2D(1.0f, 1.0f);
                dest = Vector2D(15.0f, 2.0f);
                expected_points = 0;
            }
            else {
                throw string("missing case for this test_id");
            }
            vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);

            LOG("path has %d points\n", path.size());
            for(size_t i=0;i<path.size();i++) {
                Vector2D point = path.at(i);
                LOG("    %d : %f, %f\n", i, point.x, point.y);
            }

            if( path.size() != expected_points ) {
                throw string("Unexpected number of path points");
            }
            else if( path.size() > 0 && path.at( path.size()-1 ) != dest ) {
                throw string("Unexpected end of path");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_0 || test_id == TEST_POINTINPOLYGON_1 || test_id == TEST_POINTINPOLYGON_2 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(-1.0f, -1.0f));
            poly.addPoint(Vector2D(0.5f, -1.1f));
            poly.addPoint(Vector2D(-0.1f, 0.8f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_0 ) {
                test_pt.set(0.0f, -1.05f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_1 ) {
                test_pt.set(0.5f, -1.1f);
                exp_inside = true;
            }
            else {
                test_pt.set(0.0f, -1.1f);
                exp_inside = false;
            }
            bool inside = poly.pointInside(test_pt);
            if( inside != exp_inside ) {
                throw string("failed point inside polygon test");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_3 || test_id == TEST_POINTINPOLYGON_4 || test_id == TEST_POINTINPOLYGON_5 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(-1.0f, -1.0f));
            poly.addPoint(Vector2D(0.5f, -1.1f));
            poly.addPoint(Vector2D(0.5f, 0.8f));
            poly.addPoint(Vector2D(-1.1f, 0.8f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_3 ) {
                test_pt.set(0.49f, 0.75f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_4 ) {
                test_pt.set(0.5f, 0.1f);
                exp_inside = true;
            }
            else {
                test_pt.set(0.51f, 0.75f);
                exp_inside = false;
            }
            bool inside = poly.pointInside(test_pt);
            if( inside != exp_inside ) {
                throw string("failed point inside polygon test");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_6 || test_id == TEST_POINTINPOLYGON_7 || test_id == TEST_POINTINPOLYGON_8 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(-1.0f, -1.0f));
            poly.addPoint(Vector2D(1.0f, -1.0f));
            poly.addPoint(Vector2D(1.0f, 1.0f));
            poly.addPoint(Vector2D(0.9f, -0.9f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_6 ) {
                test_pt.set(0.91f, -0.91f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_7 ) {
                test_pt.set(0.95f, 0.05f);
                exp_inside = true;
            }
            else {
                test_pt.set(0.8f, -0.89f);
                exp_inside = false;
            }
            bool inside = poly.pointInside(test_pt);
            if( inside != exp_inside ) {
                throw string("failed point inside polygon test");
            }
        }
        else if( test_id == TEST_POINTINPOLYGON_9 || test_id == TEST_POINTINPOLYGON_10 || test_id == TEST_POINTINPOLYGON_11 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(0.0f, 0.0f));
            poly.addPoint(Vector2D(0.0f, 3.0f));
            poly.addPoint(Vector2D(2.0f, 3.0f));
            poly.addPoint(Vector2D(2.0f, 2.0f));
            poly.addPoint(Vector2D(1.0f, 2.0f));
            poly.addPoint(Vector2D(1.0f, 0.0f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_9 ) {
                test_pt.set(0.5f, 2.5f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_10 ) {
                test_pt.set(0.5f, 0.0f);
                exp_inside = true;
            }
            else {
                test_pt.set(1.1f, 0.0f);
                exp_inside = false;
            }
            bool inside = poly.pointInside(test_pt);
            if( inside != exp_inside ) {
                throw string("failed point inside polygon test");
            }
        }
        else {
            throw string("unknown test");
        }

        fprintf(testfile, "PASSED\n");
    }
    catch(const string &str) {
        LOG("ERROR: %s\n", str.c_str());
        fprintf(testfile, "FAILED,%s\n", str.c_str());
        ok = false;
    }

    if( ok ) {
        LOG("<<< TEST %d PASSED\n", test_id);
    }
    else {
        LOG("<<< TEST %d FAILED", test_id);
    }

    fclose(testfile);
}

void Game::runTests() {
    string filename = "test_results.csv";
    {
        FILE *testfile = fopen(filename.c_str(), "wt+");
        if( testfile == NULL ) {
            LOG("### FAILED to open/create %s\n", filename.c_str());
            return;
        }
        time_t time_val;
        time(&time_val);
        fprintf(testfile, "%s\n", ctime(&time_val));
        fprintf(testfile, "TEST,RESULT\n");
        fclose(testfile);
    }

    for(int i=0;i<N_TESTS;i++) {
        runTest(filename, i);
    }

}

void Game::initButton(QPushButton *button) const {
    MainWindow *window = this->screen->getMainWindow();
    button->setFont(window->font()); // needed for Android at least
    button->setStyle(style);
    button->setAutoFillBackground(true);
    button->setPalette(this->gui_palette);
}

void Game::update() {
    while( !message_queue.empty() ) {
        GameMessage *message = message_queue.front();
        message_queue.pop();
        try {
            switch( message->getGameMessageType() ) {
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING:
            {
                StartGameMessage *start_message = static_cast<StartGameMessage *>(message);
                delete gamestate;
                gamestate = NULL;
                PlayingGamestate *playing_gamestate = new PlayingGamestate(false, start_message->getPlayerType());
                gamestate = playing_gamestate;
                playing_gamestate->setDifficulty(start_message->getDifficulty());
                const QuestInfo &c_quest_info = playing_gamestate->getCQuestInfo();
                string qt_filename = DEPLOYMENT_PATH + c_quest_info.getFilename();
                playing_gamestate->loadQuest(qt_filename, false);
                this->getScreen()->getMainWindow()->unsetCursor();
                break;
            }
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING_LOAD:
            {
                delete gamestate;
                gamestate = NULL;
                PlayingGamestate *playing_gamestate = new PlayingGamestate(true, 0);
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

QPixmap Game::loadImage(const string &filename, bool clip, int xpos, int ypos, int width, int height, int expected_width) const {
    // need to use QImageReader - QPixmap::load doesn't work on large images on Symbian!
    QImageReader reader(filename.c_str());
    if( clip ) {
        //qDebug("clipping");
        int actual_width = reader.size().width();
        if( actual_width != expected_width ) {
            float ratio = ((float)actual_width)/(float)expected_width;
            xpos *= ratio;
            ypos *= ratio;
            width *= ratio;
            height *= ratio;
        }
        if( xpos > 0 || ypos > 0 || width < actual_width || height < reader.size().height() ) {
            reader.setClipRect(QRect(xpos, ypos, width, height));
        }
    }
    /*QImage image = reader.read();
    if( image.isNull() ) {
        LOG("failed to read image: %s\n", filename.c_str());
        LOG("error: %d\n", reader.error());
        LOG("error string: %s\n", reader.errorString().toStdString().c_str());
        stringstream error;
        error << "Failed to load image: " << filename;
        throw error.str();
    }
    QPixmap pixmap = QPixmap::fromImage(image);*/
    QPixmap pixmap = QPixmap::fromImageReader(&reader);
    if( pixmap.isNull() ) {
        LOG("failed to convert image to pixmap: %s\n", filename.c_str());
        LOG("image reader error: %d\n", reader.error());
        LOG("image reader error string: %s\n", reader.errorString().toStdString().c_str());
        throw string("Failed to convert image to pixmap");
    }
    return pixmap;
}

void Game::loadSound(const string &id, const string &filename) {
#ifndef Q_OS_ANDROID
    Phonon::MediaObject *mediaObject = new Phonon::MediaObject(qApp);
    //connect(sound, SIGNAL(stateChanged(Phonon::State,Phonon::State)), this, SLOT(mediaStateChanged(Phonon::State,Phonon::State)));
    mediaObject->setCurrentSource(Phonon::MediaSource(filename.c_str()));
    Phonon::AudioOutput *audioOutput = new Phonon::AudioOutput(Phonon::GameCategory, qApp);
    Phonon::Path audioPath = Phonon::createPath(mediaObject, audioOutput);
    Sound *sound = new Sound(mediaObject, audioOutput);
    this->sound_effects[id] = sound;
#else

    Sound *sound = androidAudio.loadSound(filename.c_str());
    this->sound_effects[id] = sound;

#endif
}

#ifndef Q_OS_ANDROID
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

void Game::playSound(const string &sound_effect) {
    qDebug("play sound: %s\n", sound_effect.c_str());
    if( game_g->isSoundEnabled() ) {
        Sound *sound = this->sound_effects[sound_effect];
        if( sound != NULL ) {
#ifndef Q_OS_ANDROID
            if( sound->state() == Phonon::PlayingState ) {
                qDebug("    already playing");
            }
            else {
                //sound->stop();
                sound->seek(0);
                sound->play();
            }
#else
            androidAudio.playSound(sound);
#endif
        }
    }
}

void Game::pauseSound(const string &sound_effect) {
#ifndef Q_OS_ANDROID
    qDebug("pause sound: %s\n", sound_effect.c_str());
    // n.b., need to allow pausing even if sound not enabled (so we can disable sound, then pause music)
    Sound *sound = this->sound_effects[sound_effect];
    ASSERT_LOGGER(sound != NULL);
    if( sound != NULL ) {
        sound->pause();
    }
#endif
    // not yet supported on Android
}

void Game::freeSound(const string &sound_effect) {
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        this->sound_effects.erase( this->sound_effects.find(sound_effect) );
        delete sound;
    }
}

void Game::setSoundEnabled(bool sound_enabled) {
    this->sound_enabled = sound_enabled;
    this->settings->setValue(sound_enabled_key_c, sound_enabled ? 1 : 0);
}

void Game::setLightingEnabled(bool lighting_enabled) {
    this->lighting_enabled = lighting_enabled;
    this->settings->setValue(lighting_enabled_key_c, lighting_enabled ? 1 : 0);
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

void Game::createPlayerNames() {
    this->player_types.push_back("Barbarian");
    this->player_types.push_back("Elf");
    this->player_types.push_back("Halfling");
    this->player_types.push_back("Ranger");
    this->player_types.push_back("Warrior");
}

Character *Game::createPlayer(size_t i) const {
    ASSERT_LOGGER(i < this->getNPlayerTypes() );
    Character *character = new Character(player_types.at(i), "", false);
    if( i == 0 ) {
        // barbarian
        character->setProfile(7, 6, 8, 1, 5, 5, 8, 1.8f);
        character->initialiseHealth(65);
        character->setBiography("");
    }
    else if( i == 1 ) {
        // elf
        character->setProfile(6, 8, 6, 1, 8, 7, 8, 2.25f);
        character->initialiseHealth(50);
        character->setBiography("");
    }
    else if( i == 2 ) {
        // halfling
        character->setProfile(6, 7, 5, 1, 7, 9, 7, 1.8f);
        character->initialiseHealth(40);
        character->setBiography("");
    }
    else if( i == 3 ) {
        // ranger
        character->setProfile(6, 8, 7, 1, 7, 8, 6, 2.2f);
        character->initialiseHealth(50);
        character->setBiography("");
    }
    else if( i == 4 ) {
        // warrior
        character->setProfile(7, 7, 7, 1, 6, 7, 7, 2.0f);
        character->initialiseHealth(60);
        character->setBiography("");
    }
    else {
        ASSERT_LOGGER(false);
    }

    //character->initialiseHealth(600); // CHEAT
    character->addGold( rollDice(2, 6, 10) );
    //character->addGold( 333 ); // CHEAT, simulate start of quest 2
    //character->setXP(68); // CHEAT, simulate start of quest 2
    //character->addGold( 1000 ); // CHEAT

    return character;
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

void Game::activate(bool active) {
    this->getScreen()->enableUpdateTimer(active);
    if( !active ) {
        this->getScreen()->setPaused(true); // automatically pause when application goes inactive
    }
    if( this->gamestate != NULL ) {
        this->gamestate->activate(active);
    }
}
