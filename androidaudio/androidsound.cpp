#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

#if defined(Q_OS_ANDROID)

#include "androidsound.h"

#include <QDebug>

#include <cmath>

AndroidAudio AndroidSound::androidAudio;

AndroidSound::AndroidSound() : mPlayerObject(NULL), mPlayerPlay(NULL), mPlayerQueue(NULL), mPlayerVolume(NULL), sound_effect(NULL) {
}

AndroidSound::~AndroidSound() {
    // n.b., we don't delete sound_effect, as not owned by this class (so one AndroidSound can point to multiple AndroidSoundEffects)
    qDebug("AndroidSound::~AndroidSound()");
    if (mPlayerObject != NULL) {
        qDebug("AndroidSound::~AndroidSound(): destroy player object");
        (*mPlayerObject)->Destroy(mPlayerObject);
    }
}

bool AndroidSound::setSoundEffect(const AndroidSoundEffect *sound_effect) {
    qDebug() << "Starting Sound Player";

    this->sound_effect = NULL;
    if (mPlayerObject != NULL) {
        qDebug("destroy player object");
        (*mPlayerObject)->Destroy(mPlayerObject);
        mPlayerObject = NULL;
    }
    if( !androidAudio.sound_ok ) {
        return false;
    }

    SLresult lRes;

    //Configure the sound player input/output
    SLDataLocator_AndroidSimpleBufferQueue lDataLocatorIn;
    lDataLocatorIn.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
    lDataLocatorIn.numBuffers = 1;

    //Set the data format
    SLDataFormat_PCM lDataFormat;
    lDataFormat.formatType = SL_DATAFORMAT_PCM;
    lDataFormat.numChannels = 2;
    //lDataFormat.samplesPerSec = SL_SAMPLINGRATE_44_1;
    lDataFormat.samplesPerSec = SL_SAMPLINGRATE_22_05; // for some reason, samples play too high pitched when using SL_SAMPLINGRATE_44_1?!
    lDataFormat.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    lDataFormat.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
    lDataFormat.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    lDataFormat.endianness = SL_BYTEORDER_LITTLEENDIAN;

    SLDataSource lDataSource;
    lDataSource.pLocator = &lDataLocatorIn;
    lDataSource.pFormat = &lDataFormat;

    SLDataLocator_OutputMix lDataLocatorOut;
    lDataLocatorOut.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    lDataLocatorOut.outputMix = androidAudio.mOutputMixObject;

    SLDataSink lDataSink;
    lDataSink.pLocator = &lDataLocatorOut;
    lDataSink.pFormat = NULL;

    //Create the sound player
    /*const SLuint32 lSoundPlayerIIDCount = 4;
    const SLInterfaceID lSoundPlayerIIDs[] = { SL_IID_PLAY, SL_IID_BUFFERQUEUE, SL_IID_VOLUME, SL_IID_SEEK };
    const SLboolean lSoundPlayerReqs[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };*/
    const SLuint32 lSoundPlayerIIDCount = 3;
    const SLInterfaceID lSoundPlayerIIDs[] = { SL_IID_PLAY, SL_IID_BUFFERQUEUE, SL_IID_VOLUME };
    const SLboolean lSoundPlayerReqs[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

    qDebug() << "Configured Sound Player";

    lRes = (*androidAudio.mEngineEngine)->CreateAudioPlayer(androidAudio.mEngineEngine, &mPlayerObject, &lDataSource, &lDataSink, lSoundPlayerIIDCount, lSoundPlayerIIDs, lSoundPlayerReqs);
    Q_ASSERT(SL_RESULT_SUCCESS == lRes);
    if( lRes != SL_RESULT_SUCCESS ) {
        qDebug("failed to create audio player");
        return false;
    }

    qDebug() << "Created Sound Player";

    lRes = (*mPlayerObject)->Realize(mPlayerObject, SL_BOOLEAN_FALSE);
    Q_ASSERT(SL_RESULT_SUCCESS == lRes);
    if( lRes != SL_RESULT_SUCCESS ) {
        qDebug("failed to realize sound player");
        return false;
    }

    qDebug() << "Realised Sound Player";
    lRes = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_PLAY, &mPlayerPlay);
    Q_ASSERT(SL_RESULT_SUCCESS == lRes);
    if( lRes != SL_RESULT_SUCCESS ) {
        qDebug("failed to get player interface");
        return false;
    }

    lRes = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_VOLUME, &mPlayerVolume);
    Q_ASSERT(SL_RESULT_SUCCESS == lRes);
    if( lRes != SL_RESULT_SUCCESS ) {
        qDebug("failed to obtain volume interface");
        return false;
    }

    /*lRes = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_VOLUME, &mPlayerSeek);
    Q_ASSERT(SL_RESULT_SUCCESS == lRes);
    if( lRes != SL_RESULT_SUCCESS ) {
        qDebug("failed to obtain seek interface");
        return false;
    }*/

    lRes = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_BUFFERQUEUE, &mPlayerQueue);
    Q_ASSERT(SL_RESULT_SUCCESS == lRes);
    if( lRes != SL_RESULT_SUCCESS ) {
        qDebug("failed to get player queue interface");
        return false;
    }

    lRes = (*mPlayerPlay)->SetPlayState(mPlayerPlay, SL_PLAYSTATE_PLAYING);
    Q_ASSERT(SL_RESULT_SUCCESS == lRes);
    if( lRes != SL_RESULT_SUCCESS ) {
        qDebug("failed to set player state");
        return false;
    }

    qDebug() << "Created Buffer Player";
    this->sound_effect = sound_effect;
    return true;
}

