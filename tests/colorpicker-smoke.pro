# Link the freshly built production objects, replacing only the application entry point.
include(../src/ngPost.pro)
SOURCES = $$PWD/colorpicker_smoke.cpp
HEADERS =
FORMS =
RESOURCES =
RC_ICONS =
TARGET = colorpicker-smoke
CONFIG -= windows
CONFIG += console
productionObjects = $$files($$PWD/../build-qt6/release/*.obj)
for(object, productionObjects) {
    !contains(object, .*/main[.]obj$) {
        LIBS += $$object
        PRE_TARGETDEPS += $$object
    }
}
