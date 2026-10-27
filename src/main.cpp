#include <QApplication>
#include <QCommandLineParser>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("onvif-client");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("local");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Клиент IP-камер с ONVIF-поиском. "
                       "--profile <имена> — подключить сохранённые камеры "
                       "(через запятую или повторно); --fullscreen — полноэкранный режим."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption profileOpt(
        QStringList() << "p" << "profile",
        QStringLiteral("Список сохранённых камер (имена или IP), через запятую."),
        QStringLiteral("names"));
    QCommandLineOption fsOpt(
        QStringList() << "f" << "fullscreen",
        QStringLiteral("Запустить в полноэкранном режиме."));
    parser.addOption(profileOpt);
    parser.addOption(fsOpt);
    parser.process(app);

    MainWindow w;
    w.show();

    QStringList names;
    for (const QString &raw : parser.values(profileOpt))
        for (const QString &s : raw.split(',', Qt::SkipEmptyParts)) {
            const QString t = s.trimmed();
            if (!t.isEmpty() && !names.contains(t))
                names << t;
        }

    if (!names.isEmpty() || parser.isSet(fsOpt))
        w.startProfile(names, parser.isSet(fsOpt));

    return app.exec();
}
