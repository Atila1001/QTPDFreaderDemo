#include "Pdfreader.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Pdfreader w;
    w.show();
    return a.exec();
}
