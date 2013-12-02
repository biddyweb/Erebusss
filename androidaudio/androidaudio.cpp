// modified from https://groups.google.com/forum/?fromgroups=#!topic/android-qt/rpPa_W6PF1Y , by Adam Pigg

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

#if defined(Q_OS_ANDROID)

#include "androidaudio.h"

#include <QDebug>

AndroidAudio::AndroidAudio() : sound_ok(false), mEngineObject(NULL), mEngineEngine(NULL), mOutputMixObject(NULL) {
    if( createEngine() ) {
        sound_ok = true;
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

    if (mOutputMixObject != NULL) {
        qDebug("destroy mix object");
        (*mOutputMixObject)->Destroy(mOutputMixObject);
    }

    if (mEngineObject != NULL) {
        qDebug("destroy engine object");
        (*mEngineObject)->Destroy(mEngineObject);
    }

    qDebug("AndroidAudio::destroyEngine() done");
}

#endif
