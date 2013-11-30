#pragma once

#include <string>
using std::string;

#include "common.h"

// SFML not supported on Qt Android
#ifdef Q_OS_ANDROID

#include "androidaudio/androidaudio.h"

#else

#include <SFML/Audio.hpp>

#endif

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
    AndroidSoundEffect *android_sound;

public:
    Sound(AndroidSoundEffect *android_sound) : android_sound(android_sound) {
    }
    ~Sound() {
        delete android_sound;
    }

    AndroidSoundEffect *getAndroidSound() const {
        return android_sound;
    }
#endif

};
