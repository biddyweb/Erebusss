#include <QPainter>

#if QT_VERSION < 0x050000
#include <qmath.h>
#endif

#include "particlesystem.h"
#include "game.h"
#include "qt_screen.h"

Particle::Particle() : xpos(0.0f), ypos(0.0f), xspeed(0.0f), yspeed(0.0f), birth_time(0), flag(false) {
    this->birth_time = game_g->getScreen()->getGameTimeTotalMS();
}

void Particle::move(int loop_time) {
    this->xpos += loop_time * this->xspeed;
    this->ypos += loop_time * this->yspeed;
}


void ParticleSystem::advance(int phase) {
    //qDebug("AnimatedObject::advance() phase %d", phase);
    if( phase == 1 ) {
        this->updatePS();
    }
}

void ParticleSystem::moveParticles() {
    int real_loop_time = game_g->getScreen()->getGameTimeFrameMS();
    for(int i=0;i<particles.size();i++) {
        particles.at(i).move(real_loop_time);
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

SmokeParticleSystem::SmokeParticleSystem(const QPixmap &pixmap, QGraphicsItem *parent) : ParticleSystem(pixmap, parent),
    type(TYPE_RISE), birth_rate(0.0f), life_exp(1500), last_emit_time(0) {
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

    if( type == TYPE_RISE ) {
        int real_loop_time = game_g->getScreen()->getGameTimeFrameMS();
        // update particle speed
        for(int i=0;i<particles.size();i++) {
            int prob = poisson(100, real_loop_time);
            if( rand() % RAND_MAX <= prob ) {
                float xspeed = particles.at(i).getXSpeed();
                float yspeed = particles.at(i).getYSpeed();
                xspeed = - xspeed;
                particles.at(i).setSpeed(xspeed, yspeed);
            }
        }
    }

    // now move the particles
    this->moveParticles();

    // emit new particles
    int accumulated_time = game_g->getScreen()->getGameTimeTotalMS() - this->last_emit_time;
    //qDebug("accumulated_time = %d - %d = %d", game_g->getScreen()->getGameTimeTotalMS(), this->last_emit_time, accumulated_time);
    int new_particles = (int)(this->birth_rate/1000.0f * accumulated_time);
    this->last_emit_time += (int)(1000.0f/birth_rate * new_particles);
    if( new_particles > 0 ) {
        //qDebug("%d new particles (total will be %d)", new_particles, particles.size() + new_particles);
        for(int i=0;i<new_particles;i++) {
            Particle particle;
            if( type == TYPE_RISE ) {
                int dir = rand() % 2 == 0 ? 1 : -1;
                particle.setSpeed(dir*0.03f, -0.06f);
            }
            else if( type == TYPE_RADIAL ) {
                int deg = rand() % 360;
                float rad = (deg*M_PI)/180.0f;
                float speed = 0.2f;
                particle.setSpeed(speed*cos(rad), speed*sin(rad));
            }
            particles.push_back(particle);
        }
    }

    this->update();
}
