#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{

    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);
    QApplication::setAttribute(Qt::AA_ForceRasterWidgets,false);

    QFile file(":/new/prefix1/stylesheet.css");
    file.open(QFile::ReadOnly);
    QTextStream stream(&file);
    QString styleSheet = stream.readAll();
    a.setStyleSheet(styleSheet);
    file.close();

    qRegisterMetaType<Ball*>("Ball*");

    MainWindow w;
    w.show();

    return a.exec();
}
