#ifndef ANDROIDSOUNDEFFECT_H
#define ANDROIDSOUNDEFFECT_H

#include <QtGlobal> // need this to get Q_OS_ANDROID #define, which we need before we include anything else!

#if defined(Q_OS_ANDROID)

#include <QObject>
#include <QDebug>
#include <QString>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

class AndroidSoundEffect : public QObject
{
    Q_OBJECT
public:
    explicit AndroidSoundEffect(const QString& pPath, QObject *parent = 0);
    ~AndroidSoundEffect();

    const char* getPath();
    bool load();
    bool unload();

signals:

public slots:

private:
    char* mBuffer;
    off_t mLength;

    QString mPath;

    friend class AndroidAudio;
};

#endif

#endif // ANDROIDSOUNDEFFECT_H
