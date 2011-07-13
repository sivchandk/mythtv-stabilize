include ( ../../settings.pro)
include ( ../../version.pro)
include ( ../programs-libs.pro)

QT += sql network xml
CONFIG -= opengl

TEMPLATE = app
CONFIG += thread
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
SOURCES += main.cpp jobinforun.cpp jobsocket.cpp clientsocket.cpp commandlineparser.cpp

HEADERS += jobinforun.h jobsocket.h clientsocket.h commandlineparser.h
