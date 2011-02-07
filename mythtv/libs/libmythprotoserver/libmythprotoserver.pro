include ( ../../settings.pro )

QT += network sql

INCLUDEPATH += ../libmyth
INCLUDEPATH += ../libmythbase
INCLUDEPATH += ../libmythtv
INCLUDEPATH += ../libmythui

DEPENDPATH  += ../libmythbase
DEPENDPATH  += ../libmyth
DEPENDPATH  += ../libmythtv
DEPENDPATH  += ../libmythui

LIBS      += -L../libmyth -lmyth-$$LIBVERSION
LIBS      += -L../libmythbase -lmythbase-$$LIBVERSION
LIBS      += -L../libmythtv -lmythtv-$$LIBVERSION
LIBS      += -L../libmythui -lmythui-$$LIBVERSION
LIBS      += -L$${LIBDIR}/qt4

TEMPLATE = lib
TARGET = mythprotoserver-$$LIBVERSION
CONFIG += thread
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += mainserver.h     handler.h       basehandler.h        server.h

SOURCES += mainserver.cpp   handler.cpp     basehandler.cpp      server.cpp


LIBS += $$EXTRA_LIBS

include ( ../libs-targetfix.pro )
