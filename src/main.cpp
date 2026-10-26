#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("onvif-client");
    app.setOrganizationName("local");

    MainWindow w;
    w.show();

    return app.exec();
}
