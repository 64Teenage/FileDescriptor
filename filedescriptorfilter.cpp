#include "filterwidget.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QMainWindow window;

    FilterWidget *widget = new FilterWidget();
    window.setCentralWidget(widget);
    //window.resize(900, 600);
    window.show();

    return app.exec();
}
