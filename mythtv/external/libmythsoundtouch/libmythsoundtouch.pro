include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythsoundtouch-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt

INCLUDEPATH += . ../../

# Like libavcodec, debug mode on x86 runs out of registers on some GCCs.
!win32-msvc* { 
  !profile:QMAKE_CXXFLAGS_DEBUG += -O
}

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# Input
HEADERS += AAFilter.h
HEADERS += cpu_detect.h
HEADERS += FIRFilter.h
HEADERS += RateTransposer.h
HEADERS += TDStretch.h
HEADERS += STTypes.h

SOURCES += AAFilter.cpp
SOURCES += FIRFilter.cpp
SOURCES += FIFOSampleBuffer.cpp
SOURCES += RateTransposer.cpp
SOURCES += SoundTouch.cpp
SOURCES += TDStretch.cpp

win32-msvc* {

  SOURCES += cpu_detect_x86_win.cpp

} else {

  SOURCES += cpu_detect_x86_gcc.cpp

  contains(ARCH_X86, yes) {
        DEFINES += ALLOW_SSE2 ALLOW_SSE3
        SOURCES += sse_gcc.cpp
  }
}

mingw {
    dll : contains( TEMPLATE, lib ) {

        # Qt under Linux/UnixMac OS X builds libBlah.a and libBlah.so,
        # but is using the Windows defaults  libBlah.a and    Blah.dll.
        #
        # So that our dependency targets work between SUBDIRS, override:
        #
        TARGET = lib$${TARGET}


        # Windows doesn't have a nice variable like LD_LIBRARY_PATH,
        # which means make install would be broken without extra steps.
        # As a workaround, we store dlls with exes. Also improves debugging!
        #
        target.path = $${PREFIX}/bin
    }
}

