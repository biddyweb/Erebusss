//#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!
#include "../common.h" // need this to get Q_OS_ANDROID #define, as well as WANT_ANDROID_SOUND

#if defined(Q_OS_ANDROID)
#ifdef WANT_ANDROID_SOUND

#include "androidsoundeffect.h"
#include <QFile>

AndroidSoundEffect::AndroidSoundEffect(const QString& path, QObject *parent) :
    QObject(parent), mBuffer(NULL), mLength(0), mPath(path)
{
}

AndroidSoundEffect::~AndroidSoundEffect()
{

}

bool AndroidSoundEffect::load()
{
    QFile lSoundFile(mPath);

    qDebug() << "opening:" << mPath;

    if (!lSoundFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open: " << mPath;
        return false;
    }

    qDebug() << "readingSize:";
    mLength = lSoundFile.size();
    if (mLength <= 0) {
        qDebug() << "File size <= 0 :" << mPath;
        lSoundFile.close();
        return false;
    }

    qDebug() << "readingData:" << mLength;

    mBuffer = (char*)malloc((mLength));

    int dataRead = lSoundFile.read(mBuffer, mLength);
    if (dataRead != mLength) {
        qDebug() << "File size != dataRead :" << mPath;
        lSoundFile.close();
        return false;
    }

    qDebug() << "closing:";

    lSoundFile.close();
    return true;
}


bool AndroidSoundEffect::unload()
{
    delete[] mBuffer;
    mBuffer = NULL;
    mLength = 0;
    return true;
}

#endif
#endif
