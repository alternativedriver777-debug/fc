QT += core gui widgets charts

CONFIG += c++17

SOURCES += \
    crate.cpp \
    ltr11.cpp \
    ltr114.cpp \
    ltr212.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    crate.h \
    ltr11.h \
    ltr114.h \
    ltr212.h \
    mainwindow.h \
    $$PWD/LTR/ltrapi.h \
    $$PWD/LTR/ltrapidefine.h \
    $$PWD/LTR/ltrapitypes.h \
    $$PWD/LTR/lwintypes.h \
    $$PWD/LTR/ltrapi_config.h \
    $$PWD/LTR/ltr11api.h \
    $$PWD/LTR/ltr114api.h \
    $$PWD/LTR/ltr212api.h \
    module.h


FORMS += mainwindow.ui

# Пути к ltrapi.dll и .a (на MinGW)
INCLUDEPATH += $$PWD/LTR

LIBS += $$PWD/LTR/libltrapi.a \
        $$PWD/LTR/ltrapi.dll \
        $$PWD/LTR/ltr11api.dll \
        $$PWD/LTR/ltr114api.dll \
        $$PWD/LTR/libltr11api.a \
        $$PWD/LTR/libltr114api.a \
        $$PWD/LTR/libltr212api.a \
        $$PWD/LTR/ltr212api.dll

#  libltrapi.a называется по прицнипу lib<name>.a (libltrapi.a -> -lltrapi)
