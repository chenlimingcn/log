####################################################################################################
# DEVHOME It is the home directory of the project
####################################################################################################

DEVHOME = ../.. #TODO find the abstract

# set the environment variables of bin lib include directory etc.
BIN_PATH = $$DEVHOME/bin
BUILD_PATH = $$DEVHOME/build
LIB_PATH = $$DEVHOME/lib
INCLUDE_PATH = $$DEVHOME/include
SOURCE_PATH = $$DEVHOME/source
DEBUG_PATH = $$BUILD_PATH/debug
RELEASE_PATH = $$BUILD_PATH/release
INSTALL_PATH = $$DEVHOME/install

# THIRDPATY_PATH
THIRDPATY_PATH = $$(THIRDPATY_HOME)
#log($$THIRDPATY_PATH)
isEmpty(THIRDPATY_PATH){
	THIRDPATY_PATH = $$DEVHOME/thirdparty
#	log(' empyty ')
}

QMAKE_LIBDIR *= $$LIB_PATH
DEPENDPATH *= . $$INCLUDE_PATH
INCLUDEPATH *= . $$INCLUDE_PATH
