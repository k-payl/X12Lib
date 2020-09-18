#-------------------------------------------------
#
# AdvancedDockingSystem project.
# Output: .lib
#
#-------------------------------------------------

CONFIG( debug, debug|release ) {
    DESTDIR = $$PWD/../../../../../build/Debug
} else {
    DESTDIR = $$PWD/../../../../../build/Release
}


CONFIG( debug, debug|release ) {
    TARGET = AdvancedDockingSystemd
} else {
   TARGET = AdvancedDockingSystem
}
TEMPLATE = lib
VERSION = 1.0.0

#CONFIG += adsBuildShared

QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
#greaterThan(QT_MAJOR_VERSION, 4): DEFINES += ADS_NAMESPACE_ENABLED

adsBuildShared {
    CONFIG += shared
    DEFINES += ADS_EXPORT
}
!adsBuildShared {
    CONFIG += staticlib
}

INCLUDEPATH += $$PWD/include

windows {
    # MinGW
    *-g++* {
        QMAKE_CXXFLAGS += -std=c++11
        QMAKE_CXXFLAGS += -Wall -Wextra -pedantic
    }
    # MSVC
    *-msvc* {
    }
}

RESOURCES += \
    res/ads.qrc

include(AdvancedDockingSystem.pri)
# objs
CONFIG( debug, debug|release ) {
    OBJECTS_DIR = $$PWD/../../../../../build/AdvancedDockingSystem-obj/debug
} else {
    OBJECTS_DIR = $$PWD/../../../../../build/AdvancedDockingSystem-obj/release
}

#message("Executing qmake for AdvancedDockingSystem..." )
#message(Object path for AdvancedDockingSystem = $$OBJECTS_DIR)
#message(Dest path for AdvancedDockingSystem = $$DESTDIR)

