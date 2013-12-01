#include "game.h"
#include "optionsgamestate.h"
#include "playinggamestate.h"
#include "qt_screen.h"
#include "qt_utils.h"
#include "sound.h"
#include "logiface.h"

#ifdef Q_OS_ANDROID
#include "androidaudio/androidsound.h"
#endif

#include <ctime>

#include <QApplication>
#include <QDesktopWidget>
#include <QXmlStreamReader>
#include <QTextStream>
#include <QImageReader>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QStyleFactory>
#include <QDir>
#include <QDateTime>

#ifdef USING_WEBKIT
#include <QWebFrame>
#else
#include <QTextEdit>
#include <QUrl>
#endif

#if QT_VERSION < 0x050000
#include <QWindowsStyle>
#include <qmath.h>
#endif

#ifdef _DEBUG
#include <cassert>
#endif

Game *game_g = NULL;

/*const QString sound_enabled_key_c = "sound_enabled";
const int default_sound_enabled_c = true;*/
#ifndef Q_OS_ANDROID
const QString sound_volume_key_c = "sound_volume";
const int default_sound_volume_c = 100;
const QString sound_volume_music_key_c = "sound_volume_music";
const int default_sound_volume_music_c = 100;
const QString sound_volume_effects_key_c = "sound_volume_effects";
const int default_sound_volume_effects_c = 100;
#endif

const QString lighting_enabled_key_c = "lighting_enabled";
#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
const int default_lighting_enabled_c = false; // lighting effects can be a bit too slow on mobile platforms
#else
const int default_lighting_enabled_c = true;
#endif

#ifdef USING_WEBKIT
void WebViewEventFilter::setWebView(QWebView *webView) {
    qDebug("setWebView");
    this->webView = webView;
    this->webView->installEventFilter(this);
    //this->textEdit = NULL;
    this->filterMouseMove = false;
    this->orig_mouse_x = 0;
    this->orig_mouse_y = 0;
    this->saved_mouse_x = 0;
    this->saved_mouse_y = 0;
    this->last_scroll_y = -1;
}
#else
void WebViewEventFilter::setTextEdit(QTextEdit *textEdit) {
    qDebug("setTextEdit");
    this->textEdit = textEdit;
    this->textEdit->installEventFilter(this);
    //this->webView = NULL;
    this->filterMouseMove = false;
    this->orig_mouse_x = 0;
    this->orig_mouse_y = 0;
    this->saved_mouse_x = 0;
    this->saved_mouse_y = 0;
    this->last_scroll_y = -1;
}
#endif

