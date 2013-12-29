#pragma once

#include <set>
using std::set;

#include <QGraphicsView>
#include <QGraphicsTextItem>
#include <QElapsedTimer>

#include "common.h"

#include "rpg/utils.h" // for Vector2D

class PlayingGamestate;
class MainGraphicsView;

class TextEffect : public QGraphicsTextItem {
    int time_expire;
    MainGraphicsView *view;

    virtual void advance(int phase);
public:
    TextEffect(MainGraphicsView *view, const QString &text, int duration_ms, const QColor &color);
    virtual ~TextEffect() {
    }
};

class GUIOverlay;

class MainGraphicsView : public QGraphicsView {
    Q_OBJECT

public:
    enum KeyCode {
        KEY_L = 0,
        KEY_R = 1,
        KEY_U = 2,
        KEY_D = 3,
        KEY_LU = 4,
        KEY_RU = 5,
        KEY_LD = 6,
        KEY_RD = 7,
        N_KEYS = 8
    };

private:
    PlayingGamestate *playing_gamestate;
    int mouse_down_x, mouse_down_y;
    bool single_left_mouse_down; // if left mouse button is down, and not a multitouch operation
    bool has_last_mouse;
    int last_mouse_x, last_mouse_y;
    //float kinetic_scroll_x, kinetic_scroll_y;
    bool has_kinetic_scroll;
    Vector2D kinetic_scroll_dir;
    float kinetic_scroll_speed;
    //QGraphicsProxyWidget *gui_overlay_item;
    GUIOverlay *gui_overlay;
    float c_scale;
    set<TextEffect *> text_effects;
    bool calculated_lighting_pixmap;
    QPixmap lighting_pixmap;
    bool calculated_lighting_pixmap_scaled;
    int lasttime_calculated_lighting_pixmap_scaled_ms;
    QPixmap lighting_pixmap_scaled;
    unsigned char darkness_alpha;
    int fps_frame_count;
    QElapsedTimer fps_timer;

    bool key_down[N_KEYS];

    bool handleKey(const QKeyEvent *event, bool down);

    const static float min_zoom_c;
    const static float max_zoom_c;

    void zoom(QPointF zoom_centre, bool in);

    virtual bool viewportEvent(QEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);
    virtual void resizeEvent(QResizeEvent *event);
    virtual void paintEvent(QPaintEvent *event);

public:
    explicit MainGraphicsView(PlayingGamestate *playing_gamestate, QGraphicsScene *scene, QWidget *parent);
    virtual ~MainGraphicsView() {
    }

    void setGUIOverlay(/*QGraphicsProxyWidget *gui_overlay_item, */GUIOverlay *gui_overlay) {
        //this->gui_overlay_item = gui_overlay_item;
        this->gui_overlay = gui_overlay;
    }
    GUIOverlay *getGUIOverlay() {
        return this->gui_overlay;
    }
    void createLightingMap(unsigned char lighting_min);
    void updateInput();
    void setScale(float c_scale);
    void setScale(QPointF centre, float c_scale);
    float getScale() const {
        return this->c_scale;
    }
    void addTextEffect(TextEffect *text_effect);
    void removeTextEffect(TextEffect *text_effect);
    void clear() {
        this->scene()->clear();
        this->text_effects.clear();
    }
    bool keyDown(KeyCode code) {
        return this->key_down[(int)code];
    }

public slots:
    void zoomOut();
    void zoomIn();
    void centreOnPlayer();
};

class GUIOverlay : public QWidget {
    PlayingGamestate *playing_gamestate;

    virtual void paintEvent(QPaintEvent *event);
    /*virtual QSize sizeHint() const {
        return QSize(640, 360);
    }*/

    bool display_progress;
    string progress_message;
    int  progress_percent;

    float fps;

    bool has_fade;
    bool fade_in;
    int fade_time_start_ms;

    void drawBar(QPainter &painter, float fx, float fy, float fwidth, float fheight, float fraction, QColor color);
public:
    GUIOverlay(PlayingGamestate *playing_gamestate, MainGraphicsView *view);
    virtual ~GUIOverlay() {
    }

    void setProgress(int progress_percent);
    void setProgress(int progress_percent, const string &progress_message) {
        this->display_progress = true;
        this->progress_percent = progress_percent;
        this->progress_message = progress_message;
        this->repaint();
    }
    void unsetProgress() {
        this->display_progress = false;
    }
    void setFPS(float fps) {
        this->fps = fps;
    }
    void setFadeIn();
    void setFadeOut();
};

/*class GUIOverlayItem : public QGraphicsProxyWidget {
    MainGraphicsView *view;

    virtual void advance(int phase);

public:
    GUIOverlayItem(MainGraphicsView *view) : QGraphicsProxyWidget(), view(view) {
    }
    virtual ~GUIOverlayItem() {
    }
};*/
