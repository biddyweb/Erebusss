// modified from https://groups.google.com/forum/?fromgroups=#!topic/android-qt/rpPa_W6PF1Y , by Adam Pigg

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

#if defined(Q_OS_ANDROID)

#include "androidaudio.h"
#include "androidsoundeffect.h"

#include <QDebug>

#include <cmath>

AndroidAudio::AndroidAudio() : sound_ok(false), mEngineObject(NULL), mEngineEngine(NULL), mOutputMixObject(NULL), mPlayerObject(NULL) {
    if( createEngine() ) {
        if( startSoundPlayer() ) {
            sound_ok = true;
        }
    }
}

AndroidAudio::~AndroidAudio()
{
    destroyEngine();

}

// create the engine and output mix objects
bool AndroidAudio::createEngine()
{
    SLresult result;

    // create engine
    result = slCreateEngine(&mEngineObject, 0, NULL, 0, NULL, NULL);
    Q_ASSERT(SL_RESULT_SUCCESS == result);
    if( result != SL_RESULT_SUCCESS ) {
        qDebug("slCreateEngine failed");
        return false;
    }

    // realize the engine
    result = (*mEngineObject)->Realize(mEngineObject, SL_BOOLEAN_FALSE);
    Q_ASSERT(SL_RESULT_SUCCESS == result);
    if( result != SL_RESULT_SUCCESS ) {
        qDebug("engine Realize failed");
        return false;
    }

    // get the engine interface, which is needed in order to create other objects
    result = (*mEngineObject)->GetInterface(mEngineObject, SL_IID_ENGINE, &mEngineEngine);
    Q_ASSERT(SL_RESULT_SUCCESS == result);
    if( result != SL_RESULT_SUCCESS ) {
        qDebug("failed to get engine interface");
        return false;
    }

    // create output mix
    const SLInterfaceID ids[] = {};
    const SLboolean req[] = {};
    result = (*mEngineEngine)->CreateOutputMix(mEngineEngine, &mOutputMixObject, 0, ids, req);
    Q_ASSERT(SL_RESULT_SUCCESS == result);
    if( result != SL_RESULT_SUCCESS ) {
        qDebug("failed to create output mix");
        return false;
    }

    // realize the output mix
    result = (*mOutputMixObject)->Realize(mOutputMixObject, SL_BOOLEAN_FALSE);
    Q_ASSERT(SL_RESULT_SUCCESS == result);
    if( result != SL_RESULT_SUCCESS ) {
        qDebug("failed to realize output mix");
        return false;
    }

    qDebug() << "Created Android Audio Engine";
    return true;
}

void AndroidAudio::destroyEngine()
{
    qDebug("AndroidAudio::destroyEngine()");

    if (mPlayerObject != NULL) {
        qDebug("destroy player object %d", mPlayerObject);
        (*mPlayerObject)->Destroy(mPlayerObject);
    }

    if (mOutputMixObject != NULL) {
        qDebug("destroy mix object %d", mOutputMixObject);
        (*mOutputMixObject)->Destroy(mOutputMixObject);
    }

    if (mEngineObject != NULL) {
        qDebug("destroy engine object %d", mEngineObject);
        (*mEngineObject)->Destroy(mEngineObject);
    }

    qDebug("AndroidAudio::destroyEngine() done");
}

AndroidSoundEffect *AndroidAudio::loadSound(const QString &filename) {
    if( !sound_ok ) {
        qDebug("sound engine not available");
        return NULL;
    }
    AndroidSoundEffect *sound = new AndroidSoundEffect(filename);
    if( !sound->load() ) {
        qDebug() << "failed to load sound";
        delete sound;
        sound = NULL;
    }
    return sound;
}

bool AndroidAudio::startSoundPlayer()
{
    qDebug() << "Starting Sound Player";

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
    lDataLocatorOut.outputMix = mOutputMixObject;

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

    lRes = (*mEngineEngine)->CreateAudioPlayer(mEngineEngine, &mPlayerObject, &lDataSource, &lDataSink, lSoundPlayerIIDCount, lSoundPlayerIIDs, lSoundPlayerReqs);
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
    return true;
}

void AndroidAudio::setVolume(int volume) {
    if( !sound_ok ) {
        return;
    }

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

void AndroidAudio::playSound(const AndroidSoundEffect *sound, bool loop) {
    //qDebug("AndroidAudio::playSound(%d, %d)\n", sound, loop);
    SLresult lRes;
    SLuint32 lPlayerState;

    if( !sound_ok ) {
        qDebug("sound engine not available");
        return;
    }

    if (!sound) {
        return;
    }
    //Get the current state of the player
    (*mPlayerObject)->GetState(mPlayerObject, &lPlayerState);

    //If the player is realised
    if (lPlayerState == SL_OBJECT_STATE_REALIZED) {
        //Get the buffer and length of the effect
        int16_t* lBuffer = (int16_t *)sound->mBuffer;
        off_t lLength = sound->mLength;

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

#endif
