QT       += core gui
QT       += core widgets network

greaterThan(QT_MAJOR_VERSION, 4): QT += core widgets serialport

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    businessworker.cpp \
    calibrationdialog.cpp \
    cameramanager.cpp \
    camparasetdialog.cpp \
    configmanager.cpp \
    detectionconfigdialog.cpp \
    detectiondisplaydialog.cpp \
    detectionworker.cpp \
    filecleaner.cpp \
    layoutmanager.cpp \
    main.cpp \
    mainwindow.cpp \
    messagequeue.cpp \
    modbusmasterdialog.cpp \
    modbusslavedialog.cpp \
    mvcameraqt.cpp \
    parammanager.cpp \
    tool.cpp

HEADERS += \
    businessworker.h \
    calibrationdialog.h \
    cameramanager.h \
    camparasetdialog.h \
    configmanager.h \
    configstruct.h \
    detectionconfigdialog.h \
    detectiondisplaydialog.h \
    detectionworker.h \
    filecleaner.h \
    layoutmanager.h \
    mainwindow.h \
    messagequeue.h \
    modbusmasterdialog.h \
    modbusslavedialog.h \
    mvcameraqt.h \
    parammanager.h \
    tool.h

FORMS += \
    detectionconfigdialog.ui \
    mainwindow.ui \
    modbusslavedialog.ui

# ===== 海康SDK路径 =====
MVS_SDK_PATH = D:/software/mvs/MVS/Development
INCLUDEPATH += $$MVS_SDK_PATH/Includes
LIBS += -L$$MVS_SDK_PATH/Libraries/win64 -lMvCameraControl

INCLUDEPATH += D:/software/opencv/opencv/build/include
CONFIG(debug, debug|release) {
    # ===== Debug 模式 =====
    message("Building in DEBUG mode")
    # OpenCV - Debug 库
    LIBS += -LD:/software/opencv/opencv/build/x64/vc16/lib -lopencv_world4100d
} else {
    # ===== Release 模式 =====
    message("Building in RELEASE mode")
    # OpenCV - Release 库
    LIBS += -LD:/software/opencv/opencv/build/x64/vc16/lib -lopencv_world4100
}

INCLUDEPATH += E:/Work_File/MyVisualStudioLib/SegmentationLib/SegmentationLib
LIBS += -LE:/Work_File/MyVisualStudioLib/SegmentationLib/x64/Release -lSegmentationLib

# ===== 资源文件 =====
RESOURCES += \
    resources.qrc

# ===== 样式表 =====
DISTFILES += \
    style.qss

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RC_ICONS = ai_version.ico
