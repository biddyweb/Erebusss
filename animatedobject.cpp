#ifdef _DEBUG
#include <cassert>
#endif

#if QT_VERSION < 0x050000
#include <qmath.h>
#endif

#include <QPainter>

#include "animatedobject.h"
#include "logiface.h"
#include "game.h"

AnimationSet::AnimationSet(AnimationType animation_type, int ms_per_frame, unsigned int n_dimensions, size_t n_frames, vector<QPixmap> pixmaps) : animation_type(animation_type), n_dimensions(n_dimensions), n_frames(n_frames), ms_per_frame(ms_per_frame), pixmaps(pixmaps) {
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

AnimationSet *AnimationSet::create(const QPixmap &image, AnimationType animation_type, int ms_per_frame, int stride_x, int stride_y, int x_offset, unsigned int n_dimensions, size_t n_frames, int icon_off_x, int icon_off_y, int icon_width, int icon_height) {
    //qDebug("### %d x %d\n", icon_width, icon_height);
    vector<QPixmap> frames;
    for(unsigned int i=0;i<n_dimensions;i++) {
        for(size_t j=0;j<n_frames;j++) {
            int xpos = stride_x*(x_offset+j) + icon_off_x;
            int ypos = stride_y*i + icon_off_y;
            frames.push_back( image.copy(xpos, ypos, icon_width, icon_height));
        }
    }
    AnimationSet *animation_set = new AnimationSet(animation_type, ms_per_frame, n_dimensions, n_frames, frames);
    return animation_set;
}

int AnimationSet::getMemorySize() const {
    int memory_size = 0;
    foreach(const QPixmap &pixmap, pixmaps) {
        int pixmap_size = pixmap.width() * pixmap.height() * 4;
        memory_size += pixmap_size;
    }
    return memory_size;
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
        AnimationSet *animation_set = AnimationSet::create(image, animation_layer_definition.animation_type, animation_layer_definition.ms_per_frame, stride_x, stride_y, animation_layer_definition.position, n_dimensions, animation_layer_definition.n_frames, off_x, off_y, width, height);
        layer->addAnimationSet(animation_layer_definition.name, animation_set);
    }
    qDebug("    done");
    return layer;
}

AnimationLayer *AnimationLayer::create(const string &filename, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions) {
    QPixmap image = game_g->loadImage(filename.c_str());
    return create(image, animation_layer_definitions, clip, off_x, off_y, width, height,stride_x, stride_y, expected_total_width, n_dimensions);
}

int AnimationLayer::getMemorySize() const {
    int memory_size = 0;
    for(map<string, const AnimationSet *>::const_iterator iter = this->animation_sets.begin(); iter != this->animation_sets.end(); ++iter) {
        const AnimationSet *animation_set = iter->second;
        memory_size += animation_set->getMemorySize();
    }
    return memory_size;
}

LazyAnimationLayer::LazyAnimationLayer(const QPixmap &pixmap, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions) :
    animation_layer(NULL), clip(false), off_x(0), off_y(0), width(0), height(0), stride_x(0), stride_y(0), expected_total_width(0), n_dimensions(0)
{
    // pixmap already supplied, so we load straight away
    this->animation_layer = AnimationLayer::create(pixmap, animation_layer_definitions, clip, off_x, off_y, width, height, stride_x, stride_y, expected_total_width, n_dimensions);
}

LazyAnimationLayer::~LazyAnimationLayer() {
    if( this->animation_layer != NULL ) {
        delete animation_layer;
    }
}

AnimationLayer *LazyAnimationLayer::getAnimationLayer() {
    if( this->animation_layer == NULL ) {
        LOG("lazily load animation layer from: %s\n", this->filename.c_str());
        this->animation_layer = AnimationLayer::create(filename, animation_layer_definitions, clip, off_x, off_y, width, height, stride_x, stride_y, expected_total_width, n_dimensions);
        if( this->animation_layer == NULL ) {
            throw string("failed to load animation layer from: " + this->filename);
        }
        LOG("    done\n");
    }
    return this->animation_layer;
}

int LazyAnimationLayer::getMemorySize() const {
    int memory_size = 0;
    if( this->animation_layer != NULL )
        memory_size = this->animation_layer->getMemorySize();
    return memory_size;
}

AnimatedObject::AnimatedObject(QGraphicsItem *parent) : /*animation_layer(NULL), c_animation_set(NULL),*/
    QGraphicsItem(parent), set_c_animation_name(false), c_dimension(0), c_frame(0), animation_time_start_ms(0), bounce(false),
    clip(false), clip_sx(0), clip_sy(0), clip_sw(0), clip_sh(0)
{
}

AnimatedObject::~AnimatedObject() {
}

void AnimatedObject::advance(int phase) {
    //qDebug("AnimatedObject::advance() phase %d", phase);
    if( c_animation_sets.size() == 0 )
        return;
    if( phase == 1 ) {
        const AnimationSet *animation_set = c_animation_sets.at(0);
        int ms_per_frame = animation_set->getMSPerFrame();
        //int time_elapsed_ms = game_g->getScreen()->getElapsedMS() - animation_time_start_ms;
        int time_elapsed_ms = game_g->getGameTimeTotalMS() - animation_time_start_ms;
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
        int time_elapsed_ms = game_g->getGameTimeTotalMS() - animation_time_start_ms;
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
        animation_time_start_ms = game_g->getGameTimeTotalMS();
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
