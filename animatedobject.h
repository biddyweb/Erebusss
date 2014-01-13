#pragma once

#include <vector>
using std::vector;

#include <string>
using std::string;

#include <map>
using std::map;

#include <QGraphicsItem>

#include "common.h"

class AnimationSet {
public:
    enum AnimationType {
        ANIMATIONTYPE_LOOP   = 0,
        ANIMATIONTYPE_BOUNCE = 1,
        ANIMATIONTYPE_SINGLE = 2
    };

protected:
    AnimationType animation_type;
    unsigned int n_dimensions;
    size_t n_frames;
    int ms_per_frame;
    vector<QPixmap> pixmaps; // vector of length n_dimensions * n_frames
    /*QRectF bounding_rect;

    virtual QRectF boundingRect() const;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);*/
public:
    AnimationSet(AnimationType animation_type, int ms_per_frame, unsigned int n_dimensions, size_t n_frames, vector<QPixmap> pixmaps); // pixmaps array of length n_dimensions * n_frames
    virtual ~AnimationSet();

    /*size_t getNFrames() const {
        return this->n_frames;
    }*/
    const QPixmap &getFrame(unsigned int c_dimension, size_t c_frame) const;
    int getMSPerFrame() const {
        return this->ms_per_frame;
    }

    static AnimationSet *create(const QPixmap &image, AnimationType animation_type, int ms_per_frame, int stride_x, int stride_y, int x_offset, unsigned int n_dimensions, size_t n_frames, int icon_off_x, int icon_off_y, int icon_width, int icon_height);
};

/* Helper class used to define animation image formats, when loading in the
 * animation frames.
 */
class AnimationLayerDefinition {
    friend class AnimationLayer;
    string name;
    int position;
    size_t n_frames;
    AnimationSet::AnimationType animation_type;
    int ms_per_frame;
public:
    AnimationLayerDefinition(string name, int position, size_t n_frames, AnimationSet::AnimationType animation_type, int ms_per_frame) :
        name(name), position(position), n_frames(n_frames), animation_type(animation_type), ms_per_frame(ms_per_frame) {
    }
};

class AnimationLayer {
    map<string, const AnimationSet *> animation_sets;
    int width, height; // size of each frame image in pixels
public:
    AnimationLayer(int width, int height) : width(width), height(height) {
    }
    ~AnimationLayer();

    void addAnimationSet(const string &name, const AnimationSet *animation_set) {
        this->animation_sets[name] = animation_set;
    }
    const AnimationSet *getAnimationSet(const string &name) const {
        map<string, const AnimationSet *>::const_iterator iter = this->animation_sets.find(name);
        if( iter == this->animation_sets.end() )
            return NULL;
        return iter->second;
    }
    int getWidth() const {
        return this->width;
    }
    int getHeight() const {
        return this->height;
    }

    static AnimationLayer *create(const QPixmap &image, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions);
    static AnimationLayer *create(const string &filename, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions);
};

class LazyAnimationLayer {
    AnimationLayer *animation_layer;
    // if animation_layer == NULL, then we also store the information to load it when required:
    string filename;
    vector<AnimationLayerDefinition> animation_layer_definitions;
    bool clip; // must be true for animations
    int off_x;
    int off_y;
    int width;
    int height;
    int stride_x;
    int stride_y;
    int expected_total_width;
    unsigned int n_dimensions;
public:
    LazyAnimationLayer(AnimationLayer *animation_layer) :
        animation_layer(animation_layer), clip(false), off_x(0), off_y(0), width(0), height(0), stride_x(0), stride_y(0), expected_total_width(0), n_dimensions(0)
    {
    }
    LazyAnimationLayer(const string &filename, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions) :
        animation_layer(NULL), filename(filename), animation_layer_definitions(animation_layer_definitions), clip(clip), off_x(off_x), off_y(off_y), width(width), height(height), stride_x(stride_x), stride_y(stride_y), expected_total_width(expected_total_width), n_dimensions(n_dimensions)
    {
    }
    LazyAnimationLayer(const QPixmap &pixmap, const vector<AnimationLayerDefinition> &animation_layer_definitions, bool clip, int off_x, int off_y, int width, int height, int stride_x, int stride_y, int expected_total_width, unsigned int n_dimensions);

    ~LazyAnimationLayer();

    AnimationLayer *getAnimationLayer();
};

class AnimatedObject : public QGraphicsItem {
    vector<AnimationLayer *> animation_layers;
    bool set_c_animation_name;
    string c_animation_name;
    vector<const AnimationSet *> c_animation_sets;
    unsigned int c_dimension;
    size_t c_frame;
    int animation_time_start_ms;
    bool bounce; // whether to bounce up and down
    bool clip; // whether to only draw a portion
    float clip_sx, clip_sy, clip_sw, clip_sh;

    virtual void advance(int phase);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

public:
    explicit AnimatedObject(QGraphicsItem *parent = 0);
    virtual ~AnimatedObject();

    virtual QRectF boundingRect() const;

    void addAnimationLayer(AnimationLayer *animation_layer);
    void clearAnimationLayers();
    void setAnimationSet(const string &name, bool force_restart);
    void setDimension(unsigned int c_dimension);
    int getWidth() const;
    int getHeight() const;
    void setBounce(bool bounce) {
        this->bounce = bounce;
    }
    void setClip(float sx, float sy, float sw, float sh) {
        this->clip = true;
        this->clip_sx = sx;
        this->clip_sy = sy;
        this->clip_sw = sw;
        this->clip_sh = sh;
    }
};