// returns true to filter the event
bool WebViewEventFilter::eventFilter(QObject *obj, QEvent *event) {
    //qDebug("eventFilter type: %d", event->type());
    switch( event->type() ) {
        case QEvent::Timer:
            {
                // hack to fix problem where when drag-scrolling with mouse or touch, when mouse/pointer moves outside the textedit, it starts scrolling in the opposte direction
                return true;
                break;
            }
        case QEvent::MouseButtonPress:
            {
                //qDebug("MouseButtonPress");
                QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                if( mouseEvent->button() == Qt::LeftButton ) {
                    //filterMouseMove = true;
                    // disallow selection - but we need to allow click and drag for the scroll bars!
#ifdef USING_WEBKIT
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
#else
                    if( textEdit != NULL ) {
                        filterMouseMove = true;
                        orig_mouse_x = saved_mouse_x = mouseEvent->globalX();
                        orig_mouse_y = saved_mouse_y = mouseEvent->globalY();
                    }
#endif
                }
                break;
            }
        case QEvent::MouseButtonRelease:
            {
                //qDebug("MouseButtonRelease");
#ifndef USING_WEBKIT
                if( textEdit != NULL && last_scroll_y != -1 ) {
                    // fix problem where when drag-scrolling with mouse or touch, if mouse/pointer has moved outside the textedit, it scrolls back to original position when we let go
                    textEdit->verticalScrollBar()->setValue(last_scroll_y);
                }
#endif
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
#ifndef USING_WEBKIT
                if( textEdit != NULL ) {
                    // need to handle links manually!
                    // problem that Qt::LinksAccessibleByMouse messes up the drag scrolling, so have to use Qt::NoTextInteraction
                    QString url = textEdit->anchorAt(mouseEvent->pos());
                    if( url.length() > 0 ) {
                        LOG("textEdit: clicked on: %s\n", url.toStdString().c_str());
                        if( url.at(0) == '#' ) {
                            last_scroll_y = -1;
                            textEdit->scrollToAnchor(url.mid(1));
                        }
                        else if( url.contains("://") ) {
                            QDesktopServices::openUrl(url);
                        }
                        else {
                            QFile file(QString(DEPLOYMENT_PATH) + "docs/" + url);
                            if( file.open(QFile::ReadOnly | QFile::Text) ) {
                                QTextStream in(&file);
                                textEdit->setHtml(in.readAll());
                            }
                            else {
                                LOG("failed to load: %s\n", url.toStdString().c_str());
                            }
                        }
                    }
                }
#endif
                break;
            }
        case QEvent::MouseMove:
            {
                //qDebug("MouseMove");
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
                    //if( webView != NULL || textEdit != NULL ) {
#ifdef USING_WEBKIT
                    if( webView != NULL ) {
#else
                    if( textEdit != NULL ) {
#endif
                        // support for swipe scrolling
                        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                        int new_mouse_x = mouseEvent->globalX();
                        int new_mouse_y = mouseEvent->globalY();
                        //qDebug("mouse %d, %d", new_mouse_x, new_mouse_y);
#ifdef USING_WEBKIT
                        if( webView != NULL ) {
                            webView->page()->mainFrame()->scroll(saved_mouse_x - new_mouse_x, saved_mouse_y - new_mouse_y);
                        }
#else
                        if( textEdit != NULL ){
                            qDebug("scroll %d, %d", saved_mouse_x - new_mouse_x, saved_mouse_y - new_mouse_y);
                            int value = textEdit->verticalScrollBar()->value();
                            value += saved_mouse_y - new_mouse_y;
                            if( value < textEdit->verticalScrollBar()->minimum() )
                                value = textEdit->verticalScrollBar()->minimum();
                            if( value > textEdit->verticalScrollBar()->maximum() )
                                value = textEdit->verticalScrollBar()->maximum();
                            textEdit->verticalScrollBar()->setValue(value);
                            //qDebug("    value is now %d", value);
                            last_scroll_y = value;

                            value = textEdit->horizontalScrollBar()->value();
                            value += saved_mouse_x - new_mouse_x;
                            if( value < textEdit->horizontalScrollBar()->minimum() )
                                value = textEdit->horizontalScrollBar()->minimum();
                            if( value > textEdit->horizontalScrollBar()->maximum() )
                                value = textEdit->horizontalScrollBar()->maximum();
                            textEdit->horizontalScrollBar()->setValue(value);
                            //qDebug("    value is now %d", value);
                        }
#endif
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
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            last_scroll_y = -1;
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
    ms_per_frame(ms_per_frame), set_c_animation_name(false), c_dimension(0), c_frame(0), animation_time_start_ms(0), bounce(false),
    clip(false), clip_sx(0), clip_sy(0), clip_sw(0), clip_sh(0)
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
            this->update();
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
        if( this->clip ) {
            int i_sx = (int)(clip_sx * pixmap.width());
            int i_sy = (int)(clip_sy * pixmap.height());
            // do it this way, rather than scaling i_sw or i_sh directly, to avoid problems with rounding
            int i_sx2 = (int)((clip_sx+clip_sw) * pixmap.width());
            int i_sy2 = (int)((clip_sy+clip_sh) * pixmap.height());
            int i_sw = i_sx2 - i_sx;
            int i_sh = i_sy2 - i_sy;
            painter->drawPixmap(i_sx, i_sy+off_y, pixmap, i_sx, i_sy, i_sw, i_sh);
        }
        else {
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
        this->updatePS();
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
    // Since we update the entire view every frame anyway, it doesn't matter what rect we return - and there's no point trying to return a more optimal rect.
    return QRectF(-100, -100, 200, 200);
}

SmokeParticleSystem::SmokeParticleSystem(const QPixmap &pixmap) : ParticleSystem(pixmap),
    birth_rate(0.0f), life_exp(1500), last_emit_time(0) {
    this->last_emit_time = game_g->getScreen()->getGameTimeTotalMS();
}

void SmokeParticleSystem::setBirthRate(float birth_rate) {
    this->birth_rate = birth_rate;
}

void SmokeParticleSystem::updatePS() {
    //qDebug("smoke update");
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

    this->update();
}

ScrollingListWidget::ScrollingListWidget() : QListWidget(), saved_x(0), saved_y(0) {
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
#ifdef Q_OS_ANDROID
    this->setStyleSheet("color: black; background-color: white"); // workaround for Android colour problem
#endif
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

Game::Game() : settings(NULL), style(NULL), webViewEventFilter(NULL), gamestate(NULL), screen(NULL), /*sound_enabled(default_sound_enabled_c),*/
#ifdef Q_OS_ANDROID
    sdcard_ok(false),
#else
    sound_volume(default_sound_volume_c),
    sound_volume_music(default_sound_volume_music_c),
    sound_volume_effects(default_sound_volume_effects_c),
#endif
    lighting_enabled(default_lighting_enabled_c) {
    game_g = this;

    QCoreApplication::setApplicationName("erebus");
    settings = new QSettings("Mark Harman", "erebus");

#if QT_VERSION >= 0x050000
    style = QStyleFactory::create("windows");
#else
    style = new QWindowsStyle(); // needed to get the textured buttons (which doesn't work with Windows XP, Symbian or Android styles)
#endif

    webViewEventFilter = new WebViewEventFilter();

    // initialise paths
    // n.b., not safe to use logging until after copied/removed old log files!
#if QT_VERSION >= 0x050000
    QString pathQt = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#else
    QString pathQt = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#endif
    qDebug("user data location path: %s", pathQt.toStdString().c_str());
    if( !QDir(pathQt).exists() ) {
        QDir().mkpath(pathQt);
    }
    QString nativePath(QDir::toNativeSeparators(pathQt));
    application_path = nativePath;

#if defined(Q_OS_ANDROID)
    // on Android, try for the sdcard, so we can find somewhere more accessible to the user (and that I can read on my Galaxy Nexus!!!)
    this->sdcard_ok = true;
    sdcard_path = QString("/sdcard/net.sourceforge.erebusrpg");
    qDebug("try sd card: %s", sdcard_path.toStdString().c_str());
    if( !QDir(sdcard_path).exists() ) {
        qDebug("try creating application folder in sdcard/");
        // folder doesn't seem to exist - try creating it
        QDir().mkdir(sdcard_path);
        if( !QDir(sdcard_path).exists() ) {
            qDebug("failed to create application folder in sdcard/");
            sdcard_ok = false;
        }
    }
    if( sdcard_ok ) {
        logfilename = getFilename(sdcard_path, "log.txt");
        oldlogfilename = getFilename(sdcard_path, "log_old.txt");
    }
    else {
        logfilename = getApplicationFilename("log.txt");
        oldlogfilename = getApplicationFilename("log_old.txt");
    }
#else
    logfilename = getApplicationFilename("log.txt");
    oldlogfilename = getApplicationFilename("log_old.txt");
#endif
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

    LOG("Data folder: %s\n", DEPLOYMENT_PATH);

    bool ok = true;

#ifndef Q_OS_ANDROID
    int sound_volume_i = settings->value(sound_volume_key_c, default_sound_volume_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings sound_volume not ok, set to default");
        this->sound_volume = default_sound_volume_c;
    }
    else {
        this->sound_volume = sound_volume_i;
    }

    int sound_volume_music_i = settings->value(sound_volume_music_key_c, default_sound_volume_music_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings sound_volume_music not ok, set to default");
        this->sound_volume_music = default_sound_volume_music_c;
    }
    else {
        this->sound_volume_music = sound_volume_music_i;
    }

    int sound_volume_effects_i = settings->value(sound_volume_effects_key_c, default_sound_volume_effects_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings sound_volume_effects not ok, set to default");
        this->sound_volume_effects = default_sound_volume_effects_c;
    }
    else {
        this->sound_volume_effects = sound_volume_effects_i;
    }
#endif

    int lighting_enabled_i = settings->value(lighting_enabled_key_c, default_lighting_enabled_c).toInt(&ok);
    if( !ok ) {
        qDebug("settings lighting_enabled not ok, set to default");
        this->lighting_enabled = default_lighting_enabled_c;
    }
    else {
        this->lighting_enabled = lighting_enabled_i != 0;
    }

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
    LOG("Game::~Game()\n");
    if( screen != NULL ) {
        LOG("delete screen\n");
        // hack to prevent the MyApplication::event() from calling screen functions when deactivating
        Screen *local_screen = screen;
        screen = NULL;
        delete local_screen;
    }

    LOG("delete sounds\n");
    for(map<string, Sound *>::iterator iter = this->sound_effects.begin(); iter != this->sound_effects.end(); ++iter) {
        Sound *sound = iter->second;
        delete sound;
    }

    if( webViewEventFilter != NULL ) {
        LOG("delete webViewEventFilter");
        delete webViewEventFilter;
    }

    if( style != NULL ) {
        LOG("delete style\n");
        delete style;
    }

    if( settings != NULL ) {
        LOG("delete settings");
        delete settings;
    }

    LOG("Game::~Game() done\n");
    game_g = NULL;
}

void Game::init(bool fullscreen) {
    LOG("fullscreen? %d\n", fullscreen);
    screen = new Screen(fullscreen);

    LOG("smallscreen? %d\n", smallscreen_c);
    LOG("touchscreen? %d\n", touchscreen_c);
    LOG("lightdistribution? %d\n", lightdistribution_c);

    // setup fonts
    MainWindow *window = game_g->getScreen()->getMainWindow();
    // palettes disabled for now - problems with differences between platforms, plus setting it for this window doesn't apply to the subwindows that we open up
    /*{
        QPalette palette = window->palette();
        palette.setColor(QPalette::Window, Qt::black);
        palette.setColor(QPalette::WindowText, Qt::gray);
        // we should also set sensible defaults (note that buttons usually have Button and ButtonText set via gui_palette, but we should still set a sensible default
        palette.setColor(QPalette::Button, Qt::lightGray);
        palette.setColor(QPalette::ButtonText, Qt::black);
        // n.b., on Android, Comboboxes uses "Text" for the text, but uses "Window" (instead of "Base") for the background)
        // so for consistency between platforms we set Window and Base to be the same
        palette.setColor(QPalette::Base, Qt::black);
        palette.setColor(QPalette::Text, Qt::white);
        window->setPalette(palette);
    }*/
#ifdef USING_WEBKIT
    QWebSettings *web_settings = QWebSettings::globalSettings();
#endif
    int screen_w = QApplication::desktop()->availableGeometry().width();
    int screen_h = QApplication::desktop()->availableGeometry().height();
    LOG("resolution %d x %d\n", screen_w, screen_h);
#if defined(Q_OS_ANDROID)
    {
        qDebug("setting up fonts for Android");
        QFont new_font = window->font();
        // n.b., need to set font size directly, as new_font.pointSize() returns -1 on Android!
        if( screen_w <= 480 ) {
            // optimise for smaller screens
            this->font_scene = new_font;
            this->font_small = QFont(new_font);
            this->font_small.setPointSize(5);
            this->font_std = QFont(new_font);
            this->font_std.setPointSize(7);
            this->font_big = QFont(new_font);
            this->font_big.setPointSize(8);
        }
        else {
            this->font_scene = new_font;
            this->font_small = QFont(new_font);
            this->font_small.setPointSize(7);
            this->font_std = QFont(new_font);
            this->font_std.setPointSize(9);
            this->font_big = QFont(new_font);
            this->font_big.setPointSize(13);
        }

#ifdef USING_WEBKIT
        web_settings->setFontFamily(QWebSettings::StandardFont, font_std.family());
        web_settings->setFontSize(QWebSettings::DefaultFontSize, font_std.pointSize() + 20);
        web_settings->setFontSize(QWebSettings::DefaultFixedFontSize, font_std.pointSize() + 20);
#endif
    }
#else
    if( smallscreen_c ) {
        QFont new_font = window->font();
        qDebug("setting up fonts for non-Android smallscreen");
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
    }
    else {
        qDebug("setting up fonts for non-Android non-mobile");
        LOG("default font size: %d\n", window->font().pointSize());
        QString font_family = window->font().family();
        LOG("font family: %s\n", font_family.toStdString().c_str());
        this->font_scene = QFont(font_family, 16, QFont::Normal);
        this->font_small = QFont(font_family, 16, QFont::Normal);
        this->font_std = QFont(font_family, 18, QFont::Normal);
        this->font_big = QFont(font_family, 24, QFont::Bold);

#ifdef USING_WEBKIT
        web_settings->setFontFamily(QWebSettings::StandardFont, font_std.family());
        web_settings->setFontSize(QWebSettings::DefaultFontSize, font_std.pointSize());
        web_settings->setFontSize(QWebSettings::DefaultFixedFontSize, font_std.pointSize());
#endif
    }
#endif

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
}

#ifdef USING_WEBKIT
void Game::setWebView(QWebView *webView) {
#ifdef Q_OS_ANDROID
    // scrollbars need to be explicitly disabled, as otherwise we get a space being left, even though scrollbars don't show
    webView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    webView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#endif
    this->webViewEventFilter->setWebView(webView);
}
#else
void Game::setTextEdit(QTextEdit *textEdit) {
    // need to set colours for Android at least
    QPalette p = textEdit->palette();
    p.setColor(QPalette::Base, Qt::white);
    p.setColor(QPalette::Text, Qt::black);
    textEdit->setPalette(p);
    textEdit->setReadOnly(true);
    textEdit->setTextInteractionFlags(Qt::NoTextInteraction);
    //textEdit->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    //textEdit->setFont(this->getFontSmall());
    QFont font = this->getFontSmall();
    if( !smallscreen_c ) {
        // font seems slightly too large, at least for Windows
        font.setPointSizeF(font.pointSizeF() - 4.5f);
    }
    textEdit->setFont(font);
#ifdef Q_OS_ANDROID
    // scrollbars need to be explicitly disabled, as otherwise we get a space being left, even though scrollbars don't show
    textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#endif
    this->webViewEventFilter->setTextEdit(textEdit);
}
#endif

void Game::resizeTopLevelWidget(QWidget *widget) const {
    int width = QApplication::desktop()->availableGeometry().width();
    int height = QApplication::desktop()->availableGeometry().height();
    widget->setMinimumWidth(width/2);
    widget->setMinimumHeight(height/2);
}

void Game::run(bool fullscreen) {

    this->init(fullscreen);

    try {
        gamestate = new OptionsGamestate();
    }
    catch(string &error) {
        LOG("exception creating initial gamestate: %s\n", error.c_str());
        this->getScreen()->getMainWindow()->unsetCursor();

        stringstream str;
        str << "Failed to start game:\n" << error;
        game_g->showErrorDialog(str.str());
        // can't use qApp->quit() here, as not yet in main event loop
        return;
    }
    catch(...) {
        LOG("unexpected exception creating initial gamestate\n");
        this->getScreen()->getMainWindow()->unsetCursor();

        stringstream str;
        str << "Unexpected error starting game!\n";
        game_g->showErrorDialog(str.str());
        // can't use qApp->quit() here, as not yet in main event loop
        return;
    }

    screen->runMainLoop();

    delete gamestate;
    gamestate = NULL;
}

enum TestID {
    TEST_PATHFINDING_0 = 0,
    TEST_PATHFINDING_1 = 1,
    TEST_PATHFINDING_2 = 2,
    TEST_PATHFINDING_3 = 3,
    TEST_PATHFINDING_4 = 4,
    TEST_PATHFINDING_5 = 5,
    TEST_PATHFINDING_6 = 6,
    TEST_POINTINPOLYGON_0 = 7,
    TEST_POINTINPOLYGON_1 = 8,
    TEST_POINTINPOLYGON_2 = 9,
    TEST_POINTINPOLYGON_3 = 10,
    TEST_POINTINPOLYGON_4 = 11,
    TEST_POINTINPOLYGON_5 = 12,
    TEST_POINTINPOLYGON_6 = 13,
    TEST_POINTINPOLYGON_7 = 14,
    TEST_POINTINPOLYGON_8 = 15,
    TEST_POINTINPOLYGON_9 = 16,
    TEST_POINTINPOLYGON_10 = 17,
    TEST_POINTINPOLYGON_11 = 18,
    TEST_PERF_POINTINPOLYGON_0 = 19,
    TEST_PERF_POINTINPOLYGON_1 = 20,
    TEST_PERF_POINTINPOLYGON_2 = 21,
    TEST_PERF_DISTANCEGRAPH_0 = 22,
    TEST_PERF_PATHFINDING_0 = 23,
    TEST_PERF_REMOVE_SCENERY_0 = 24,
    TEST_PERF_REMOVE_SCENERY_1 = 25,
    TEST_PERF_REMOVE_SCENERY_2 = 26,
    TEST_PERF_UPDATE_VISIBILITY_0 = 27,
    TEST_PERF_NUDGE_0 = 28,
    TEST_PERF_NUDGE_1 = 29,
    TEST_PERF_NUDGE_2 = 30,
    TEST_PERF_NUDGE_3 = 31,
    TEST_PERF_NUDGE_4 = 32,
    TEST_PERF_NUDGE_5 = 33,
    TEST_PERF_NUDGE_6 = 34,
    TEST_PERF_NUDGE_7 = 35,
    TEST_PERF_NUDGE_8 = 36,
    TEST_PERF_NUDGE_9 = 37,
    TEST_PERF_NUDGE_10 = 38,
    TEST_PERF_NUDGE_11 = 39,
    TEST_PERF_NUDGE_12 = 40,
    TEST_PERF_NUDGE_13 = 41,
    TEST_PERF_NUDGE_14 = 42,
    TEST_LOADSAVEQUEST_0 = 43,
    TEST_LOADSAVEQUEST_1 = 44,
    TEST_LOADSAVEQUEST_2 = 45,
    TEST_LOADSAVERANDOMQUEST_0 = 46,
    N_TESTS = 47
};

/**
  TEST_PATHFINDING_0 - find a path
  TEST_PATHFINDING_1 - find a path
  TEST_PATHFINDING_2 - can't find a path as no route available
  TEST_PATHFINDING_3 - can't find a path, as start isn't in valid floor region
  TEST_PATHFINDING_4 - can't find a path, as destination isn't in valid floor region
  TEST_PATHFINDING_5 - find a path, with scenery being ignored
  TEST_PATHFINDING_6 - can't find a path - scenery is ignored but still blocking, as it doesn't block the last segment
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
  TEST_LOADSAVEQUEST_n - tests that we can load the nth quest, then test saving, then test loading the save game
  TEST_LOADSAVERANDOMQUEST_0 - tests that we can create a random quest, then test saving, then test loading the save game
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
        if( test_id == TEST_PATHFINDING_0 || test_id == TEST_PATHFINDING_1 || test_id == TEST_PATHFINDING_2 || test_id == TEST_PATHFINDING_3 || test_id == TEST_PATHFINDING_4 || test_id == TEST_PATHFINDING_5 || test_id == TEST_PATHFINDING_6 ) {
            Location location("");

            FloorRegion *floor_region = NULL;
            if( test_id == TEST_PATHFINDING_5 || test_id == TEST_PATHFINDING_6 ) {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 10.0f);
                location.addFloorRegion(floor_region);
            }
            else {
                floor_region = FloorRegion::createRectangle(0.0f, 0.0f, 5.0f, 5.0f);
                location.addFloorRegion(floor_region);
                if( test_id != TEST_PATHFINDING_2 ) {
                    FloorRegion *floor_region = FloorRegion::createRectangle(10.0f, 1.0f, 4.0f, 3.0f);
                    location.addFloorRegion(floor_region);
                }
                floor_region = FloorRegion::createRectangle(5.0f, 3.0f, 5.0f, 1.0f);
                location.addFloorRegion(floor_region);
            }

            void *ignore = NULL;
            Scenery *scenery = NULL;
            if( test_id == TEST_PATHFINDING_5 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 2.0f);
                ignore = scenery;

                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 4.0f);
            }
            else if( test_id == TEST_PATHFINDING_6 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 2.0f);
                ignore = scenery;

                scenery = new Scenery("", "", 1.0f, 8.0f, 8.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 1.5f, 5.5f);
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
            else if( test_id == TEST_PATHFINDING_5 ) {
                src = Vector2D(2.5f, 6.0f);
                dest = Vector2D(2.5f, 2.0f);
                expected_points = 3;
            }
            else if( test_id == TEST_PATHFINDING_6 ) {
                src = Vector2D(2.5f, 6.0f);
                dest = Vector2D(1.5f, 1.0f);
                expected_points = 3;
            }
            else {
                throw string("missing case for this test_id");
            }
            vector<Vector2D> path = location.calculatePathTo(src, dest, ignore, false);

            LOG("path has %d points\n", path.size());
            for(size_t i=0;i<path.size();i++) {
                Vector2D point = path.at(i);
                LOG("    %d : %f, %f\n", i, point.x, point.y);
            }

            if( path.size() != expected_points ) {
                throw string("Unexpected number of path points");
            }
            else if( ignore == NULL && path.size() > 0 && path.at( path.size()-1 ) != dest ) {
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
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 5.5f, 22.5f);
            }
            else if( test_id == TEST_PERF_REMOVE_SCENERY_1 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 4.25f, 22.5f);
            }
            else if( test_id == TEST_PERF_REMOVE_SCENERY_2 ) {
                scenery = new Scenery("", "", 0.1f, 1.0f, 1.0f, false, 0.0f);
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
                scenery = new Scenery("", "", 0.1f, 1.0f, 1.0f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 4.95f, 2.5f);
            }
            else if( test_id == TEST_PERF_NUDGE_2 || test_id == TEST_PERF_NUDGE_3 || test_id == TEST_PERF_NUDGE_6 || test_id == TEST_PERF_NUDGE_7 ) {
                scenery = new Scenery("", "", 1.0f, 0.1f, 0.9f, false, 0.0f);
                scenery->setBlocking(true, true);
                location.addScenery(scenery, 2.5f, 4.95f);
            }
            else if( test_id == TEST_PERF_NUDGE_12 || test_id == TEST_PERF_NUDGE_13 ) {
                scenery = new Scenery("", "", 1.0f, 1.0f, 1.0f, false, 0.0f);
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
        else if( test_id == TEST_LOADSAVEQUEST_0 || test_id == TEST_LOADSAVEQUEST_1 || test_id == TEST_LOADSAVEQUEST_2 ) {
            QElapsedTimer timer;
            timer.start();
            PlayingGamestate *playing_gamestate = new PlayingGamestate(false, GAMETYPE_CAMPAIGN, "Warrior", "name", false, false, 0);
            gamestate = playing_gamestate;

            QString qt_filename;
            if( test_id == TEST_LOADSAVEQUEST_0 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_kill_goblins.xml");
            }
            else if( test_id == TEST_LOADSAVEQUEST_1 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_wizard_dungeon_find_item.xml");
            }
            else if( test_id == TEST_LOADSAVEQUEST_2 ) {
                qt_filename = DEPLOYMENT_PATH + QString("data/quest_through_mountains.xml");
            }

            playing_gamestate->loadQuest(qt_filename, false);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            QString filename = "EREBUSTEST_" + QString::number(test_id - (int)TEST_LOADSAVEQUEST_0) + ".xml";
            LOG("try saving as %s\n", filename.toStdString().c_str());
            if( !playing_gamestate->saveGame(filename, false) ) {
                throw string("failed to save game");
            }

            delete gamestate;
            gamestate = NULL;

            QString full_filename = this->getApplicationFilename(savegame_folder + filename);
            LOG("now try loading %s\n", full_filename.toStdString().c_str());
            playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            gamestate = playing_gamestate;
            playing_gamestate->loadQuest(full_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_CAMPAIGN ) {
                throw string("expected GAMETYPE_CAMPAIGN");
            }

            delete gamestate;
            gamestate = NULL;

            has_score = true;
            score = ((double)timer.elapsed());
            score /= 1000.0;
        }
        else if( test_id == TEST_LOADSAVERANDOMQUEST_0 ) {
            QElapsedTimer timer;
            timer.start();
            PlayingGamestate *playing_gamestate = new PlayingGamestate(false, GAMETYPE_RANDOM, "Warrior", "name", false, false, 0);
            gamestate = playing_gamestate;

            playing_gamestate->createRandomQuest();
            if( playing_gamestate->getGameType() != GAMETYPE_RANDOM ) {
                throw string("expected GAMETYPE_RANDOM");
            }

            QString filename = "EREBUSTEST_RANDOM.xml";
            LOG("try saving as %s\n", filename.toStdString().c_str());
            if( !playing_gamestate->saveGame(filename, false) ) {
                throw string("failed to save game");
            }

            delete gamestate;
            gamestate = NULL;

            QString full_filename = this->getApplicationFilename(savegame_folder + filename);
            LOG("now try loading %s\n", full_filename.toStdString().c_str());
            playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
            // n.b., use GAMETYPE_CAMPAIGN as we're loading a game
            gamestate = playing_gamestate;
            playing_gamestate->loadQuest(full_filename, true);
            if( playing_gamestate->getGameType() != GAMETYPE_RANDOM ) {
                throw string("expected GAMETYPE_RANDOM");
            }

            delete gamestate;
            gamestate = NULL;

            has_score = true;
            score = ((double)timer.elapsed());
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

    this->init(true); // some tests need a Screen etc
    for(int i=0;i<N_TESTS;i++) {
        runTest(filename, i);
    }
    //runTest(filename, ::TEST_LOADSAVEQUEST_1);
}

void Game::initButton(QWidget *button) const {
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
                PlayingGamestate *playing_gamestate = new PlayingGamestate(false, start_message->getGametype(), start_message->getPlayerType(), start_message->getName(), start_message->getPermadeath(), start_message->getCheatMode(), start_message->getCheatStartLevel());
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
                PlayingGamestate *playing_gamestate = new PlayingGamestate(true, GAMETYPE_CAMPAIGN, "", "", false, false, 0);
                gamestate = playing_gamestate;
                LoadGameMessage *load_message = static_cast<LoadGameMessage *>(message);
                QString full_filename = this->getApplicationFilename(savegame_folder + load_message->getFilename());
                try {
                    playing_gamestate->loadQuest(full_filename, true);
                    this->getScreen()->getMainWindow()->unsetCursor();
                }
                catch(string &error) {
                    LOG("exception creating new gamestate when loading: %s\n", error.c_str());
                    this->getScreen()->getMainWindow()->unsetCursor();
                    stringstream str;
                    str << "Failed to load save game file:\n" << error;
#ifdef Q_OS_ANDROID
                    // also copy failed save game to sdcard on Android, so user can potentially access it to send it to me
                    this->exportFilenameToSDCard(full_filename, load_message->getFilename());
#endif
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
                LOG("deleting gamestate");
                delete gamestate;
                gamestate = NULL;
            }

            stringstream str;
            str << "Failed to load game data:\n" << error;
            game_g->showErrorDialog(str.str());
            qApp->quit();
        }
        catch(...) {
            LOG("unexpected exception creating new gamestate\n");
            this->getScreen()->getMainWindow()->unsetCursor();
            if( gamestate != NULL ) {
                LOG("deleting gamestate");
                delete gamestate;
                gamestate = NULL;
            }

            stringstream str;
            str << "Unexpected error loading game data!\n";
            game_g->showErrorDialog(str.str());
            qApp->quit();
        }
        delete message;
    }
}

void Game::update() {
    this->handleMessages(); // needed to process any messages from earlier update call
    if( this->current_stream_sound_effect.length() > 0 ) {
#ifndef Q_OS_ANDROID
        Sound *current_sound = this->sound_effects[this->current_stream_sound_effect];
        ASSERT_LOGGER(current_sound != NULL);
        if( current_sound != NULL ) {
            current_sound->update();
        }
#endif
    }
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

QString Game::getFilename(const QString &path, const QString &name) const {
    // not safe to use LOG here, as logfile may not have been initialised!
    QString pathQt = path + QString("/") + name;
    QString nativePath(QDir::toNativeSeparators(pathQt));
    qDebug("getFilename returns: %s", nativePath.toUtf8().data());
    return nativePath;
}

QString Game::getApplicationFilename(const QString &name) const {
    // not safe to use LOG here, as logfile may not have been initialised!
    return getFilename(application_path, name);
}

#ifdef Q_OS_ANDROID
int Game::importFilesToSDCard() const {
    int count = 0;
    if( this->sdcard_ok ) {
        QDir dir( sdcard_path );
        QStringList filter;
        filter << "*" + savegame_ext;
        QFileInfoList files = dir.entryInfoList(filter, QDir::Files, QDir::Time);
        for(size_t i=0;i<files.size();i++) {
            QFileInfo file_info = files.at(i);
            QString full_filename = this->getApplicationFilename(savegame_folder + file_info.fileName());
            QFile file(full_filename);
            file.remove(); // in case already present
            qDebug("copy from %s to %s", file_info.filePath().toStdString().c_str(), full_filename.toStdString().c_str());
            QFile::copy(file_info.filePath(), full_filename);
            count++;
        }
    }
    return count;
}

void Game::exportFilenameToSDCard(const QString &src_fullfilename, const QString &filename) const {
    if( this->sdcard_ok ) {
        qDebug("Game::exportFilenameToSDCard(%s, %s)", src_fullfilename.toStdString().c_str(), filename.toStdString().c_str());
        QFile dir_file(this->getFilename(sdcard_path, filename));
        dir_file.remove(); // in case already present
        QFile::copy(src_fullfilename, dir_file.fileName());
    }
}
#endif

void Game::log(const char *text, va_list vlist) {
    char buffer[65536] = "";
    vsprintf(buffer,text,vlist);

    if( logfilename.length() > 0 ) {
        QFile logfile(logfilename);
        if( logfile.open(QIODevice::Append | QIODevice::Text) ) {
            QTextStream stream(&logfile);
            stream << buffer;
        }
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

void Game::loadSound(const string &id, const string &filename, bool stream) {
    LOG("load sound: %s : %s , %d\n", id.c_str(), filename.c_str(), stream);
#ifndef Q_OS_ANDROID
    try {
        this->sound_effects[id] = new Sound(filename, stream);
    }
    catch(const string &str) {
        LOG("Error when loading %s\n", filename.c_str());
        LOG("%s\n", str.c_str());
    }
#else
    Sound *sound = NULL;
    AndroidSoundEffect *android_sound_effect = androidAudio.loadSoundEffect(filename.c_str());
    if( android_sound_effect != NULL ) {
        AndroidSound *android_sound = new AndroidSound();
        if( android_sound->setSoundEffect(&androidAudio, android_sound_effect) ) {
            sound = new Sound(android_sound, android_sound_effect);
        }
        else {
            delete android_sound;
            delete android_sound_effect;
        }
    }
    if( sound != NULL ) {
        this->sound_effects[id] = sound;
    }
#endif
}

void Game::playSound(const string &sound_effect, bool loop) {
    //qDebug("play sound: %s\n", sound_effect.c_str());
    //if( game_g->isSoundEnabled() )
    {
        Sound *sound = this->sound_effects[sound_effect];
        if( sound != NULL ) {
#ifndef Q_OS_ANDROID
            if( sound->isStream() ) {
                this->cancelCurrentStream();
            }
            sound->play(loop);
            if( sound->isStream() ) {
                this->current_stream_sound_effect = sound_effect;
            }
#else
            //androidAudio.playSound(sound->getAndroidSound(), loop);
            sound->getAndroidSound()->playSound(loop);
#endif
        }
    }
}

void Game::pauseSound(const string &sound_effect) {
#ifndef Q_OS_ANDROID
    qDebug("pause sound: %s\n", sound_effect.c_str());
    // n.b., need to allow pausing even if sound not enabled (so we can disable sound, then pause music)
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        sound->pause();
    }
#endif
    // not yet supported on Android
}

void Game::stopSound(const string &sound_effect) {
#ifndef Q_OS_ANDROID
    qDebug("stop sound: %s\n", sound_effect.c_str());
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        sound->stop();
    }
#endif
    // not yet supported on Android
    if( current_stream_sound_effect == sound_effect ) {
        this->current_stream_sound_effect = "";
    }
}

void Game::fadeSound(const string &sound_effect) {
#ifndef Q_OS_ANDROID
    qDebug("fade sound: %s\n", sound_effect.c_str());
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        sound->fadeOut(2000);
    }
#endif
    // not yet supported on Android
}

void Game::cancelCurrentStream() {
#ifndef Q_OS_ANDROID
    if( this->current_stream_sound_effect.length() > 0 ) {
        // cancel current stream
        qDebug("cancel stream: %s", this->current_stream_sound_effect.c_str());
        Sound *current_sound = this->sound_effects[this->current_stream_sound_effect];
        ASSERT_LOGGER(current_sound != NULL);
        if( current_sound != NULL ) {
            current_sound->stop();
        }
        this->current_stream_sound_effect = "";
    }
#endif
    // not yet supported on Android
}

void Game::freeSound(const string &sound_effect) {
    qDebug("Game::freeSound(%s)", sound_effect.c_str());
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        this->sound_effects.erase( this->sound_effects.find(sound_effect) );
        delete sound;
    }
    if( current_stream_sound_effect == sound_effect ) {
        this->current_stream_sound_effect = "";
    }
}

void Game::updateSoundVolume(const string &sound_effect) {
#ifndef Q_OS_ANDROID
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        sound->updateVolume();
    }
#endif
    // not yet supported on Android
}

void Game::setSoundVolume(const string &sound_effect, float volume) {
#ifndef Q_OS_ANDROID
    Sound *sound = this->sound_effects[sound_effect];
    if( sound != NULL ) {
        sound->setVolume(volume);
    }
#endif
    // not yet supported on Android
}

/*void Game::setSoundEnabled(bool sound_enabled) {
    this->sound_enabled = sound_enabled;
    this->settings->setValue(sound_enabled_key_c, sound_enabled ? 1 : 0);
}*/

#ifndef Q_OS_ANDROID
void Game::setGlobalSoundVolume(int sound_volume) {
    this->sound_volume = sound_volume;
    this->settings->setValue(sound_volume_key_c, sound_volume);
}

void Game::setGlobalSoundVolumeMusic(int sound_volume_music) {
    this->sound_volume_music = sound_volume_music;
    this->settings->setValue(sound_volume_music_key_c, sound_volume_music);
}

void Game::setGlobalSoundVolumeEffects(int sound_volume_effects) {
    this->sound_volume_effects = sound_volume_effects;
    this->settings->setValue(sound_volume_effects_key_c, sound_volume_effects);
}
#endif

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

Character *Game::createPlayer(const string &player_type, const string &player_name) const {
    //ASSERT_LOGGER(i < this->getNPlayerTypes() );
    //Character *character = new Character(player_types.at(i), "", false);
    Character *character = new Character(player_name, "", false);
    if( player_type == "Barbarian" ) {
        character->initialiseProfile(1, 8, 6, 8, 1, 5, 5, 8, 1.8f);
        character->initialiseHealth(75);
        character->setBiography("");
        character->setPortrait("portrait_barbarian");
    }
    else if( player_type == "Elf" ) {
        character->initialiseProfile(1, 7, 8, 6, 1, 8, 7, 8, 2.25f);
        character->initialiseHealth(60);
        character->setBiography("");
        character->setPortrait("portrait_elf");
    }
    else if( player_type == "Halfling" ) {
        character->initialiseProfile(1, 7, 7, 5, 1, 7, 9, 7, 1.8f);
        character->initialiseHealth(50);
        character->setBiography("");
        character->setPortrait("portrait_halfling");
    }
    else if( player_type == "Ranger" ) {
        character->initialiseProfile(1, 7, 8, 7, 1, 7, 8, 6, 2.2f);
        character->initialiseHealth(60);
        character->setBiography("");
        character->setPortrait("portrait_ranger");
    }
    else if( player_type == "Warrior" ) {
        character->initialiseProfile(1, 8, 7, 7, 1, 6, 7, 7, 2.0f);
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

void Game::activate(bool active) {
    this->getScreen()->enableUpdateTimer(active);
    bool newly_paused = false;
    if( !active ) {
        newly_paused = !this->getScreen()->isPaused();
        this->getScreen()->setPaused(true, false); // automatically pause when application goes inactive
#ifndef Q_OS_ANDROID
        if( this->current_stream_sound_effect.length() > 0 ) {
            // pause current stream
            qDebug("pause stream: %s", this->current_stream_sound_effect.c_str());
            Sound *current_sound = this->sound_effects[this->current_stream_sound_effect];
            ASSERT_LOGGER(current_sound != NULL);
            if( current_sound != NULL ) {
                current_sound->pause();
            }
        }
#endif
    }
    else {
#ifndef Q_OS_ANDROID
        if( this->current_stream_sound_effect.length() > 0 ) {
            // unpause current stream
            qDebug("unpause stream: %s", this->current_stream_sound_effect.c_str());
            Sound *current_sound = this->sound_effects[this->current_stream_sound_effect];
            ASSERT_LOGGER(current_sound != NULL);
            if( current_sound != NULL ) {
                current_sound->unpause();
            }
        }
#endif
    }
    if( this->gamestate != NULL ) {
        this->gamestate->activate(active, newly_paused);
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
            if( !smallscreen_c ) {
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
