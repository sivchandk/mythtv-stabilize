include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += network sql xml

TEMPLATE = lib
CONFIG += plugin thread warn_on debug
TARGET = mythsmolt
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

uifiles.path = $${PREFIX}/share/mythtv/themes/default
uifiles.files = smolt-ui.xml
installfiles.path = $${PREFIX}/share/mythtv
installfiles.files = smolt-ui.xml

scriptfiles.path = $${PREFIX}/share/mythtv/mythsmolt/scripts
scriptfiles.files = ../mythsmolt/scripts/*

INSTALLS +=  scriptfiles


INSTALLS += uifiles

# Input
HEADERS += smoltlog.h
SOURCES += main.cpp smoltlog.cpp

DEFINES += MPLUGIN_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}

include ( ../../libs-targetfix.pro )
