#include "sound.h"
#include "game.h"
#include "qt_screen.h"

#ifndef Q_OS_ANDROID
Sound::Sound(const string &filename, bool stream) : volume(1.0f), stream(stream), is_fading(false), fade_start_time(-1), fade_end_time(-1) {
    if( this->stream ) {
        if( !music.openFromFile(filename.c_str()) ) {
            throw string("SFML failed to openFromFile");
        }
    }
    else {
        if( !buffer.loadFromFile(filename.c_str() ) ) {
            throw string("SFML failed to loadFromFile");
        }
        sound.setBuffer(buffer);
    }
}

void Sound::updateVolume() {
    // multiply with the global volume
    float real_volume = ((float)game_g->getGlobalSoundVolume())/100.0f * volume;
    int scale = stream ? game_g->getGlobalSoundVolumeMusic() : game_g->getGlobalSoundVolumeEffects();
    real_volume = ((float)scale)/100.0f * real_volume;
    if( is_fading ) {
        int time = game_g->getScreen()->getGameTimeTotalMS();
        if( time >= this->fade_end_time )
            real_volume = 0.0f;
        else {
            float alpha = ((float)(time - this->fade_start_time))/(float)(this->fade_end_time - this->fade_start_time);
            real_volume *= (1.0f - alpha);
        }
    }
    if( stream )
        music.setVolume(100.0f*real_volume);
    else
        sound.setVolume(100.0f*real_volume);
}

void Sound::setVolume(float volume) {
    this->volume = volume;
    this->updateVolume();
}

void Sound::fadeOut(int delay) {
    this->is_fading = true;
    this->fade_start_time = game_g->getScreen()->getGameTimeTotalMS();
    this->fade_end_time = fade_start_time + delay;
}

bool Sound::update() {
    if( this->is_fading ) {
        int time = game_g->getScreen()->getGameTimeTotalMS();
        if( time >= fade_end_time ) {
            this->stop();
            return true;
        }
        else {
            this->updateVolume();
        }
    }
    return false;
}

#else

Sound::Sound(const string &filename, bool stream) : android_sound(NULL), android_sound_effect(NULL) {
    this->android_sound_effect = AndroidSoundEffect::create(filename.c_str());
    if( android_sound_effect == NULL ) {
        throw string("failed to create AndroidSoundEffect");
    }
    this->android_sound = AndroidSound::create(android_sound_effect);
    if( android_sound == NULL ) {
        delete android_sound_effect;
        throw string("failed to create AndroidSound");
    }
}

Sound::~Sound() {
    qDebug("Sound::~Sound()");
    if( android_sound != NULL ) {
        delete android_sound;
    }
    if( android_sound_effect != NULL ) {
        delete android_sound_effect;
    }
}

#endif
