#-------------------------------------------------
#
# Project created by QtCreator 2016-08-12T21:55:19
#
#-------------------------------------------------

# Version
VT_VERSION = 1.16
VT_INTRO_VERSION = 1

VT_ANDROID_VERSION_ARMV7 = 22
VT_ANDROID_VERSION_ARM64 = 23
VT_ANDROID_VERSION_X86 = 24

VT_ANDROID_VERSION = $$VT_ANDROID_VERSION_X86

# Ubuntu 18.04
# sudo apt install qml-module-qt-labs-folderlistmodel qml-module-qtquick-extras qml-module-qtquick-controls2 qt5-default libqt5quickcontrols2-5 qtquickcontrols2-5-dev qtcreator qtcreator-doc libqt5serialport5-dev build-essential qml-module-qt3d qt3d5-dev

DEFINES += VT_VERSION=$$VT_VERSION
DEFINES += VT_INTRO_VERSION=$$VT_INTRO_VERSION

CONFIG += c++11

# Build mobile GUI
#CONFIG += build_mobile

# Bluetooth available
DEFINES += HAS_BLUETOOTH

# CAN bus available
#DEFINES += HAS_CANBUS

# Debug build (e.g. F5 to reload QML files)
#DEFINES += DEBUG_BUILD

!android: {
    # Serial port available
    DEFINES += HAS_SERIALPORT
}

# Options
#CONFIG += build_original
#CONFIG += build_platinum
#CONFIG += build_gold
#CONFIG += build_silver
#CONFIG += build_bronze
#CONFIG += build_free

QT       += core gui
QT       += widgets
QT       += printsupport
QT       += network
QT       += quick
QT       += quickcontrols2

contains(DEFINES, HAS_SERIALPORT) {
    QT       += serialport
}

contains(DEFINES, HAS_CANBUS) {
    QT       += serialbus
}

contains(DEFINES, HAS_BLUETOOTH) {
    QT       += bluetooth
}

android: QT += androidextras

android: TARGET = vesc_tool
!android: TARGET = vesc_tool_$$VT_VERSION


ANDROID_VERSION = 1

android:contains(QT_ARCH, i386) {
    VT_ANDROID_VERSION = $$VT_ANDROID_VERSION_X86
}

contains(ANDROID_TARGET_ARCH, arm64-v8a) {
    VT_ANDROID_VERSION = $$VT_ANDROID_VERSION_ARM64
}

contains(ANDROID_TARGET_ARCH, armeabi-v7a) {
    VT_ANDROID_VERSION = $$VT_ANDROID_VERSION_ARMV7
}

android: {
    manifest.input = $$PWD/android/AndroidManifest.xml.in
    manifest.output = $$PWD/android/AndroidManifest.xml
    QMAKE_SUBSTITUTES += manifest
}

TEMPLATE = app

release_win {
    DESTDIR = build/win
    OBJECTS_DIR = build/win/obj
    MOC_DIR = build/win/obj
    RCC_DIR = build/win/obj
    UI_DIR = build/win/obj
}

release_lin {
    # http://micro.nicholaswilson.me.uk/post/31855915892/rules-of-static-linking-libstdc-libc-libgcc
    # http://insanecoding.blogspot.se/2012/07/creating-portable-linux-binaries.html
    QMAKE_LFLAGS += -static-libstdc++ -static-libgcc
    DESTDIR = build/lin
    OBJECTS_DIR = build/lin/obj
    MOC_DIR = build/lin/obj
    RCC_DIR = build/lin/obj
    UI_DIR = build/lin/obj
}

release_macos {
    # brew install qt
    DESTDIR = build/macos
    OBJECTS_DIR = build/macos/obj
    MOC_DIR = build/macos/obj
    RCC_DIR = build/macos/obj
    UI_DIR = build/macos/obj
}

release_android {
    DESTDIR = build/android
    OBJECTS_DIR = build/android/obj
    MOC_DIR = build/android/obj
    RCC_DIR = build/android/obj
    UI_DIR = build/android/obj
}

build_mobile {
    DEFINES += USE_MOBILE
}

SOURCES += main.cpp\
        mainwindow.cpp \
    packet.cpp \
    vbytearray.cpp \
    commands.cpp \
    configparams.cpp \
    configparam.cpp \
    vescinterface.cpp \
    parametereditor.cpp \
    digitalfiltering.cpp \
    setupwizardapp.cpp \
    setupwizardmotor.cpp \
    startupwizard.cpp \
    utility.cpp

HEADERS  += mainwindow.h \
    packet.h \
    vbytearray.h \
    commands.h \
    datatypes.h \
    configparams.h \
    configparam.h \
    vescinterface.h \
    parametereditor.h \
    digitalfiltering.h \
    setupwizardapp.h \
    setupwizardmotor.h \
    startupwizard.h \
    utility.h

FORMS    += mainwindow.ui \
    parametereditor.ui

contains(DEFINES, HAS_BLUETOOTH) {
    SOURCES += bleuart.cpp
    HEADERS += bleuart.h
}

include(pages/pages.pri)
include(widgets/widgets.pri)
include(mobile/mobile.pri)

RESOURCES += res.qrc

build_original {
    RESOURCES += res_original.qrc \
    res_fw_original.qrc
    DEFINES += VER_ORIGINAL
} else:build_platinum {
    RESOURCES += res_platinum.qrc \
    res_fw.qrc
    DEFINES += VER_PLATINUM
} else:build_gold {
    RESOURCES += res_gold.qrc \
    res_fw.qrc
    DEFINES += VER_GOLD
} else:build_silver {
    RESOURCES += res_silver.qrc \
    res_fw.qrc
    DEFINES += VER_SILVER
} else:build_bronze {
    RESOURCES += res_bronze.qrc \
    res_fw.qrc
    DEFINES += VER_BRONZE
} else:build_free {
    RESOURCES += res_free.qrc \
    res_fw.qrc
    DEFINES += VER_FREE
} else {
    RESOURCES += res_neutral.qrc \
    res_fw.qrc
    DEFINES += VER_NEUTRAL
}

DISTFILES += \
    android/AndroidManifest.xml \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradlew \
    android/res/values/libs.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.properties

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
