#include <QApplication>
#include <QTextEdit>
#include <QtGui/QGuiApplication>
#include <catch2/catch_session.hpp>

int main(int argc, char** argv)
{
    int _argc = 0;
    QApplication app(_argc, nullptr);
    return Catch::Session().run(argc, argv);
}
