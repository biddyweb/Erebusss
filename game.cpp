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
#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
const int default_lighting_enabled_c = false; // lighting effects can be a bit too slow on mobile platforms
#else
const int default_lighting_enabled_c = true;
#endif

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
    //qDebug("%d : type %d, frame %d / %d\n", this, this->animation_type, c_frame, this->n_dimensions);
    c_dimension = c_dimension % this->n_dimensions;
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
    //LOG("    >>> %d\n", c_frame);

    //qDebug("get frame %d", c_frame);
    return this->pixmaps[c_dimension*n_frames + c_frame];
}

AnimationSet *AnimationSet::create(const QPixmap &image, AnimationType animation_type, int stride_x, int stride_y, int x_offset, unsigned int n_dimensions, size_t n_frames, int icon_off_x, int icon_off_y, int icon_width, int icon_height) {
    //qDebug("### %d x %d\n", icon_width, icon_height);
    vector<QPixmap> frames;
    for(unsigned int i=0;i<n_dimensions;i++) {
        for(size_t j=0;j<n_frames;j++) {
            int xpos = stride_x*(x_offset+j) + icon_off_x;
            int ypos = stride_y*i + icon_off_y;
            frames.push_back( image.copy(xpos, ypos, icon_width, icon_height));
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

AnimationLayer *AnimationLayer::create(const QPixmap &image, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions) {
    if( image.height() % n_dimensions != 0 ) {
        throw string("image height is not multiple of n_dimensions");
    }
    if( !clip ) {
        bool static_image = true;
        for(vector<AnimationLayerDefinition>::const_iterator iter = animation_layer_definitions.begin(); iter != animation_layer_definitions.end() && static_image; ++iter) {
            const AnimationLayerDefinition animation_layer_definition = *iter;
            if( animation_layer_definition.position > 0 || animation_layer_definition.n_frames > 1 )
                static_image = false;
        }
        if( static_image ) {
            // fill in info automatically
            off_x = 0;
            off_y = 0;
            width = image.width();
            if( image.height() % n_dimensions != 0 ) {
                LOG("image %d x %d ; n_dimensions = %d\n", image.width(), image.height(), n_dimensions);
                throw string("image height not a multiple of n_dimensions");
            }
            height = image.height() / n_dimensions;
            expected_total_width = width;
            stride_x = width;
            stride_y = height;
            //qDebug("image is %d x %d n_dimensions = %d", width, height, n_dimensions);
            //throw string("blah");
        }
        else {
            throw string("animated images must always be clipped");
        }
    }
    if( expected_total_width != image.width() ) {
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

AnimationLayer *AnimationLayer::create(const string &filename, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions) {
    QPixmap image = game_g->loadImage(filename.c_str());
    return create(image, animation_layer_definitions, clip, off_x, off_y, width, height,stride_x, stride_y, expected_total_width, n_dimensions);
}

AnimatedObject::AnimatedObject(int ms_per_frame) : /*animation_layer(NULL), c_animation_set(NULL),*/
    ms_per_frame(ms_per_frame), set_c_animation_name(false), c_dimension(0), c_frame(0), animation_time_start_ms(0), bounce(false)
{
}

AnimatedObject::~AnimatedObject() {
}

void AnimatedObject::advance(int phase) {
    //qDebug("AnimatedObject::advance() phase %d", phase);
    if( phase == 1 ) {
        //int time_elapsed_ms = game_g->getScreen()->getElapsedMS() - animation_time_start_ms;
        int time_elapsed_ms = game_g->getScreen()->getGameTimeTotalMS() - animation_time_start_ms;
        size_t n_frame = ( time_elapsed_ms / ms_per_frame );
        if( n_frame != c_frame ) {
            c_frame = n_frame;
            //this->update();
        }
    }
}

QRectF AnimatedObject::boundingRect() const {
    //qDebug("boundingRect");
    float width = static_cast<float>(this->getWidth());
    float height = static_cast<float>(this->getHeight());
    return QRectF(0.0f, 0.0f, width, height);
}

void AnimatedObject::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {

    //painter->setRenderHint(QPainter::SmoothPixmapTransform);
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

    for(vector<const AnimationSet *>::const_iterator iter = c_animation_sets.begin(); iter != c_animation_sets.end(); ++iter) {
        const AnimationSet *c_animation_set = *iter;
        const QPixmap &pixmap = c_animation_set->getFrame(c_dimension, c_frame);
        painter->drawPixmap(0, off_y, pixmap);
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
    //this->clearStaticImage();
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
                // reset to standard animation, needed for static images
                c_animation_set = animation_layer->getAnimationSet("");
                if( c_animation_set == NULL ) {
                    LOG("unknown animation set: %s - also can't find standard animation\n", name.c_str());
                    throw string("Unknown animation set");
                }
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
    ASSERT_LOGGER( this->animation_layers.size() > 0 );
    const AnimationLayer *animation_layer = this->animation_layers.at(0);
    return animation_layer->getWidth();
}

int AnimatedObject::getHeight() const {
    ASSERT_LOGGER( this->animation_layers.size() > 0 );
    const AnimationLayer *animation_layer = this->animation_layers.at(0);
    return animation_layer->getHeight();
}

LazyAnimationLayer::LazyAnimationLayer(const QPixmap &pixmap, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions) :
    animation_layer(NULL), clip(false), off_x(0), off_y(0), width(0), height(0), stride_x(0), stride_y(0), expected_total_width(0), n_dimensions(0)
{
    // pixmap already supplied, so we load straight away
    this->animation_layer = AnimationLayer::create(pixmap, animation_layer_definitions, clip, off_x, off_y, width, height, stride_x, stride_y, expected_total_width, n_dimensions);
}

AnimationLayer *LazyAnimationLayer::getAnimationLayer() {
    if( this->animation_layer == NULL ) {
        LOG("lazily load animation layer from: %s\n", this->filename.c_str());
        this->animation_layer = AnimationLayer::create(filename, animation_layer_definitions, clip, off_x, off_y, width, height, stride_x, stride_y, expected_total_width, n_dimensions);
        LOG("    done\n");
    }
    return this->animation_layer;
}

Particle::Particle() : xpos(0.0f), ypos(0.0f), birth_time(0), flag(false) {
    this->birth_time = game_g->getScreen()->getGameTimeTotalMS();
}

void ParticleSystem::advance(int phase) {
    //qDebug("AnimatedObject::advance() phase %d", phase);
    if( phase == 1 ) {
        this->update();
    }
}

void ParticleSystem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    //qDebug("draw %d particles", particles.size());
    const float size = 31.0f;
    for(vector<Particle>::const_iterator iter = particles.begin(); iter != particles.end(); ++iter) {
        painter->drawPixmap(iter->getX() - size*0.5f, iter->getY() - size*0.5f, size, size, this->pixmap);
    }
}

QRectF ParticleSystem::boundingRect() const {
    //qDebug("ParticleSystem::boundingRect()");
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
    const float xspeed = 0.03f;
    const float yspeed = 0.06f;
    // update particles
    for(int i=particles.size()-1;i>=0;i--) { // count backwards in case of deletion
            float xpos = particles.at(i).getX();
            float ypos = particles.at(i).getY();
            float xdiff = real_loop_time * xspeed;
            float ydiff = real_loop_time * yspeed;
            ypos -= ydiff;
            /*if( rand() % 2 == 0 ) {
                    xdiff = - xdiff;
            }*/
            int prob = poisson(100, real_loop_time);
            if( rand() % RAND_MAX <= prob ) {
                particles.at(i).setFlag( !particles.at(i).isFlag() );
            }
            if( !particles.at(i).isFlag() ) {
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
    //qDebug("accumulated_time = %d - %d = %d", game_g->getScreen()->getGameTimeTotalMS(), this->last_emit_time, accumulated_time);
    int new_particles = (int)(this->birth_rate/1000.0f * accumulated_time);
    this->last_emit_time += (int)(1000.0f/birth_rate * new_particles);
    if( new_particles > 0 ) {
            //qDebug("%d new particles (total will be %d)", new_particles, particles.size() + new_particles);
            for(int i=0;i<new_particles;i++) {
                    Particle particle;
                    particle.setFlag( rand() % 2 == 0 );
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
    QString nativePath(QDir::toNativeSeparators(pathQt));
    application_path = nativePath;
    logfilename = getApplicationFilename("log.txt");
    oldlogfilename = getApplicationFilename("log_old.txt");
    qDebug("application_path: %s", application_path.toUtf8().data());
    qDebug("logfilename: %s", logfilename.toUtf8().data());
    qDebug("oldlogfilename: %s", oldlogfilename.toUtf8().data());

    {
        QFile logfile(logfilename);
        if( !logfile.open(QIODevice::Append | QIODevice::Text) ) {
            qDebug("Failed to open logfile! Try in program folder");
            logfilename = "log.txt";
            oldlogfilename = "log_old.txt";
            qDebug("logfilename: %s", logfilename.toUtf8().data());
            qDebug("oldlogfilename: %s", oldlogfilename.toUtf8().data());
            QFile logfile2(logfilename);
            if( !logfile2.open(QIODevice::Append | QIODevice::Text) ) {
                qDebug("Still failed to open logfile!");
            }
        }
    }

    QFile::remove(oldlogfilename);
    QFile::rename(logfilename, oldlogfilename);
    QFile::remove(logfilename); // just in case we failed to rename, make sure the old log file is removed

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

    QString savegame_path = getApplicationFilename(savegame_folder);
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

void Game::run(bool fullscreen) {
    screen = new Screen(fullscreen);

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
        //QString font_family = "Candara";
        QString font_family = window->font().family();
        LOG("font family: %s\n", font_family.toStdString().c_str());
        this->font_scene = QFont(font_family, 16);
        this->font_small = QFont(font_family, 16);
        this->font_std = QFont(font_family, 18);
        this->font_big = QFont(font_family, 24, QFont::Bold);

        web_settings->setFontFamily(QWebSettings::StandardFont, font_std.family());
        web_settings->setFontSize(QWebSettings::DefaultFontSize, font_std.pointSize());
        web_settings->setFontSize(QWebSettings::DefaultFixedFontSize, font_std.pointSize());
    }

/*#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR)
    // crashes on Android?!
    // and doesn't work on Symbian
    // see Game::initButton()
#else*/
    /*unsigned char filter_max[] = {220, 220, 220};
    unsigned char filter_min[] = {120, 120, 120};*/
    unsigned char filter_max[] = {228, 217, 206};
    unsigned char filter_min[] = {160, 153, 143};
    QPixmap gui_pixmap_buttons = createNoise(256, 256, 16.0f, 16.0f, filter_max, filter_min, NOISEMODE_PERLIN, 4);
    this->gui_brush_buttons.setTexture(gui_pixmap_buttons);
    this->gui_palette.setBrush(QPalette::Button, gui_brush_buttons);
    this->gui_palette.setColor(QPalette::ButtonText, QColor(76, 0, 0));
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
    TEST_PERF_POINTINPOLYGON_0 = 17,
    TEST_PERF_POINTINPOLYGON_1 = 18,
    TEST_PERF_POINTINPOLYGON_2 = 19,
    TEST_PERF_DISTANCEGRAPH_0 = 20,
    TEST_PERF_PATHFINDING_0 = 21,
    TEST_PERF_REMOVE_SCENERY_0 = 22,
    TEST_PERF_REMOVE_SCENERY_1 = 23,
    TEST_PERF_REMOVE_SCENERY_2 = 24,
    TEST_PERF_UPDATE_VISIBILITY_0 = 25,
    TEST_PERF_NUDGE_0 = 26,
    TEST_PERF_NUDGE_1 = 27,
    TEST_PERF_NUDGE_2 = 28,
    TEST_PERF_NUDGE_3 = 29,
    TEST_PERF_NUDGE_4 = 30,
    TEST_PERF_NUDGE_5 = 31,
    TEST_PERF_NUDGE_6 = 32,
    TEST_PERF_NUDGE_7 = 33,
    TEST_PERF_NUDGE_8 = 34,
    TEST_PERF_NUDGE_9 = 35,
    TEST_PERF_NUDGE_10 = 36,
    TEST_PERF_NUDGE_11 = 37,
    TEST_PERF_NUDGE_12 = 38,
    TEST_PERF_NUDGE_13 = 39,
    TEST_PERF_NUDGE_14 = 40,
    N_TESTS = 41
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
  TEST_PERF_POINTINPOLYGON_0 - performance test version of TEST_POINTINPOLYGON_9
  TEST_PERF_POINTINPOLYGON_1 - performance test version of TEST_POINTINPOLYGON_11
  TEST_PERF_POINTINPOLYGON_2 - as TEST_PERF_POINTINPOLYGON_1, but point is now outside of AABB
  TEST_PERF_DISTANCEGRAPH_0 - performance test for calculating a distance graph
  TEST_PERF_PATHFINDING_0 - performance test for calculating a shortest path
  TEST_PERF_REMOVE_SCENERY_0 - performance test for removing scenery (including recalculating distance graph)
  TEST_PERF_REMOVE_SCENERY_1 - performance test for removing scenery that was blocking important waypoint (including recalculating distance graph)
  TETS_PERF_REMOVE_SCENERY_2 - performance test for removing scenery, also tests the bug (fixed in 0.7) where we weren't updating the nodes correctly
  TEST_PERF_UPDATE_VISIBILITY_0 - performance test for updating visibility
  TEST_PERF_NUDGE_0 - performance test for nudging: clicking far side on a west/east door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_1 - performance test for nudging: clicking near side on a west/east door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_2 - performance test for nudging: clicking far side on a north/south door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_3 - performance test for nudging: clicking near side on a north/south door (there exists another route, but we should nudge to the near side)
  TEST_PERF_NUDGE_4 - performance test for nudging: clicking far side outside a west/east door (there isn't another route, so we should internal-nudge to the near side)
  TEST_PERF_NUDGE_5 - performance test for nudging: clicking near side outside a west/east door
  TEST_PERF_NUDGE_6 - performance test for nudging: clicking far side outside a north/south door (there isn't another route, so we should internal-nudge to the near side)
  TEST_PERF_NUDGE_7 - performance test for nudging: clicking near side outside a north/south door
  TEST_PERF_NUDGE_8 - performance test for nudging: clicking on west side of u-shaped narrow passageway, away from wall
  TEST_PERF_NUDGE_9 - performance test for nudging: clicking on west side of u-shaped narrow passageway, close to wall
  TEST_PERF_NUDGE_10 - performance test for nudging: clicking on east side of u-shaped narrow passageway, away from wall
  TEST_PERF_NUDGE_11 - performance test for nudging: clicking on east side of u-shaped narrow passageway, close to wall
  TEST_PERF_NUDGE_12 - performance test for nudging: clicking on east side of scenery, src is on east side
  TEST_PERF_NUDGE_13 - performance test for nudging: clicking on west side of scenery, src is on east side
  TEST_PERF_NUDGE_14 - performance test for nudging: clicking near 90 degree corner
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
    bool has_score = false;
    double score = 0;

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
            location.createBoundariesForFixedNPCs();
            location.addSceneryToFloorRegions();
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
        else if( test_id == TEST_POINTINPOLYGON_9 || test_id == TEST_POINTINPOLYGON_10 || test_id == TEST_POINTINPOLYGON_11 ||
                 test_id == TEST_PERF_POINTINPOLYGON_0 || test_id == TEST_PERF_POINTINPOLYGON_1 || test_id == TEST_PERF_POINTINPOLYGON_2
                 ) {
            Polygon2D poly;
            poly.addPoint(Vector2D(0.0f, 0.0f));
            poly.addPoint(Vector2D(0.0f, 3.0f));
            poly.addPoint(Vector2D(2.0f, 3.0f));
            poly.addPoint(Vector2D(2.0f, 2.0f));
            poly.addPoint(Vector2D(1.0f, 2.0f));
            poly.addPoint(Vector2D(1.0f, 0.0f));
            Vector2D test_pt;
            bool exp_inside = false;
            if( test_id == TEST_POINTINPOLYGON_9 || test_id == TEST_PERF_POINTINPOLYGON_0 ) {
                test_pt.set(0.5f, 2.5f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_10 ) {
                test_pt.set(0.5f, 0.0f);
                exp_inside = true;
            }
            else if( test_id == TEST_POINTINPOLYGON_11 || test_id == TEST_PERF_POINTINPOLYGON_1 ) {
                test_pt.set(1.1f, 0.1f);
                exp_inside = false;
            }
            else {
                test_pt.set(1.1f, -0.1f);
                exp_inside = false;
            }

            if( test_id == TEST_POINTINPOLYGON_9 || test_id == TEST_POINTINPOLYGON_10 || test_id == TEST_POINTINPOLYGON_11 ) {
                bool inside = poly.pointInside(test_pt);
                if( inside != exp_inside ) {
                    throw string("failed point inside polygon test");
                }
            }
            else {
                QElapsedTimer timer;
                timer.start();
                //int time_s = clock();
                int n_times = 1000000;
                for(int i=0;i<n_times;i++) {
                    bool inside = poly.pointInside(test_pt);
                    if( inside != exp_inside ) {
                        throw string("failed point inside polygon test");
                    }
                }
                has_score = true;
                //score = ((double)(clock() - time_s)) / (double)n_times;
                score = ((double)timer.elapsed()) / ((double)n_times);
                score /= 1000.0;
            }
        }
        else if( test_id == TEST_PERF_DISTANCEGRAPH_0 || test_id == TEST_PERF_PATHFINDING_0 || test_id == TEST_PERF_REMOVE_SCENERY_0 || test_id == TEST_PERF_REMOVE_SCENERY_1 || test_id == TEST_PERF_REMOVE_SCENERY_2 || test_id == TEST_PERF_UPDATE_VISIBILITY_0 ) {
            Location location("");

            FloorRegion *floor_region = NULL;
            if( test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 0.0f, 1.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 3.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
            }
            else {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 3.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(10.0f, 1.0f, 4.0f, 3.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(14.0f, 2.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(1.0f, 5.0f, 2.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 10.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(1.0f, 15.0f, 2.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 20.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(1.0f, 25.0f, 2.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 30.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(5.0f, 22.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(10.0f, 20.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);

                floor_region = FloorRegion::createRectangle(15.0f, 22.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(20.0f, 20.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
            }

            Scenery *scenery = NULL;
            if( test_id == TEST_PERF_REMOVE_SCENERY_0 ) {
                scenery = new Scenery("", "", false, 1.0f, 1.0f, 1.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 5.5f, 22.5f);
            }
            else if( test_id == TEST_PERF_REMOVE_SCENERY_1 ) {
                scenery = new Scenery("", "", false, 1.0f, 1.0f, 1.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 4.25f, 22.5f);
            }
            else if( test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                scenery = new Scenery("", "", false, 0.1f, 1.0f, 1.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 4.95f, 3.5f);
            }

            location.createBoundariesForRegions();
            location.createBoundariesForScenery();
            location.createBoundariesForFixedNPCs();
            location.addSceneryToFloorRegions();
            location.calculateDistanceGraph();

            QElapsedTimer timer;
            timer.start();
            int n_times = 1000;
            if( test_id == TEST_PERF_REMOVE_SCENERY_0 || test_id == TEST_PERF_REMOVE_SCENERY_1 || test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                n_times = 1; // test can only be run once!
            }
            for(int i=0;i<n_times;i++) {
                if( test_id == TEST_PERF_DISTANCEGRAPH_0 ) {
                    location.calculateDistanceGraph();
                }
                else if( test_id == TEST_PERF_PATHFINDING_0 ) {
                    Vector2D src(1.0f, 1.0f);
                    Vector2D dest(21.0f, 21.0f);
                    vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() == 0 ) {
                        throw string("failed to find path");
                    }
                }
                else if( test_id == TEST_PERF_REMOVE_SCENERY_0 || test_id == TEST_PERF_REMOVE_SCENERY_1 ) {
                    Vector2D src(1.0f, 1.0f);
                    Vector2D dest(21.0f, 21.0f);
                    vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() != 0 ) {
                        for(vector<Vector2D>::const_iterator iter = path.begin(); iter != path.end(); ++iter) {
                            Vector2D pos = *iter;
                            LOG("path pos: %f, %f\n", pos.x, pos.y);
                        }
                        throw string("unexpectedly found a path");
                    }

                    location.removeScenery(scenery);

                    path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() == 0 ) {
                        throw string("failed to find path");
                    }
                }
                else if( test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                    Vector2D src(0.5f, 0.5f);
                    Vector2D dest(0.5f, 3.5f);
                    vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() != 0 ) {
                        for(vector<Vector2D>::const_iterator iter = path.begin(); iter != path.end(); ++iter) {
                            Vector2D pos = *iter;
                            LOG("path pos: %f, %f\n", pos.x, pos.y);
                        }
                        throw string("unexpectedly found a path");
                    }

                    location.removeScenery(scenery);

                    path = location.calculatePathTo(src, dest, NULL, false);
                    if( path.size() == 0 ) {
                        throw string("failed to find path");
                    }
                }
                else if( test_id == TEST_PERF_UPDATE_VISIBILITY_0 ) {
                    Vector2D pos(1.0f, 1.0f);
                    location.clearVisibility();
                    vector<FloorRegion *> floor_regions = location.updateVisibility(pos);
                    if( floor_regions.size() == 0 ) {
                        throw string("didn't find any floor regions");
                    }
                }
            }
            has_score = true;
            score = ((double)timer.elapsed()) / ((double)n_times);
            score /= 1000.0;

            //vector<Vector2D> path = location.calculatePathTo(src, dest, NULL, false);

        }
        else if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 || test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 || test_id == TEST_PERF_NUDGE_4 || test_id == TEST_PERF_NUDGE_5 || test_id == TEST_PERF_NUDGE_6 || test_id == TEST_PERF_NUDGE_7 || test_id == TEST_PERF_NUDGE_8 || test_id == TEST_PERF_NUDGE_9 || test_id == TEST_PERF_NUDGE_10 || test_id == TEST_PERF_NUDGE_11 || test_id == TEST_PERF_NUDGE_12 || test_id == TEST_PERF_NUDGE_13 || test_id == TEST_PERF_NUDGE_14 ) {
            Location location("");

            FloorRegion *floor_region = NULL;
            if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 || test_id == TEST_PERF_NUDGE_4 || test_id == TEST_PERF_NUDGE_5 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 2.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 ) {
                    floor_region = FloorRegion::createRectangle(0.0f, 3.0f, 1.0f, 1.0f);
                    location.addFloorRegion(floor_region);
                    floor_region = FloorRegion::createRectangle(0.0f, 4.0f, 5.0f, 1.0f);
                    location.addFloorRegion(floor_region);
                }
            }
            else if( test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 || test_id == TEST_PERF_NUDGE_6 || test_id == TEST_PERF_NUDGE_7 ) {
                floor_region = FloorRegion::createRectangle(2.0f, 0.0f, 1.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(0.0f, 5.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                if( test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 ) {
                    floor_region = FloorRegion::createRectangle(3.0f, 0.0f, 1.0f, 1.0f);
                    location.addFloorRegion(floor_region);
                    floor_region = FloorRegion::createRectangle(4.0f, 0.0f, 1.0f, 5.0f);
                    location.addFloorRegion(floor_region);
                }
            }
            else if( test_id == TEST_PERF_NUDGE_8 || test_id == TEST_PERF_NUDGE_9 || test_id == TEST_PERF_NUDGE_10 || test_id == TEST_PERF_NUDGE_11 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.0f, 4.0f, 0.01f, 1.0f);
                location.addFloorRegion(floor_region);
                floor_region = FloorRegion::createRectangle(5.01f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
            }
            else if( test_id == TEST_PERF_NUDGE_12 || test_id == TEST_PERF_NUDGE_13 || test_id == TEST_PERF_NUDGE_14 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
            }

            Scenery *scenery = NULL;
            if( test_id == TEST_PERF_NUDGE_0 || test_id == TEST_PERF_NUDGE_1 || test_id == TEST_PERF_NUDGE_4 || test_id == TEST_PERF_NUDGE_5 ) {
                scenery = new Scenery("", "", false, 0.1f, 1.0f, 1.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 4.95f, 2.5f);
            }
            else if( test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 || test_id == TEST_PERF_NUDGE_6 || test_id == TEST_PERF_NUDGE_7 ) {
                scenery = new Scenery("", "", false, 1.0f, 0.1f, 0.9f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 4.95f);
            }
            else if( test_id == TEST_PERF_NUDGE_12 || test_id == TEST_PERF_NUDGE_13 ) {
                scenery = new Scenery("", "", false, 1.0f, 1.0f, 1.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 2.5f);
            }

            location.createBoundariesForRegions();
            location.createBoundariesForScenery();
            location.createBoundariesForFixedNPCs();
            location.addSceneryToFloorRegions();
            location.calculateDistanceGraph();
            for(size_t i=0;i<location.getNFloorRegions();i++) {
                FloorRegion *floor_region = location.getFloorRegion(i);
                floor_region->setVisible(true);
            }

            QElapsedTimer timer;
            timer.start();
            int n_times = 1000;
            for(int i=0;i<n_times;i++) {
                Vector2D src, pos, expected_nudge;
                if( test_id == TEST_PERF_NUDGE_0 ) {
                    src = Vector2D(6.0f, 2.5f);
                    pos = Vector2D(4.91f, 2.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 2.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_1 ) {
                    src = Vector2D(6.0f, 2.5f);
                    pos = Vector2D(4.99f, 2.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 2.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_2 ) {
                    src = Vector2D(2.5f, 6.0f);
                    pos = Vector2D(2.5f, 4.91f);
                    expected_nudge = Vector2D(2.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_3 ) {
                    src = Vector2D(2.5f, 6.0f);
                    pos = Vector2D(2.5f, 4.99f);
                    expected_nudge = Vector2D(2.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_4 ) {
                    src = Vector2D(6.0f, 3.5f);
                    pos = Vector2D(4.85f, 3.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 3.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_5 ) {
                    src = Vector2D(6.0f, 3.5f);
                    pos = Vector2D(5.05f, 3.5f);
                    expected_nudge = Vector2D(5.0f + npc_radius_c + E_TOL_LINEAR, 3.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_6 ) {
                    src = Vector2D(3.5f, 6.0f);
                    pos = Vector2D(3.5f, 4.85f);
                    expected_nudge = Vector2D(3.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_7 ) {
                    src = Vector2D(3.5f, 6.0f);
                    pos = Vector2D(3.5f, 5.05f);
                    expected_nudge = Vector2D(3.5f, 5.0f + npc_radius_c + E_TOL_LINEAR);
                }
                else if( test_id == TEST_PERF_NUDGE_8 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(4.5f, 3.0f);
                    expected_nudge = Vector2D(4.5f, 3.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_9 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(4.9f, 2.0f);
                    expected_nudge = Vector2D(5.0f - npc_radius_c - E_TOL_LINEAR, 2.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_10 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(5.5f, 3.0f);
                    expected_nudge = Vector2D(5.5f, 3.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_11 ) {
                    src = Vector2D(4.5f, 2.0f);
                    pos = Vector2D(5.11f, 2.0f);
                    expected_nudge = Vector2D(5.01f + npc_radius_c + E_TOL_LINEAR, 2.0f);
                }
                else if( test_id == TEST_PERF_NUDGE_12 ) {
                    src = Vector2D(3.5f, 2.5f);
                    pos = Vector2D(3.1f, 2.5f);
                    expected_nudge = Vector2D(3.0f + npc_radius_c + E_TOL_LINEAR, 2.5f);
                }
                else if( test_id == TEST_PERF_NUDGE_13 ) {
                    src = Vector2D(3.5f, 2.5f);
                    pos = Vector2D(1.9f, 2.6f);
                    expected_nudge = Vector2D(2.0f - npc_radius_c - E_TOL_LINEAR, 2.6f);
                }
                else if( test_id == TEST_PERF_NUDGE_14 ) {
                    src = Vector2D(2.5f, 2.5f);
                    pos = Vector2D(0.1f, 4.95f);
                    expected_nudge = Vector2D(npc_radius_c + E_TOL_LINEAR, 5.0f - npc_radius_c - E_TOL_LINEAR);
                }
                Vector2D nudge = location.nudgeToFreeSpace(src, pos, npc_radius_c);
                if( (nudge - expected_nudge).magnitude() > E_TOL_LINEAR ) {
                    LOG("src: %f, %f\n", src.x, src.y);
                    LOG("pos: %f, %f\n", pos.x, pos.y);
                    LOG("nudge: %f, %f\n", nudge.x, nudge.y);
                    LOG("expected_nudge: %f, %f\n", expected_nudge.x, expected_nudge.y);
                    throw string("unexpected nudge");
                }
                vector<Vector2D> path = location.calculatePathTo(src, nudge, NULL, false);
                if( path.size() == 0 ) {
                    throw string("failed to find path");
                }
            }
            has_score = true;
            score = ((double)timer.elapsed()) / ((double)n_times);
            score /= 1000.0;
        }
        else {
            throw string("unknown test");
        }

        fprintf(testfile, "PASSED,");
        if( has_score ) {
            if( score < 0.01 ) {
                fprintf(testfile, "%E", score);
            }
            else {
                fprintf(testfile, "%f", score);
            }
        }
        fprintf(testfile, "\n");
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
        LOG("<<< TEST %d FAILED\n", test_id);
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
        fprintf(testfile, "TEST,RESULT,SCORE\n");
        fclose(testfile);
    }

    for(int i=0;i<N_TESTS;i++) {
        runTest(filename, i);
    }
    //runTest(filename, TEST_PERF_NUDGE_14);
}

void Game::initButton(QAbstractButton *button) const {
    MainWindow *window = this->screen->getMainWindow();
    button->setFont(window->font()); // needed for Android at least
    button->setStyle(style);
    button->setAutoFillBackground(true);
    button->setPalette(this->gui_palette);
}

void Game::handleMessages() {
    //qDebug("Game::handleMessages()");

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
                PlayingGamestate *playing_gamestate = new PlayingGamestate(false, start_message->getPlayerType(), start_message->getName(), start_message->getPermadeath(), start_message->getCheatMode(), start_message->getCheatStartLevel());
                gamestate = playing_gamestate;
                playing_gamestate->setDifficulty(start_message->getDifficulty());

                if( start_message->getGametype() == GAMETYPE_CAMPAIGN ) {
                    const QuestInfo &c_quest_info = playing_gamestate->getCQuestInfo();
                    QString qt_filename = DEPLOYMENT_PATH + QString(c_quest_info.getFilename().c_str());
                    playing_gamestate->loadQuest(qt_filename, false);
                }
                else if( start_message->getGametype() == GAMETYPE_RANDOM ) {
                    playing_gamestate->createRandomQuest();
                }
                else {
                    ASSERT_LOGGER(false);
                }

                this->getScreen()->getMainWindow()->unsetCursor();
                break;
            }
            case GameMessage::GAMEMESSAGETYPE_NEWGAMESTATE_PLAYING_LOAD:
            {
                delete gamestate;
                gamestate = NULL;
                PlayingGamestate *playing_gamestate = new PlayingGamestate(true, 0, "", false, false, 0);
                gamestate = playing_gamestate;
                try {
                    LoadGameMessage *load_message = static_cast<LoadGameMessage *>(message);
                    QString full_filename = this->getApplicationFilename(savegame_folder + load_message->getFilename());
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
}

void Game::update() {
    this->handleMessages(); // needed to process any messages from earlier update call
    if( gamestate != NULL ) {
        gamestate->update();
    }
}

void Game::updateInput() {
    this->handleMessages(); // needed to process any messages from earlier updateInput call
    if( gamestate != NULL ) {
        gamestate->updateInput();
    }
}

void Game::render() {
    if( gamestate != NULL ) {
        gamestate->render();
    }
}

/*void Game::mouseClick(int m_x, int m_y) {
    gamestate->mouseClick(m_x, m_y);
}*/

QString Game::getApplicationFilename(const QString &name) {
    // not safe to use LOG here, as logfile may not have been initialised!
    QString pathQt = application_path + QString("/") + name;
    QString nativePath(QDir::toNativeSeparators(pathQt));
    qDebug("getApplicationFilename returns: %s", nativePath.toUtf8().data());
    return nativePath;
}

void Game::log(const char *text, va_list vlist) {
    char buffer[65536] = "";
    vsprintf(buffer,text,vlist);

    QFile logfile(logfilename);
    if( logfile.open(QIODevice::Append | QIODevice::Text) ) {
        QTextStream stream(&logfile);
        stream << buffer;
    }
    qDebug("%s", buffer);
}

QPixmap Game::loadImage(const string &filename, bool clip, int xpos, int ypos, int width, int height, int expected_width) const {
    // need to use QImageReader - QPixmap::load doesn't work on large images on Symbian!
    QImageReader reader(filename.c_str());
    //qDebug("Game::loadImage(): %s", filename.c_str());
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
        //throw string("Failed to convert image to pixmap");
        string error;
        if( reader.error() == QImageReader::InvalidDataError ) {
            // Normally this code returns the string "Unable to read image data", but (on Symbian at least) can occur when we run
            // out of memory. Better to return a more helpful code (and make it clear it's not a bug, but a problem with lack of resources!)
            error = "Out of memory";
        }
        else {
            error = reader.errorString().toStdString();
        }
        throw error;
    }
    //qDebug("    %d  %d\n", pixmap.width(), pixmap.height());
    return pixmap;
}

void Game::stateChanged(Phonon::State newstate, Phonon::State oldstate) const {
#ifndef Q_OS_ANDROID
    if( newstate == Phonon::ErrorState ) {
        LOG("Game::stateChanged received Phonon::ErrorState\n");
        Phonon::MediaObject *mediaObject = static_cast<Phonon::MediaObject *>(this->sender());
        LOG("    error code %d\n", mediaObject->errorType());
        LOG("    error string: %s\n", mediaObject->errorString().toStdString().c_str());
    }
#endif
}

void Game::loadSound(const string &id, const string &filename) {
#ifndef Q_OS_ANDROID
    Phonon::MediaObject *mediaObject = new Phonon::MediaObject(qApp);
    if( mediaObject == NULL ) {
        LOG("failed to create media object for: %s\n", filename.c_str());
    }
    else {
        connect(mediaObject, SIGNAL(stateChanged(Phonon::State,Phonon::State)), this, SLOT(stateChanged(Phonon::State, Phonon::State)));
        mediaObject->setCurrentSource(Phonon::MediaSource(filename.c_str()));
        Phonon::AudioOutput *audioOutput = new Phonon::AudioOutput(Phonon::GameCategory, qApp);
        if( audioOutput == NULL ) {
            LOG("failed to create audio output for: %s\n", filename.c_str());
        }
        else {
            Phonon::Path audioPath = Phonon::createPath(mediaObject, audioOutput);
            Sound *sound = new Sound(mediaObject, audioOutput);
            this->sound_effects[id] = sound;
        }
    }
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
    //qDebug("play sound: %s\n", sound_effect.c_str());
    if( game_g->isSoundEnabled() ) {
        Sound *sound = this->sound_effects[sound_effect];
        if( sound != NULL ) {
#ifndef Q_OS_ANDROID
            if( sound->state() == Phonon::PlayingState ) {
                //qDebug("    already playing");
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

Character *Game::createPlayer(size_t i, const string &player_name) const {
    ASSERT_LOGGER(i < this->getNPlayerTypes() );
    //Character *character = new Character(player_types.at(i), "", false);
    Character *character = new Character(player_name, "", false);
    if( i == 0 ) {
        // barbarian
        character->setProfile(8, 6, 8, 1, 5, 5, 8, 1.8f);
        character->initialiseHealth(75);
        character->setBiography("");
        character->setPortrait("portrait_barbarian");
    }
    else if( i == 1 ) {
        // elf
        character->setProfile(7, 8, 6, 1, 8, 7, 8, 2.25f);
        character->initialiseHealth(60);
        character->setBiography("");
        character->setPortrait("portrait_elf");
    }
    else if( i == 2 ) {
        // halfling
        character->setProfile(7, 7, 5, 1, 7, 9, 7, 1.8f);
        character->initialiseHealth(50);
        character->setBiography("");
        character->setPortrait("portrait_halfling");
    }
    else if( i == 3 ) {
        // ranger
        character->setProfile(7, 8, 7, 1, 7, 8, 6, 2.2f);
        character->initialiseHealth(60);
        character->setBiography("");
        character->setPortrait("portrait_ranger");
    }
    else if( i == 4 ) {
        // warrior
        character->setProfile(8, 7, 7, 1, 6, 7, 7, 2.0f);
        character->initialiseHealth(70);
        character->setBiography("");
        character->setPortrait("portrait_warrior");
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
        this->getScreen()->setPaused(true, false); // automatically pause when application goes inactive
    }
    if( this->gamestate != NULL ) {
        this->gamestate->activate(active);
    }
}

void Game::keyPress(QKeyEvent *key_event) {
    if( this->gamestate != NULL ) {
        this->gamestate->keyPress(key_event);
    }
}

void Game::fillSaveGameFiles(ScrollingListWidget **list, vector<QString> *filenames) const {
    QDir dir( game_g->getApplicationFilename(savegame_folder) );
    QStringList filter;
    filter << "*" + savegame_ext;
    QFileInfoList files = dir.entryInfoList(filter, QDir::Files, QDir::Time);
    if( files.size() > 0 ) {
        if( *list == NULL ) {
            *list = new ScrollingListWidget();
            (*list)->grabKeyboard();
            if( !mobile_c ) {
                QFont list_font = (*list)->font();
                list_font.setPointSize( list_font.pointSize() + 8 );
                (*list)->setFont(list_font);
            }
            (*list)->setSelectionMode(QAbstractItemView::SingleSelection);
        }

        for(size_t i=0;i<files.size();i++) {
            QFileInfo file_info = files.at(i);
            filenames->push_back(file_info.fileName());
            QString file_str = file_info.fileName();
            file_str += " [" + file_info.lastModified().toString("d-MMM-yyyy hh:mm") + "]";
            (*list)->addItem(file_str);
        }
    }

}
