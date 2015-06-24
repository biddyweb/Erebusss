# Add files and directories to ship with the application
# by adapting the examples below.
# file1.source = myfile
# dir1.source = mydir
#DEPLOYMENTFOLDERS = # file1 dir1
android {
    # no music on Android, to reduce file size
    dir1.source = data
    dir2.source = docs
    dir3.source = gfx
    dir4.source = sound
    dir5.source = ts
    DEPLOYMENTFOLDERS += dir1 dir2 dir3 dir4 dir5
}
else:symbian {
    # no music on Symbian, to reduce file size
    dir1.source = data
    dir2.source = docs
    dir3.source = gfx
    dir4.source = sound
    dir5.source = ts
    DEPLOYMENTFOLDERS += dir1 dir2 dir3 dir4 dir5
}
else {
    # for other platforms, this is only really relevant to make runnable from QtCreator - actual deployment is done via scripts
    dir1.source = data
    dir2.source = docs
    dir3.source = gfx
    dir4.source = sound
    dir5.source = ts
    dir6.source = music
    DEPLOYMENTFOLDERS += dir1 dir2 dir3 dir4 dir5 dir6
    win32 {
        # copy required DLLs
        PWD_WIN = $${PWD}
        PWD_WIN ~= s,/,\\,g
        OUT_PWD_WIN = $${OUT_PWD}
        OUT_PWD_WIN ~= s,/,\\,g
        # for SFML:
        CONFIG(debug, debug|release) {
            QMAKE_POST_LINK = copy $${PWD_WIN}\\libsndfile-1.dll $${OUT_PWD_WIN}\\ && copy $${PWD_WIN}\\openal32.dll $${OUT_PWD_WIN}\\ && copy $${PWD_WIN}\\sfml-audio-d-2.dll $${OUT_PWD_WIN}\\ && copy $${PWD_WIN}\\sfml-system-d-2.dll $${OUT_PWD_WIN}\\
        }
        else {
            QMAKE_POST_LINK = copy $${PWD_WIN}\\libsndfile-1.dll $${OUT_PWD_WIN}\\ && copy $${PWD_WIN}\\openal32.dll $${OUT_PWD_WIN}\\ && copy $${PWD_WIN}\\sfml-audio-2.dll $${OUT_PWD_WIN}\\ && copy $${PWD_WIN}\\sfml-system-2.dll $${OUT_PWD_WIN}\\
        }
    }
}

greaterThan(QT_MAJOR_VERSION, 4) {
    USING_WEBKIT {
        QT += webkitwidgets
    }
    else {
        QT += widgets
    }
}
USING_WEBKIT {
    QT += webkit
}
QT += xml
//QT += opengl

greaterThan(QT_MAJOR_VERSION, 4) {
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
}

# Test UID only:
#symbian:TARGET.UID3 = 0xE11B6032
# For use with Nokia Ovi Store only:
symbian:TARGET.UID3 = 0x2006dbd2

VERSION = 0.15 # remember to update version in common.h, and for Android/necessitas!

# Smart Installer package's UID
# This UID is from the protected range 
# and therefore the package will fail to install if self-signed
# By default qmake uses the unprotected range value if unprotected UID is defined for the application
# and 0x2002CCCF value if protected UID is given to the application
#symbian:DEPLOYMENT.installer_header = 0x2002CCCF

# SwEvent needed so that lauching the online help still works on Symbian when browser already open
symbian {
    TARGET.CAPABILITY += SwEvent

    my_deployment.pkg_prerules += vendorinfo

    DEPLOYMENT += my_deployment

    vendorinfo += "%{\"Mark Harman\"}" ":\"Mark Harman\""
}

# Allow network access on Symbian
#symbian:TARGET.CAPABILITY += NetworkServices

# If your application uses the Qt Mobility libraries, uncomment
# the following lines and add the respective components to the 
# MOBILITY variable. 
# CONFIG += mobility
# MOBILITY +=

android {
    LIBS += -lOpenSLES
}
else {
    # SFML:
    win32 {
        INCLUDEPATH += $$PWD # add the source folder for headers (needed for Qt 5)
        LIBS += -L$$PWD # add the source folder for libs
    }
    CONFIG(debug, debug|release) {
        LIBS += -lsfml-audio-d -lsfml-system-d
    }
    else {
        LIBS += -lsfml-audio -lsfml-system
    }
}

