#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("JRTE Desktop");
    app.setOrganizationName("JRTE");

    MainWindow window;
    window.show();

    return app.exec();
}
