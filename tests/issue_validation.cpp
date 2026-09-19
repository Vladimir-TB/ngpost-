#include "NgPost.h"
#include "hmi/MainWindow.h"
#include "hmi/AutoPostWidget.h"
#include "hmi/PostingWidget.h"
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QLineEdit>
#include <QTimer>
#include <QProgressDialog>
#include <QPushButton>
#include <QTabWidget>
#include <QThread>
#include <algorithm>
#include <iostream>
#include <stdexcept>

class TestApp : public NgPost {
public:
    using NgPost::NgPost;
    MainWindow *window() const { return _hmi; }
};
static void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
static void write(const QString &path, const QByteArray &data) {
    QFile file(path); check(file.open(QIODevice::WriteOnly), "fixture open");
    check(file.write(data) == data.size(), "fixture write");
}
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    TestApp app(argc, argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    try {
        const QString dir = QCoreApplication::applicationDirPath();
        check(dir.contains("/artifacts/"), "isolated artifacts directory required");
        write(dir + "/portable.mode", "");
        write(dir + "/ngPost.conf", "lang = EN\nnzbPath = " + dir.toUtf8() + "\n");
        check(app.parseDefaultConfig().isEmpty(), "parse fixture config");
        auto *window = app.window(); window->init(&app); window->show();
        auto *automatic = window->autoWidget();
        const int count = qEnvironmentVariableIntValue("NGPOST_TEST_COUNT") > 0 ? qEnvironmentVariableIntValue("NGPOST_TEST_COUNT") : 500;
        const QString folders = dir + QString("/folders-%1").arg(count);
        for (int i = 0; i < count; ++i) {
            const QString folder = folders + QString("/post-%1").arg(i, 4, 10, QChar('0'));
            QDir().mkpath(folder);
            for (int j = 0; j < 1 + i % 3; ++j) write(folder + QString("/file-%1.txt").arg(j), QByteArray(1024, 'x'));
        }
        automatic->findChild<QLineEdit *>("autoDirEdit")->setText(folders);
        automatic->findChild<QCheckBox *>("compressCB")->setChecked(true);
        automatic->findChild<QCheckBox *>("startJobsCB")->setChecked(false);
        QApplication::processEvents();
        const int initial = window->findChildren<PostingWidget *>().size();
        const int selected = window->findChild<QTabWidget *>("postTabWidget")->currentIndex();
        QElapsedTimer total, tick; total.start(); tick.start();
        QList<qint64> gaps;
        QTimer heartbeat; heartbeat.setInterval(10);
        QObject::connect(&heartbeat, &QTimer::timeout, [&] { gaps << tick.restart(); });
        heartbeat.start();
        check(QMetaObject::invokeMethod(automatic, "onScanAutoDirClicked"), "scan slot");
        // Async scanning is finished when the public operation becomes idle.
        while (automatic->property("batchBusy").toBool()) { QApplication::processEvents(); QThread::msleep(1); }
        const auto scanMs = total.elapsed();
        check(QMetaObject::invokeMethod(automatic, "onGenQuickPosts"), "generate slot");
        while (window->findChildren<PostingWidget *>().size() < initial + count || automatic->property("batchBusy").toBool()) {
            QApplication::processEvents(); QThread::msleep(1);
            check(total.elapsed() < 180000, "batch timeout");
        }
        QApplication::processEvents(); gaps << tick.elapsed(); heartbeat.stop();
        std::sort(gaps.begin(), gaps.end());
        const qint64 maxGap = gaps.last();
        const qint64 p95 = gaps[qMin(gaps.size()-1, qsizetype(gaps.size()*0.95))];
        std::cout << "BATCH count=" << count << " scan_ms=" << scanMs << " total_ms=" << total.elapsed()
                  << " max_gui_gap_ms=" << maxGap << " p95_gui_gap_ms=" << p95 << std::endl;
        check(window->findChildren<PostingWidget *>().size() == initial + count, "exact session count");
        check(window->findChild<QTabWidget *>("postTabWidget")->currentIndex()==selected,"batch preserves selected tab");
        if (qEnvironmentVariableIsSet("NGPOST_ENFORCE_RESPONSIVENESS")) {
            check(maxGap <= 500, "GUI gap exceeds 500 ms");
            check(p95 <= 100, "GUI p95 exceeds 100 ms");
        }
        const int beforeCancel=window->findChildren<PostingWidget *>().size();
        QElapsedTimer cancel; cancel.start();
        QMetaObject::invokeMethod(automatic,"onGenQuickPosts");
        QTimer::singleShot(20,window,[window] {
            auto *progress=window->findChild<QProgressDialog *>("folderBatchProgress");
            if(progress) if(auto *button=progress->findChild<QPushButton *>()) button->click();
        });
        while(automatic->property("batchBusy").toBool()) {
            QApplication::processEvents(); QThread::msleep(1); check(cancel.elapsed()<1000,"cancel deadline");
        }
        check(window->findChildren<PostingWidget *>().size()<beforeCancel+count,"cancellation stops generation");
        return 0;
    } catch (const std::exception &error) { std::cerr << "FAIL: " << error.what() << std::endl; return 1; }
}
