include ( ../mythconfig.mak )
include ( ../settings.pro )
include (../programs-libs.pro)
QT += network sql xml

TEMPLATE = lib
CONFIG += plugin thread
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

macx {
    QMAKE_LFLAGS += -flat_namespace -undefined suppress
}
