#pragma once

#include <string>
using std::string;

// SFML not supported on Qt Android
#ifdef Q_OS_ANDROID

// n.b., not safe to use forward declarations here - as need to call destructors
#include "androidaudio/androidsound.h"
#include "androidaudio/androidsoundeffect.h"

#else

#include <SFML/Audio.hpp>

#endif

#include "common.h"

class Sound {
#ifndef Q_OS_ANDROID

    float volume;
    bool stream;
    // if not streaming:
    sf::SoundBuffer buffer;
    sf::Sound sound;
    // if streaming:
    sf::Music music;

    // only for streams, where we manually call update()
    bool is_fading;
    int fade_start_time;
    int fade_end_time;

public:
    Sound(const string &filename, bool stream);
    ~Sound() {
    }

    bool isStream() const {
        return this->stream;
    }
    void updateVolume(); // updates the volume to take the global game volume into account
    void setVolume(float volume);
    void play(bool loop) {
        // if already playing, this function has no effect (doesn't restart)
        //qDebug("play");
        if( stream ) {
            if( music.getStatus() == sf::SoundSource::Playing ) {
                //qDebug("    already playing");
            }
            else {
                this->updateVolume();
                music.setLoop(loop);
                music.play();
            }
        }
        else {
            if( sound.getStatus() == sf::SoundSource::Playing ) {
                //qDebug("    already playing");
            }
            else {
                this->updateVolume();
                sound.setLoop(loop);
                sound.play();
            }
        }
    }
    void pause() {
        if( stream ) {
            music.pause();
        }
        else {
            sound.pause();
        }
    }
    void unpause() {
        // plays without changing anything else
        if( stream ) {
            music.play();
        }
        else {
            sound.play();
        }
    }
    void stop() {
        if( stream ) {
            music.stop();
        }
        else {
            sound.stop();
        }
        this->is_fading = false;
    }

    void fadeOut(int delay);
    bool update();
#else
    AndroidSound *android_sound;
    AndroidSoundEffect *android_sound_effect;

public:
    Sound(const string &filename, bool stream);
    ~Sound();

    void play(bool loop) {
        if( android_sound != NULL ) {
            android_sound->play(loop);
        }
    }
    AndroidSound *getAndroidSound() const {
        return android_sound;
    }
#endif

};
