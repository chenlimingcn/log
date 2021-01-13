include (../../env.pri)

TEMPLATE = lib

TARGET = $$qtLibraryTarget(log)

DEFINES += LOG_LIB UNICODE _UNICODE QT_NO_STL_WCHAR
win32 {
DEFINES += _CRT_SECURE_NO_WARNINGS
}

DESTDIR 	= $$LIB_PATH
DLLDESTDIR 	= $$BIN_PATH
OBJECTS_DIR = $$BUILD_PATH/$$TARGET
Debug{
OBJECTS_DIR = $$BUILD_PATH/$$TARGET/debug
MOC_DIR 	= $$BUILD_PATH/$$TARGET/debug/moc

}
Release {
OBJECTS_DIR = ../../build/$$TARGET/release
MOC_DIR 	= ../../build/$$TARGET/release/moc
}
UI_DIR      = $$OBJECTS_DIR

INCLUDEPATH += . \
		$$INCLUDE_PATH
win32 {
INCLUDEPATH += $$THIRDPATY_PATH/pthreads_win32_2.9.1
}


LIBS += -L$$LIB_PATH
win32 {
LIBS += -lws2_32 -l$$qtLibraryTarget(pthread)
}
unix{
LIBS += -lrt
}

QT 		-= core gui widgets
CONFIG  += debug_and_release
win32 {
QMAKE_CFLAGS            -= -Zc:wchar_t-
QMAKE_CXXFLAGS_DEBUG -= -Zc:wchar_t-
QMAKE_CXXFLAGS_RELEASE -=  -Zc:wchar_t-
QMAKE_CXXFLAGS_DEBUG += /Od
QMAKE_CXXFLAGS += -MP
}

# Input
HEADERS += $$INCLUDE_PATH/log/*.h \
		log.pro \
		*.h \
		*.hpp

SOURCES += *.c *.cpp

# install
win32 {
	target.path=$$INSTALL_PATH/bin
}
unix {
	target.path=$$INSTALL_PATH/install/lib
}
INSTALLS += target