void AndroidSound::setVolume(int volume) {
    if( volume == 0 ) {
        (*this->mPlayerVolume)->SetVolumeLevel(mPlayerVolume, SL_MILLIBEL_MIN);
    }
    else if( volume == 100 ) {
        (*this->mPlayerVolume)->SetVolumeLevel(mPlayerVolume, 0);
    }
    else {
        float alpha = ((float)volume)/100.0f;
        //float alpha = log((float)volume)/log(100.0f);
        /*int android_volume = (int)((1.0f-alpha)*SL_MILLIBEL_MIN);*/
        // from http://stackoverflow.com/questions/14453674/audio-output-level-in-a-form-that-can-be-converted-to-decibel
        int android_volume = M_LN2/log(1.0f/(1.0f-alpha))*-1000.0f;
        if( android_volume < SL_MILLIBEL_MIN )
            android_volume = SL_MILLIBEL_MIN;
        qDebug("volume: %d alpha %f android_volume: %d", volume, alpha, android_volume);
        (*this->mPlayerVolume)->SetVolumeLevel(mPlayerVolume, android_volume);
    }
}

void AndroidSound::play(bool loop) {
    SLresult lRes;
    SLuint32 lPlayerState;

    if( sound_effect == NULL ) {
        return;
    }

    //Get the current state of the player
    (*mPlayerObject)->GetState(mPlayerObject, &lPlayerState);

    //If the player is realised
    if (lPlayerState == SL_OBJECT_STATE_REALIZED) {
        //Get the buffer and length of the effect
        int16_t* lBuffer = (int16_t *)sound_effect->getBuffer();
        off_t lLength = sound_effect->getLength();

        SLBufferQueueState queue_state;
        (*mPlayerQueue)->GetState(mPlayerQueue, &queue_state);
        if( queue_state.count != 0 ) {
            // still playing
        }
        else {
            //Remove any sound from the queue
            lRes = (*mPlayerQueue)->Clear(mPlayerQueue);
            Q_ASSERT(SL_RESULT_SUCCESS == lRes);

            // Set loop
            /*if( loop ) {
                (*mPlayerSeek)->SetLoop(mPlayerSeek, SL_BOOLEAN_TRUE, 0, SL_TIME_UNKNOWN);
            }*/

            //Play the new sound
            (*mPlayerQueue)->Enqueue(mPlayerQueue, lBuffer, lLength);
            Q_ASSERT(SL_RESULT_SUCCESS == lRes);
        }
    }
}

AndroidSound *AndroidSound::create(const AndroidSoundEffect *sound_effect) {
    AndroidSound *sound = new AndroidSound();
    if( !sound->setSoundEffect(sound_effect) ) {
        qDebug() << "failed to create sound";
        delete sound;
        sound = NULL;
    }
    return sound;
}

#endif
