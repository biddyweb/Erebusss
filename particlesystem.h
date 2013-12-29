#pragma once

#include <vector>
using std::vector;

#include <QGraphicsItem>

#include "common.h"

class Particle {
    float xpos, ypos; // floats to allow for movement
    float xspeed, yspeed;
    int birth_time;
    bool flag;
public:
    Particle();

    float getX() const {
        return this->xpos;
    }
    float getY() const {
        return this->ypos;
    }
    void setPos(float xpos, float ypos) {
        this->xpos = xpos;
        this->ypos = ypos;
    }
    float getXSpeed() const {
        return this->xspeed;
    }
    float getYSpeed() const {
        return this->yspeed;
    }
    void setSpeed(float xspeed, float yspeed) {
        this->xspeed = xspeed;
        this->yspeed = yspeed;
    }
    int getBirthTime() const {
        return this->birth_time;
    }
    void setFlag(bool flag) {
        this->flag = flag;
    }
    bool isFlag() const {
        return this->flag;
    }
    void move(int loop_time);
};

class ParticleSystem : public QGraphicsItem {
protected:
    vector<Particle> particles;
    QPixmap pixmap;

    void moveParticles();
public:
    explicit ParticleSystem(const QPixmap &pixmap, QGraphicsItem *parent = 0) : QGraphicsItem(parent), pixmap(pixmap) {
    }

    virtual void advance(int phase);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    virtual QRectF boundingRect() const;
    virtual void updatePS()=0;
};

class SmokeParticleSystem : public ParticleSystem {
public:
    enum Type {
        TYPE_RISE = 0,
        TYPE_RADIAL = 1
    };
protected:
    Type type;
    float birth_rate;
    int life_exp;
    int last_emit_time;
public:
    explicit SmokeParticleSystem(const QPixmap &pixmap, QGraphicsItem *parent = 0);
    void setType(Type type) {
        this->type =  type;
    }
    void setBirthRate(float birth_rate);

    virtual void updatePS();
};
