#-------------------------------------------------
#
# Editor project.
# Output: .exe
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG( debug, debug|release ) {
    TARGET = x12edd
} else {
    TARGET = x12ed
}

TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
#DEFINES += ADS_IMPORT
#DEFINES += ADS_NAMESPACE_ENABLED

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++17

# execubale
CONFIG( debug, debug|release ) {
    DESTDIR = $$PWD/../../build/Debug
} else {
    DESTDIR = $$PWD/../../build/Release
}

# objs
CONFIG( debug, debug|release ) {
    OBJECTS_DIR = $$PWD/../../build/editor-obj/Debug
} else {
    OBJECTS_DIR = $$PWD/../../build/editor-obj/Release
}

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/mainwindow.cpp \
    $$PWD/editorcore.cpp \
    $$PWD/renderwidget.cpp \
    $$PWD/icontitlewidget.cpp \
    $$PWD/consolewidget.cpp \
    $$PWD/consolewidget.cpp \
    $$PWD/scenewidget.cpp \
    $$PWD/treemodel.cpp \
    $$PWD/treenode.cpp \
    $$PWD/debugwidget.cpp \
    $$PWD/parameterswidget.cpp \
    $$PWD/selectmeshdialog.cpp \
    $$PWD/manipulators/manipulatortranslator.cpp \
    $$PWD/editor_common.cpp \
    $$PWD/manipulators/imanipulator.cpp \
    $$PWD/projectwidget.cpp \
    #$$PWD/propertieswidgets/camerapropertywidget.cpp \
    #$$PWD/propertieswidgets/modelpropertywidget.cpp \
    #$$PWD/propertieswidgets/lightpropertywidget.cpp \
    $$PWD/settings.cpp \
    $$PWD/texturelineedit.cpp \
    $$PWD/importthread.cpp \
    $$PWD/manipulators/manipulatorrotator.cpp \
    $$PWD/manipulators/manipulatorscale.cpp \
    $$PWD/mylineedit.cpp

HEADERS += \
    $$PWD/mainwindow.h \
    $$PWD/editorcore.h \
    $$PWD/renderwidget.h \
    $$PWD/icontitlewidget.h \
    $$PWD/consolewidget.h \
    $$PWD/consolewidget.h \
    $$PWD/scenewidget.h \
    $$PWD/treemodel.h \
    $$PWD/treenode.h \
    $$PWD/debugwidget.h \
    $$PWD/parameterswidget.h \
    $$PWD/myspinbox.h \
    $$PWD/mylineedit.h \
    $$PWD/selectmeshdialog.h \
    $$PWD/manipulators/manipulatortranslator.h \
    $$PWD/editor_common.h \
    $$PWD/manipulators/imanipulator.h \
    $$PWD/projectwidget.h \
    #$$PWD/propertieswidgets/camerapropertywidget.h \
    #$$PWD/propertieswidgets/modelpropertywidget.h \
    $$PWD/colorwidget.h \
    #$$PWD/propertieswidgets/lightpropertywidget.h \
    $$PWD/settings.h \
    $$PWD/texturelineedit.h \
    $$PWD/importthread.h \
    $$PWD/manipulators/manipulatorrotator.h \
    $$PWD/manipulators/manipulatorscale.h

INCLUDEPATH += "$$PWD/thirdparty/Qt-Advanced-Docking-System/AdvancedDockingSystem/include"
DEPENDPATH += "$$PWD/thirdparty/Qt-Advanced-Docking-System/AdvancedDockingSystem/include"

FORMS += \
    $$PWD/mainwindow.ui \
    $$PWD/consolewidget.ui \
    $$PWD/scenewidget.ui \
    $$PWD/debugwidget.ui \
    $$PWD/parameterswidget.ui \
    $$PWD/selectmeshdialog.ui \
    $$PWD/projectwidget.ui \
    #$$PWD/propertieswidgets/camerapropertywidget.ui \
    #$$PWD/propertieswidgets/modelpropertywidget.ui \
   #$$PWD/propertieswidgets/lightpropertywidget.ui \
    $$PWD/settings.ui \
    $$PWD/texturelineedit.ui

RC_FILE = ../../resources/editor/resources.rc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin1
else: unix:!android: target.path = /opt/$${TARGET}/bin1
!isEmpty(target.path): INSTALLS += target

CONFIG( debug, debug|release ) {
    # debug
    win32:LIBS += "$$PWD/../../build/Debug/AdvancedDockingSystemd.lib"
    # force relink
    PRE_TARGETDEPS += $$PWD/../../build/Debug/AdvancedDockingSystemd.lib
} else {
    # release
    win32:LIBS += "$$PWD/../../build/Release/AdvancedDockingSystem.lib"
    # force relink
    PRE_TARGETDEPS += $$PWD/../../build/Release/AdvancedDockingSystem.lib
}

DISTFILES +=

RESOURCES += \
    $$PWD/../../resources/editor/rm2.qrc

# Engine
INCLUDEPATH += $$PWD/../../include/engine/system
INCLUDEPATH += $$PWD/../../include/engine/x12
INCLUDEPATH += $$PWD/../../include/math
INCLUDEPATH += $$PWD/../../3rdparty/d3dx12

DEPENDPATH += $$PWD/../../include/engine/system
DEPENDPATH += $$PWD/../../include/engine/x12
DEPENDPATH += $$PWD/../../include/math
DEPENDPATH += $$PWD/../../3rdparty/d3dx12


CONFIG( debug, debug|release ) {
    # debug
    LIBS += -L$$PWD/../../build/Debug/ -lengine
    # force relink
    PRE_TARGETDEPS += $$PWD/../../build/Debug/engine.lib

} else {
    # release
    LIBS += -L$$PWD/../../build/Release/ -lengine
    # force relink
    PRE_TARGETDEPS += $$PWD/../../build/Release/engine.lib
}

# visual leak detector
#INCLUDEPATH += $$(VLD_ROOT)/include
#DEPENDPATH += $$(VLD_ROOT)/include
#LIBS += -L$$(VLD_ROOT)/lib/Win64 -lvld

# Copy default editor settings
CONFIG( debug, debug|release ) {
 copydata.commands += $(COPY_FILE) \"$$shell_path($$PWD\\..\..\resources\editor\save\MainWindow.dat)\" \"$$shell_path($$PWD\\..\..\build\Debug)\" &&
 copydata.commands += $(COPY_FILE) \"$$shell_path($$PWD\\..\..\resources\editor\save\Theme.dat)\" \"$$shell_path($$PWD\\..\..\build\Debug)\" &&
 copydata.commands += $(COPY_FILE) \"$$shell_path($$PWD\\..\..\resources\editor\save\Windows.dat)\" \"$$shell_path($$PWD\\..\..\build\Debug)\"
} else {
 copydata.commands += $(COPY_FILE) \"$$shell_path($$PWD\\..\..\resources\editor\save\MainWindow.dat)\" \"$$shell_path($$PWD\\..\..\build\Release)\" &&
 copydata.commands += $(COPY_FILE) \"$$shell_path($$PWD\\..\..\resources\editor\save\Theme.dat)\" \"$$shell_path($$PWD\\..\..\build\Release)\" &&
 copydata.commands += $(COPY_FILE) \"$$shell_path($$PWD\\..\..\resources\editor\save\Windows.dat)\" \"$$shell_path($$PWD\\..\..\build\Release)\"
}
 first.depends = $(first) copydata
 export(first.depends)
 export(copydata.commands)
 QMAKE_EXTRA_TARGETS += first copydata

#message( "Executing qmake for editor..." )

