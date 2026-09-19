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
#include <QTextBrowser>
#include <cmath>
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

static double contrast(const QColor &first, const QColor &second)
{
    auto luminance = [](const QColor &color) {
        auto linear = [](double value) { return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4); };
        return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
    };
    const double a = luminance(first), b = luminance(second);
    return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
}

static void chooseColor(QPushButton *pick, const QColor &color)
{
    bool opened = false;
    QTimer::singleShot(0, [&]() {
        for (QWidget *widget : QApplication::topLevelWidgets())
            if (auto *dialog = qobject_cast<QColorDialog *>(widget))
            {
                opened = true;
                dialog->setCurrentColor(color);
                dialog->accept();
            }
    });
    pick->click();
    require(opened, "Picker did not open during theme matrix");
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
        require(QMetaObject::invokeMethod(window, "onOpenHelpDialog"), "Help unavailable");
        auto *help = window->findChild<QTextBrowser *>("helpBrowser");
        require(help && help->toPlainText().contains("5.1.2"), "Release notes missing from Help");
        require(help->toPlainText().contains("Verbeteringen") && help->toPlainText().contains("Opgeloste problemen"), "Dutch release notes missing");
        QApplication::processEvents();
        help->window()->grab().save(QDir(directory).filePath("help-5.1.2-nl.png"));
        app.changeLanguage("en");
        QApplication::processEvents();
        require(help->toPlainText().contains("Improvements") && help->toPlainText().contains("Fixes"), "English release notes missing");
        help->window()->grab().save(QDir(directory).filePath("help-5.1.2-en.png"));
        help->window()->hide();
        app.changeLanguage("nl");
        QApplication::processEvents();
        std::cout << "PASS: Help release notes in Dutch and English\n";
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
            for (const QString &hex : {"#AD37C9", "#E53935", "#20C060", "#126BDB", "#808080", "#000000", "#FFFFFF", "#FFFF00"})
            {
                const QColor color(hex);
                chooseColor(pick, color);
                for (bool dark : {true, false})
                {
                    if (mode->isChecked() != dark)
                        mode->click();
                    QApplication::processEvents();
                    const QPalette pal = qApp->palette();
                    for (QPalette::ColorRole role : {QPalette::Window, QPalette::Base, QPalette::Button, QPalette::Mid})
                    {
                        const QColor surface = pal.color(role);
                        if (color.hslSaturationF() > 0.1)
                            require(qAbs(surface.hslHueF() - color.hslHueF()) < 0.02, "UI surface retained a foreign hue");
                        else
                            require(surface.hslSaturationF() < 0.02, "Neutral theme retained colored surfaces");
                    }
                    require(contrast(pal.color(QPalette::WindowText), pal.color(QPalette::Window)) >= 4.5, "Window text contrast failed");
                    require(contrast(pal.color(QPalette::Text), pal.color(QPalette::Base)) >= 4.5, "Input text contrast failed");
                    require(contrast(pal.color(QPalette::PlaceholderText), pal.color(QPalette::Button)) >= 4.5, "Muted text contrast failed");
                    require(contrast(pal.color(QPalette::Link), pal.color(QPalette::Button)) >= 4.5, "Link contrast failed");
                    require(contrast(pal.color(QPalette::HighlightedText), pal.color(QPalette::Highlight)) >= 4.5, "Selection contrast failed");
                    if (hex == "#AD37C9" || hex == "#20C060")
                    {
                        const QString prefix = hex.mid(1) + (dark ? "-dark" : "-light");
                        scroll->ensureWidgetVisible(pick);
                        pick->window()->grab().save(QDir(directory).filePath(prefix + "-preferences.png"));
                        for (const QString &name : {"overviewNavButton", "quickNavButton", "autoNavButton", "activityNavButton"})
                        {
                            auto *nav = window->findChild<QPushButton *>(name);
                            require(nav, "Navigation button missing");
                            nav->click();
                            QApplication::processEvents();
                            window->grab().save(QDir(directory).filePath(prefix + "-" + name + ".png"));
                        }
                    }
                }
            }
            if (!mode->isChecked())
                mode->click();
            std::cout << "PASS: 8 colors x light/dark; all surface hues and text contrast; four views rendered\n";
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
