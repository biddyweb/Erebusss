#include "common.h"
#include "mainwindow.h"
#include "qt_screen.h"
#include "game.h"
#include "logiface.h"

#include <QApplication>
#include <QDir>
#include <QTranslator>

/** Platform #defines:
  * smallscreen_c: whether the platform has a smallscreen or not (phones, handhelds).
  * touchscreen_c: whether to optimise the UI for a touchscreen rather than mouse/touchpad.
  * lightdistribution_c: whether the distribution misses large non-essential files (currently this means the intro music)
  */
#if defined(Q_OS_ANDROID) || defined(Q_OS_SYMBIAN) || defined(Q_WS_SIMULATOR) || defined(Q_WS_MAEMO_5)
bool smallscreen_c = true; // on Android default to true, but set dynamically from Java
bool touchscreen_c = true;
bool lightdistribution_c = true;
#else
bool smallscreen_c = false;
bool touchscreen_c = false;
bool lightdistribution_c = false;
#endif

#if defined(Q_OS_ANDROID)
char *DEPLOYMENT_PATH = "assets:/";
#else
char *DEPLOYMENT_PATH = "";
#endif

class MyApplication : public QApplication {

protected:
    bool event(QEvent *event);

public:
    MyApplication(int argc, char *argv[]) : QApplication(argc, argv) {
    }
};

bool MyApplication::event(QEvent *event) {
    /* Activate/deactivate events aren't received on Android when we switch applications. But worse, we seem
     * to receive these events on Android when we don't on Windows, e.g., when opening/closing dialogs, which
     * can lead to a bug where the game pauses unexpectedly (e.g., talk to an NPC, say at least one thing, then
     * close the talk window - the game will still be paused).
     * Inactive/active for Android is handled via JNI, see below.
     * Also disabled for Linux, as we get an inactive message when the game first starts - probably best to
     * disable, until we understand what's happening.
     */
    if( event->type() == QEvent::ApplicationActivate ) {
#if !defined(Q_OS_ANDROID) && !defined(__linux)
        LOG("application activated\n");
        if( game_g != NULL && game_g->getScreen() != NULL ) {
            game_g->activate(true);
        }
#endif
    }
    else if( event->type() == QEvent::ApplicationDeactivate ) {
#if !defined(Q_OS_ANDROID) && !defined(__linux)
        LOG("application deactivated\n");
        if( game_g != NULL && game_g->getScreen() != NULL ) {
            game_g->activate(false);
        }
#endif
    }
    return QApplication::event(event);
}

#if defined(Q_OS_ANDROID)

// native methods for Android
// see http://community.kde.org/Necessitas/JNI

#include <jni.h>

static void AndroidOnResume(JNIEnv * /*env*/, jobject /*thiz*/) {
    LOG("application activated\n");
    if( game_g != NULL && game_g->getScreen() != NULL ) {
        game_g->activate(true);
    }
}

static void AndroidOnPause(JNIEnv * /*env*/, jobject /*thiz*/) {
    LOG("application deactivated\n");
    if( game_g != NULL && game_g->getScreen() != NULL ) {
        game_g->activate(false);
    }
}

static void AndroidSetLargeScreen(JNIEnv * /*env*/, jobject /*thiz*/) {
    qDebug("application set large screen\n");
    //smallscreen_c = false;
    // don't actually use this yet - windows still open fullscreen, and problems with fonts need fixing
}

static JNINativeMethod methods[] = {
    {"AndroidOnResume", "()V", (void *)AndroidOnResume},
    {"AndroidOnPause", "()V", (void *)AndroidOnPause},
    {"AndroidSetLargeScreen", "()V", (void *)AndroidSetLargeScreen}
};

// this method is called immediately after the module is load
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    qDebug("JNI_OnLoad enter");
    JNIEnv* env = NULL;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        qDebug("JNI: can't get environment");
        return -1;
    }

    // search for our class
#if QT_VERSION >= 0x050000
    jclass clazz=env->FindClass("org/qtproject/qt5/android/bindings/QtActivity");
