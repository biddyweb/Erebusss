# Add files and directories to ship with the application 
# by adapting the examples below.
# file1.source = myfile
# dir1.source = mydir
DEPLOYMENTFOLDERS = # file1 dir1
android {
    # no music on Android, to reduce file size
}
else:symbian {
    # no music on Symbian, to reduce file size
}
else {
    dir1.source = music
    DEPLOYMENTFOLDERS += dir1
}

QT += webkit
QT += xml

symbian:TARGET.UID3 = 0xE11B6032

VERSION = 0.2 # remember to update version in game.h

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
    # phonon not supported on Android
    //LIBS += -lOpenSLES
}
else {
    QT += phonon
}

symbian {
    ICON = erebus.svg # Symbian icon
}
else:win32 {
    RC_FILE = resource.rc # Windows icon
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
    logiface.cpp
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
    logiface.h
FORMS +=

# Please do not modify the following two lines. Required for deployment.
include(deployment.pri)
qtcAddDeployment()

OTHER_FILES += \
    _docs/todo.txt \
    _docs/design.odt \
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
    _docs/erebus.html \
    data/quest_wizard_dungeon_find_item.xml

RESOURCES += \
    erebus.qrc
