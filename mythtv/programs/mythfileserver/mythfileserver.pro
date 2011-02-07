include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

QT += network xml sql

TEMPLATE = app
CONFIG += thread
TARGET = mythfileserver
target.path = $${PREFIX}/bin
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += main_helpers.h servecontext.h

SOURCES += main.cpp main_helpers.cpp servercontext.cpp