#else
    jclass clazz=env->FindClass("org/kde/necessitas/origo/QtActivity");
#endif
    if (!clazz)
    {
        qDebug("JNI: can't find QtActivity class");
        return -1;
    }
    // keep a global reference to it
    jclass classID = (jclass)env->NewGlobalRef(clazz);

    // register our native methods
    if (env->RegisterNatives(classID, methods, sizeof(methods) / sizeof(methods[0])) < 0)
    {
        qDebug("JNI: failed to register methods");
        return -1;
    }

    qDebug("JNI_OnLoad enter");
    return JNI_VERSION_1_6;
}
#endif

int main(int argc, char *argv[])
{
    MyApplication app(argc, argv);
    {
        QDir path = QDir::currentPath();
        path.cd("plugins/");
        QApplication::addLibraryPath(path.absolutePath());
    }

    QTranslator translator;
    if( !smallscreen_c ) {
        // not full tested on mobile platforms (Android doesn't pick up device language settings anyway; need to ensure that strings aren't too long for Android/Symbian)
        //qDebug("current dir path: %s", QDir::currentPath().toStdString().c_str());
        // Translation
        QString locale = QLocale::system().name().section('_', 0, 0);
        //locale = QLocale(QLocale::French, QLocale::France).name().section('_', 0, 0); // test
        qDebug("locale: %s", locale.toStdString().c_str());
        if( !translator.load(DEPLOYMENT_PATH + QString("ts/erebus_") + locale) ) {
            qDebug("failed to load translator file");
        }
        app.installTranslator(&translator);
    }

    bool fullscreen = true;
    bool runtests = false;
    bool help = false;

    //fullscreen = false;
    //runtests = true;

#if !defined(Q_OS_ANDROID)
    // n.b., crashes when run on Galaxy Nexus (even though fine in the emulator)
    for(int i=0;i<argc;i++) {
        if( strcmp(argv[i], "-windowed") == 0 )
            fullscreen = false;
        else if( strcmp(argv[i], "-runtests") == 0 )
            runtests = true;
        else if( strcmp(argv[i], "-help") == 0 )
            help = true;
        else if( strncmp(argv[i], "-datafolder=", 12) == 0 ) {
            printf("Setting data folder:\n");
            DEPLOYMENT_PATH = &(argv[i])[12];
            if( DEPLOYMENT_PATH[0] == '\"' ) {
                DEPLOYMENT_PATH++;
            }
            if( strlen(DEPLOYMENT_PATH) >= 1 && DEPLOYMENT_PATH[strlen(DEPLOYMENT_PATH)-1] == '\"' ) {
                DEPLOYMENT_PATH[strlen(DEPLOYMENT_PATH)-1] = '\0';
            }

#if defined(_WIN32)
            char folder_sep = '\\';
#else
            char folder_sep = '/';
#endif
            if( strlen(DEPLOYMENT_PATH) >= 1 && DEPLOYMENT_PATH[strlen(DEPLOYMENT_PATH)-1] != folder_sep ) {
                char *new_folder = new char[strlen(DEPLOYMENT_PATH)+2];
                strcpy(new_folder, DEPLOYMENT_PATH);
                new_folder[strlen(DEPLOYMENT_PATH)] = folder_sep;
                new_folder[strlen(DEPLOYMENT_PATH)+1] = '\0';
                DEPLOYMENT_PATH = new_folder;
            }
            printf("%s\n", DEPLOYMENT_PATH);
        }
    }
#endif

    if( help ) {
        // n.b., won't show on Windows due to being a Windowed application
        printf("Erebus v%d.%d\n", versionMajor, versionMinor);
        printf("Available options:\n");
        printf("    -windowed - Run in windowed mode\n");
        printf("    -runtests - Run tests\n");
        printf("    -help     - Display this message\n");
        printf("Please see the file erebus.html in docs/ for full instructions.\n");
        return 0;
    }

    Game game;
    if( runtests ) {
        game.runTests();
    }
    else {
        game.run(fullscreen);
    }
    return 0;
}
