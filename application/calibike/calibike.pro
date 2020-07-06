#VT_VERSION = 0.95

VT_VERSION = 1.00
VT_INTRO_VERSION = 1
VT_IS_TEST_VERSION = 0

VT_ANDROID_VERSION_ARMV7 = 22
VT_ANDROID_VERSION_ARM64 = 23
VT_ANDROID_VERSION_X86 = 24

VT_ANDROID_VERSION = $$VT_ANDROID_VERSION_X86

DEFINES += VT_VERSION=$$VT_VERSION
DEFINES += VT_INTRO_VERSION=$$VT_INTRO_VERSION
!vt_test_version: {
    DEFINES += VT_IS_TEST_VERSION=$$VT_IS_TEST_VERSION
}
vt_test_version: {
    DEFINES += VT_IS_TEST_VERSION=1
}


CONFIG += c++11

# Bluetooth available
DEFINES += HAS_BLUETOOTH

# Positioning
DEFINES += HAS_POS_CALIBIKE

!android: {
    # Serial port available
#    DEFINES += HAS_SERIALPORT
}

QT += core
QT += gui
QT += widgets
QT += serialport
QT += network
QT += printsupport
QT += quick
QT += quickcontrols2

contains(DEFINES, HAS_SERIALPORT) {
    QT += serialport
}

contains(DEFINES, HAS_BLUETOOTH) {
    QT += bluetooth
}


contains(DEFINES, HAS_POS_CALIBIKE) {
    QT += positioning
}

android: QT += androidextras

android: TARGET = calibike_tool
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

release_android {
    DESTDIR = build/android
    OBJECTS_DIR = build/android/obj
    MOC_DIR = build/android/obj
    RCC_DIR = build/android/obj
    UI_DIR = build/android/obj
}

DEFINES += USE_MOBILE
build_mobile {
    DEFINES += USE_MOBILE
}


SOURCES += main.cpp\
    commands.cpp \
    configparam.cpp \
    configparams.cpp \
    packet.cpp \
    vescinterface.cpp \
    vbytearray.cpp \
    utility.cpp \
    tcpserversimple.cpp

HEADERS  += commands.h \
    configparam.h \
    configparams.h \
    datatypes.h \
    packet.h \
    vescinterface.h \
    vbytearray.h \
    utility.h \
    tcpserversimple.h
    
contains(DEFINES, HAS_BLUETOOTH) {
    SOURCES += bleuart.cpp
    HEADERS += bleuart.h
}

include(widgets/widgets.pri)
include(lzokay/lzokay.pri)
include(mobile/mobile.pri)

RESOURCES += res.qrc
RESOURCES += res_config.qrc

DISTFILES += \
    android/AndroidManifest.xml \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradlew \
    android/res/values/libs.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.properties

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

