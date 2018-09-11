TEMPLATE = lib

isEqual(TEMPLATE,app) {
CONFIG += console
SOURCES += test.cpp
}
else {
CONFIG += static
}

CONFIG -= qt
INCLUDEPATH += .

# Input
HEADERS += ccronexpr.h zscheduler.h
SOURCES += ccronexpr.c zscheduler.cpp

