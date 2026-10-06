#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDir>


//程序的执行入口，打开对话框
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);


    QFile styleFile("E:/Work_File/MyQtProject/AI_Vision/style.qss");  // 如果用相对路径

    if (styleFile.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        a.setStyleSheet(styleSheet);  // 应用到整个应用
        styleFile.close();
    }

    qDebug() << "当前工作目录:" << QDir::currentPath();  // 看看程序在哪运行
    qDebug() << "文件是否存在:" << QFile::exists("style.qss");

    if (styleFile.open(QFile::ReadOnly)) {
        qDebug() << "文件打开成功";
        QString styleSheet = QLatin1String(styleFile.readAll());
        qDebug() << "样式内容长度:" << styleSheet.length();
        qDebug() << "样式内容预览:" << styleSheet.left(100);  // 打印前100个字符

        a.setStyleSheet(styleSheet);
        styleFile.close();
        qDebug() << "样式加载完成";
    } else {
        qDebug() << "样式文件打开失败，错误信息:" << styleFile.errorString();
    }

    QFont defaultFont;
#ifdef Q_OS_WIN
    defaultFont = QFont("Microsoft YaHei", 9);
#endif
    a.setFont(defaultFont);


    MainWindow w;
    w.show();
    return a.exec();
}
