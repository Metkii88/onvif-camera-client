#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include "mainwindow.h"

// Встроенная тёмная тема (используется, если рядом с бинарником нет themes/style.qss)
static const char *BUILTIN_QSS = R"QSS(
* { outline: 0; }
QWidget { background: #1e232b; color: #dde3ea; font-size: 14px; }
QMainWindow, QDialog { background: #1e232b; }
QToolTip { background: #232a35; color: #eef2f7; border: 1px solid #46536a; padding: 6px; }

QGroupBox {
    border: 1px solid #3a4250; border-radius: 8px;
    margin-top: 16px; padding-top: 10px;
    font-weight: bold; font-size: 15px;
}
QGroupBox::title {
    subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #9fb4cc;
}

QPushButton {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3a4557, stop:1 #2d3644);
    color: #f2f6fa; border: 1px solid #4a5872; border-radius: 8px;
    padding: 9px 20px; font-size: 15px; font-weight: 600;
}
QPushButton:hover   { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #46536a, stop:1 #38455a); border-color: #6a7ca0; }
QPushButton:pressed { background: #232b38; }
QPushButton:disabled{ background: #262b33; color: #616a77; border-color: #333a45; }
QPushButton:checked { background: #2f6fed; border-color: #5b8ff7; color: white; }
QPushButton#btnPrimary {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3f7cf5, stop:1 #2f6fed);
    border: 1px solid #5b8ff7; color: white;
}
QPushButton#btnPrimary:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4f88f8, stop:1 #3a76f0); }

QLineEdit, QComboBox, QSpinBox, QListWidget, QTextEdit, QPlainTextEdit {
    background: #171b22; color: #eef2f7; border: 1px solid #3a4250;
    border-radius: 6px; padding: 7px 10px; font-size: 15px;
    selection-background-color: #2f6fed; selection-color: white;
}
QLineEdit:focus, QComboBox:focus, QListWidget:focus { border-color: #2f6fed; }
QComboBox::drop-down { border: none; width: 26px; }
QComboBox QAbstractItemView { background: #232a35; border: 1px solid #46536a; selection-background-color: #2f6fed; }

QListWidget::item { padding: 9px 8px; border-bottom: 1px solid #262d38; border-radius: 4px; }
QListWidget::item:selected { background: #2f6fed; color: white; }
QListWidget::item:hover { background: #2a3444; }

QCheckBox { spacing: 10px; font-size: 15px; }
QCheckBox::indicator { width: 20px; height: 20px; border-radius: 4px; border: 1px solid #4a5872; background: #171b22; }
QCheckBox::indicator:checked { background: #2f6fed; border-color: #5b8ff7; }

QHeaderView::section {
    background: #262d38; color: #cfd8e3; padding: 8px;
    border: none; border-bottom: 2px solid #3a4250; font-weight: bold;
}
QTableWidget {
    background: #171b22; alternate-background-color: #1c212a;
    gridline-color: #2a3240; border: 1px solid #3a4250; color: #eef2f7;
}
QTableWidget::item { padding: 6px; }
QTableWidget::item:selected { background: #2f6fed; color: white; }

QStatusBar { background: #171b22; color: #9aa7b8; }
QMenuBar { background: #171b22; color: #cfd8e3; }
QMenuBar::item:selected { background: #2f6fed; color: white; }
QMenu { background: #232a35; border: 1px solid #3a4250; }
QMenu::item { padding: 8px 32px; }
QMenu::item:selected { background: #2f6fed; color: white; }

QScrollBar:vertical { background: #171b22; width: 12px; margin: 0; }
QScrollBar::handle:vertical { background: #3a4250; border-radius: 6px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #4a5872; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
QScrollBar:horizontal { background: #171b22; height: 12px; margin: 0; }
QScrollBar::handle:horizontal { background: #3a4250; border-radius: 6px; min-width: 24px; }

QSplitter::handle { background: #2a3240; }
QLabel { background: transparent; }
)QSS";

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("onvif-client");
    app.setApplicationVersion("2.3.0");
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
    QCommandLineOption themeOpt(
        QStringList() << "t" << "theme",
        QStringLiteral("Путь к файлу темы (.qss)."),
        QStringLiteral("file"));
    parser.addOption(profileOpt);
    parser.addOption(fsOpt);
    parser.addOption(themeOpt);
    parser.process(app);

    // Тема: 1) --theme <файл.qss>  2) themes/style.qss рядом с бинарником
    //        3) /usr/share/onvif-camera-client/themes/style.qss  4) встроенная
    QString themePath;
    if (parser.isSet(themeOpt))
        themePath = parser.value(themeOpt);
    if (themePath.isEmpty()) {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            appDir + "/themes/style.qss",
            appDir + "/../themes/style.qss",
            "/usr/share/onvif-camera-client/themes/style.qss",
        };
        for (const QString &c : candidates) {
            if (QFile::exists(c)) { themePath = c; break; }
        }
    }
    QFile qss(themePath);
    if (!themePath.isEmpty() && qss.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    else
        app.setStyleSheet(BUILTIN_QSS);

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
