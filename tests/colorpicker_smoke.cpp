#include "NgPost.h"
#include "hmi/MainWindow.h"
#include <QApplication>
#include <QColorDialog>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <iostream>
#include <stdexcept>

class SmokeApp : public NgPost
{
public:
    using NgPost::NgPost;
    MainWindow *window() const { return _hmi; }
};

static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "Cannot create isolated fixture");
    require(file.write(contents) == contents.size(), "Cannot write isolated fixture");
}

static QByteArray readFile(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read test config");
    return file.readAll();
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    SmokeApp app(argc, argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    try
    {
        const QString directory = QCoreApplication::applicationDirPath();
        // This executable must run from its dedicated artifacts build directory.
        require(directory.contains("/artifacts/"), "Refusing to run outside artifacts");
        const QString config = QDir(directory).filePath("ngPost.conf");
        writeFile(QDir(directory).filePath("portable.mode"), "");
        const bool restart = qEnvironmentVariableIsSet("COLORPICKER_SMOKE_RESTART");
        if (!restart)
            writeFile(config, "lang = NL\nTHEME_COLOR = #AD37C9\nnzbPath = " + directory.toUtf8() + "\n");
        const QString parseError = app.parseDefaultConfig();
        if (!parseError.isEmpty())
            throw std::runtime_error(parseError.toStdString());
        MainWindow *window = app.window();
        window->init(&app);
        app.changeLanguage("nl");
        window->show();
        require(QMetaObject::invokeMethod(window, "onOpenPreferencesDialog"), "Preferences unavailable");
        QApplication::processEvents();
        auto *pick = window->findChild<QPushButton *>("themeColorButton");
        auto *reset = window->findChild<QPushButton *>("themeColorResetButton");
        auto *mode = window->findChild<QPushButton *>("themeButton");
        require(pick && reset && mode, "Theme controls missing");
        require(pick->isVisible() && reset->isVisible(), "Theme controls hidden");
        auto *scroll = pick->window()->findChild<QScrollArea *>("settingsScrollArea");
        require(scroll, "Preferences scroll area missing");
        scroll->ensureWidgetVisible(pick);
        const QColor expected(restart ? "#FFFF00" : "#AD37C9");
        require(qApp->palette().color(QPalette::Highlight) == expected, "Saved color not restored");
        require(pick->text() == expected.name().toUpper(), "Swatch does not show current color");

        if (!restart)
        {
            bool dialogFound = false;
            QTimer::singleShot(0, [&]() {
                for (QWidget *widget : QApplication::topLevelWidgets())
                    if (auto *dialog = qobject_cast<QColorDialog *>(widget))
                    {
                        dialogFound = true;
                        dialog->setCurrentColor(QColor("#FFFF00"));
                        dialog->accept();
                    }
            });
            pick->click();
            require(dialogFound, "Picker button did not open color dialog");
            require(qApp->palette().color(QPalette::Highlight) == QColor("#FFFF00"), "Chosen color not applied");
            require(qApp->palette().color(QPalette::HighlightedText) == QColor(Qt::black), "Bright accent text unreadable");
            require(readFile(config).contains("THEME_COLOR = #FFFF00"), "Chosen color not persisted");
            const QByteArray beforeCancel = readFile(config);
            QTimer::singleShot(0, []() {
                for (QWidget *widget : QApplication::topLevelWidgets())
                    if (auto *dialog = qobject_cast<QColorDialog *>(widget))
                    {
                        dialog->setCurrentColor(Qt::red);
                        dialog->reject();
                    }
            });
            pick->click();
            require(readFile(config) == beforeCancel, "Cancel modified config");
            require(qApp->palette().color(QPalette::Highlight) == QColor("#FFFF00"), "Cancel modified color");
            mode->click();
            require(qApp->palette().color(QPalette::Highlight) == QColor("#FFFF00"), "Light mode lost color");
            mode->click();
            require(qApp->palette().color(QPalette::Highlight) == QColor("#FFFF00"), "Dark mode lost color");
            app.saveConfig();
            require(readFile(config).contains("THEME_COLOR = #FFFF00"), "Save Config removed chosen color");
            scroll->ensureWidgetVisible(pick);
            pick->window()->grab().save(QDir(directory).filePath("preferences-colorpicker.png"));
        }
        else
        {
            reset->click();
            require(qApp->palette().color(QPalette::Highlight) == QColor("#0A66C2"), "Reset did not restore default");
            require(!reset->isEnabled(), "Reset remains enabled without custom color");
            require(readFile(config).contains("#THEME_COLOR = #1E90FF"), "Reset not persisted");
            pick->window()->grab().save(QDir(directory).filePath("preferences-default.png"));
        }
        std::cout << (restart ? "PASS: fresh-process restore and reset\n" : "PASS: visibility, restore, choose, cancel, light/dark, contrast and save\n");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