symbian {
    ICON = erebus.svg # Symbian icon
}
else:win32 {
    RC_FILE = resource.rc # Windows icon
}
else:unix {
    icon.files = erebus64.png
    icon.path = /usr/share/pixmaps
    desktopfile.files = erebus.desktop
    desktopfile.path = /usr/share/applications
}

SOURCES += main.cpp mainwindow.cpp \
    game.cpp \
    qt_screen.cpp \
    rpg/location.cpp \
    rpg/character.cpp \
    rpg/utils.cpp \
    rpg/item.cpp \
    qt_utils.cpp \
    playinggamestate.cpp \
    optionsgamestate.cpp \
    gamestate.cpp \
    logiface.cpp \
    infodialog.cpp \
    androidaudio/androidsoundeffect.cpp \
    androidaudio/androidaudio.cpp \
    sound.cpp \
    androidaudio/androidsound.cpp \
    rpg/locationgenerator.cpp \
    rpg/profile.cpp \
    scrollinglistwidget.cpp \
    animatedobject.cpp \
    particlesystem.cpp \
    maingraphicsview.cpp \
    webvieweventfilter.cpp \
    rpg/rpgengine.cpp \
    test.cpp
HEADERS += mainwindow.h \
    game.h \
    qt_screen.h \
    rpg/location.h \
    rpg/character.h \
    rpg/utils.h \
    rpg/item.h \
    qt_utils.h \
    playinggamestate.h \
    optionsgamestate.h \
    gamestate.h \
    logiface.h \
    infodialog.h \
    common.h \
    androidaudio/androidsoundeffect.h \
    androidaudio/androidaudio.h \
    sound.h \
    androidaudio/androidsound.h \
    rpg/locationgenerator.h \
    rpg/profile.h \
    scrollinglistwidget.h \
    animatedobject.h \
    particlesystem.h \
    maingraphicsview.h \
    webvieweventfilter.h \
    rpg/rpgengine.h \
    test.h
FORMS +=

# Please do not modify the following two lines. Required for deployment.
include(deployment.pri)
qtcAddDeployment()

OTHER_FILES += \
    _devdocs/todo.txt \
    _devdocs/design.odt \
    data/items.xml \
    data/images.xml \
    android/AndroidManifest.xml \
    android/res/drawable/icon.png \
    android/res/drawable/logo.png \
    android/res/drawable-hdpi/icon.png \
    android/res/drawable-ldpi/icon.png \
    android/res/drawable-mdpi/icon.png \
    android/res/layout/splash.xml \
    android/res/values/libs.xml \
    android/res/values/strings.xml \
    android/res/values-de/strings.xml \
    android/res/values-el/strings.xml \
    android/res/values-es/strings.xml \
    android/res/values-et/strings.xml \
    android/res/values-fa/strings.xml \
    android/res/values-fr/strings.xml \
    android/res/values-id/strings.xml \
    android/res/values-it/strings.xml \
    android/res/values-ja/strings.xml \
    android/res/values-ms/strings.xml \
    android/res/values-nb/strings.xml \
    android/res/values-nl/strings.xml \
    android/res/values-pl/strings.xml \
    android/res/values-pt-rBR/strings.xml \
    android/res/values-ro/strings.xml \
    android/res/values-rs/strings.xml \
    android/res/values-ru/strings.xml \
    android/res/values-zh-rCN/strings.xml \
    android/res/values-zh-rTW/strings.xml \
    android/src/org/kde/necessitas/ministro/IMinistro.aidl \
    android/src/org/kde/necessitas/ministro/IMinistroCallback.aidl \
    android/src/org/kde/necessitas/origo/QtActivity.java \
    android/src/org/kde/necessitas/origo/QtApplication.java \
    android/version.xml \
    data/npcs.xml \
    data/quests.xml \
    data/quest_kill_goblins.xml \
    data/quest_wizard_dungeon_find_item.xml \
    data/spells.xml \
    docs/erebus.html \
    _devdocs/rules.odt \
    data/quest_necromancer.xml \
    data/quest_through_mountains.xml \
    data/quest_test.xml

RESOURCES +=
TRANSLATIONS=ts/erebus_fr.ts
