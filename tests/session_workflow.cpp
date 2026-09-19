#include "NgPost.h"
#include "PostingJob.h"
#include "hmi/MainWindow.h"
#include "hmi/PostingWidget.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QLineEdit>
#include <QLabel>
#include <QProcess>
#include <QPointer>
#include <QMessageBox>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTextEdit>
#include <QTimer>
#include <QUuid>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char *text) { if (!ok) throw std::runtime_error(text); }
static void write(const QString &path, const QByteArray &data) {
    QFile f(path); check(f.open(QIODevice::WriteOnly), "fixture open"); check(f.write(data)==data.size(), "fixture write");
}
static QByteArray read(const QString &path) { QFile f(path); check(f.open(QIODevice::ReadOnly), "output open"); return f.readAll(); }
template<class F> static void until(F done, int timeout=30000) {
    QElapsedTimer t; t.start();
    do { QApplication::processEvents(); QThread::msleep(1); check(t.elapsed()<timeout, "operation timeout"); } while (!done());
}
class TestApp : public NgPost { public: using NgPost::NgPost; MainWindow *window() { return _hmi; } };
class LocalNntp : public QTcpServer {
public:
    QList<QByteArray> articles;
    bool hold = false;
    LocalNntp() {
        check(listen(QHostAddress::LocalHost), "loopback NNTP listen");
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    auto buffer = socket->property("buffer").toByteArray() + socket->readAll();
                    while (!buffer.isEmpty()) {
                        if (socket->property("article").toBool()) {
                            const auto end = buffer.indexOf("\r\n.\r\n"); if (end<0) break;
                            articles << buffer.left(end); buffer.remove(0, end+5);
                            socket->setProperty("article", false); socket->write("240 article received\r\n");
                        } else {
                            const auto end = buffer.indexOf("\r\n"); if (end<0) break;
                            const auto command = buffer.left(end).trimmed().toUpper(); buffer.remove(0,end+2);
                            if (command.startsWith("AUTHINFO USER")) socket->write("381 password required\r\n");
                            else if (command.startsWith("AUTHINFO PASS")) socket->write("281 authenticated\r\n");
                            else if (command=="POST") { socket->setProperty("article", true); socket->write("340 send article\r\n"); }
                            else if (command=="QUIT") { socket->write("205 goodbye\r\n"); socket->disconnectFromHost(); }
                            else if (command=="MODE READER") socket->write("200 reader mode\r\n");
                            else socket->write("500 unsupported\r\n");
                        }
                    }
                    socket->setProperty("buffer",buffer);
                });
                if (!hold) socket->write("200 local test server ready\r\n");
            }
        });
    }
};
int main(int argc, char **argv) {
    if(argc>1 && QByteArray(argv[1])=="a") {
        QCoreApplication child(argc,argv); QTimer::singleShot(60000,&child,&QCoreApplication::quit); return child.exec();
    }
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    TestApp app(argc, argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    try {
        const QString base = QCoreApplication::applicationDirPath();
        check(base.contains("/artifacts/"), "isolated test directory");
        const QString dir = base + "/run-" + QUuid::createUuid().toString(QUuid::Id128);
        QDir().mkpath(dir);
        LocalNntp server;
        write(base+"/portable.mode", "");
        const QByteArray prepare=qEnvironmentVariable("NGPOST_TEST_PREPARE_PACKING","false").toUtf8();
        write(base+"/ngPost.conf", "LANG=EN\nTHREAD=1\nPREPARE_PACKING="+prepare+"\nnzbPath="+dir.toUtf8()+
              "\n[server]\nhost=127.0.0.1\nport="+QByteArray::number(server.serverPort())+"\nssl=false\nconnection=1\nenabled=true\n");
        check(app.parseDefaultConfig().isEmpty(), "test config");
        auto *w = app.window(); w->init(&app); w->show();
        QApplication::processEvents();
        auto *notice=w->findChild<QLabel *>("postingResponsibilityLabel");
        check(notice && notice->isVisible() && !notice->text().isEmpty(),"visible responsibility notice");
        w->grab().save(base+"/responsibility-and-actions.png");
        auto session = [&](const QString &name) {
            const auto path = dir+"/"+name+".txt"; write(path, QByteArray(1024,'x'));
            auto *p = w->addNewQuickTab(0,{QFileInfo(path)},false);
            p->findChild<QCheckBox *>("compressCB")->setChecked(false);
            p->findChild<QCheckBox *>("par2CB")->setChecked(false);
            p->findChild<QLineEdit *>("nzbFileEdit")->setText(dir+"/"+name+".nzb");
            return p;
        };
        auto *copy = session("copy");
        w->setTab(copy);
        QPushButton *advanced=nullptr;
        for(auto *button:copy->findChildren<QPushButton *>()) if(button->isCheckable()) { advanced=button; break; }
        check(advanced,"advanced controls button");
        if(!advanced->isChecked()) advanced->click();
        QApplication::processEvents();
        for(const char *name:{"copyNzbButton","copyArchiveButton","copyPasswordButton"})
            check(copy->findChild<QPushButton *>(name)->isVisible(),"copy controls visible with metadata");
        w->grab().save(base+"/copy-controls.png");
        const QString secret = QString::fromUtf8(" 001 p@ss Ω ! ");
        copy->findChild<QCheckBox *>("compressCB")->setChecked(true);
        copy->findChild<QCheckBox *>("nzbPassCB")->setChecked(true);
        copy->findChild<QLineEdit *>("nzbPassEdit")->setText(secret);
        copy->findChild<QPushButton *>("copyPasswordButton")->click();
        check(QApplication::clipboard()->text()==secret,"exact password clipboard");
        copy->findChild<QLineEdit *>("compressNameEdit")->setText(QString::fromUtf8("001 archive Ω"));
        copy->findChild<QPushButton *>("copyArchiveButton")->click();
        check(QApplication::clipboard()->text()==QString::fromUtf8("001 archive Ω"),"exact archive clipboard");
        copy->findChild<QPushButton *>("copyNzbButton")->click();
        check(QApplication::clipboard()->text()=="copy.nzb","NZB without path");
        w->findChild<QCheckBox *>("rarPassCB")->setChecked(true);
        w->findChild<QLineEdit *>("rarPassEdit")->setText("001-fixed");
        copy->findChild<QCheckBox *>("nzbPassCB")->setChecked(false);
        copy->findChild<QPushButton *>("copyPasswordButton")->click();
        check(QApplication::clipboard()->text()=="001-fixed","fixed password fallback");
        copy->findChild<QCheckBox *>("compressCB")->setChecked(false);
        QApplication::clipboard()->setText("unchanged");
        copy->findChild<QPushButton *>("copyPasswordButton")->click();
        check(QApplication::clipboard()->text()=="unchanged","no password without compression");
        QMetaObject::invokeMethod(copy,"onClearFilesClicked");
        if(advanced->isChecked()) advanced->click();

        auto idle = [&] { return !w->property("sessionBatchBusy").toBool() && !app.hasPostingJobs(); };
        qint64 maxGap = 0; QElapsedTimer tick; tick.start(); QTimer heartbeat; heartbeat.setInterval(10);
        QObject::connect(&heartbeat,&QTimer::timeout,[&] { maxGap=qMax(maxGap,tick.restart()); }); heartbeat.start();
        for (int count : {5,100}) {
            QList<PostingWidget *> list; const auto offset=server.articles.size();
            for (int i=0;i<count;++i) list << session(QString("queue-%1-%2").arg(count).arg(i,3,10,QChar('0')));
            w->finishAddingTabs(); QApplication::processEvents(); tick.restart(); maxGap=0;
            auto *start = w->findChild<QPushButton *>("startAllSessionsButton"); check(start,"start all button");
            start->click(); start->click(); until(idle,60000);
            if (server.articles.size()!=offset+count) {
                std::cerr<<"Expected "<<count<<" articles, got "<<server.articles.size()-offset<<std::endl;
                for(auto *log:w->findChildren<QTextEdit *>()) std::cerr<<log->toPlainText().toStdString()<<std::endl;
            }
            check(server.articles.size()==offset+count,"start all exactly once");
            for (int i=0;i<count;++i) {
                const auto name=QString("queue-%1-%2").arg(count).arg(i,3,10,QChar('0'));
                check(server.articles[offset+i].contains(name.toUtf8()+".txt"),"sequential posting order");
                check(read(dir+"/"+name+".nzb").contains("<segment"),"NZB contains posted segment");
            }
            list.last()->findChild<QPushButton *>("copyNzbButton")->click();
            check(QApplication::clipboard()->text()==QString("queue-%1-%2.nzb").arg(count).arg(count-1,3,10,QChar('0')),"copy after job deleted");
            std::cout<<"QUEUE count="<<count<<" max_gui_gap_ms="<<maxGap<<std::endl;
            check(maxGap<=500,"queue GUI responsiveness");
        }
        int dialogs=0; QString choice="skipConflict"; bool applyAll=true;
        QTimer answer; answer.setInterval(10);
        QObject::connect(&answer,&QTimer::timeout,[&] {
            for(auto *box:w->findChildren<QMessageBox *>("batchConflictDialog")) if(box->isVisible()) {
                ++dialogs; box->checkBox()->setChecked(applyAll); box->findChild<QPushButton *>(choice)->click();
            }
        }); answer.start();
        for (const QString &action : {QString("skipConflict"),QString("overwriteConflict"),QString("uniqueConflict"),QString("cancelConflict")}) {
            choice=action; dialogs=0; const auto offset=server.articles.size();
            QList<PostingWidget *> list;
            for(int i=0;i<3;++i) { auto name=action+QString::number(i); list<<session(name); write(dir+"/"+name+".nzb","KEEP"); }
            w->startSessions(list); until(idle);
            check(dialogs==1,"apply to all conflicts");
            const bool posted=action=="overwriteConflict" || action=="uniqueConflict";
            check(server.articles.size()==offset+(posted?3:0),"conflict submission count");
            for(int i=0;i<3;++i) {
                const auto path=dir+"/"+action+QString::number(i)+".nzb";
                check((read(path)=="KEEP")== (action!="overwriteConflict"),"existing output preservation");
                if(action=="uniqueConflict") { list[i]->findChild<QPushButton *>("copyNzbButton")->click(); check(QApplication::clipboard()->text()!=QFileInfo(path).fileName(),"copy resolved unique filename"); }
            }
        }
        choice="uniqueConflict";
        auto *a=session("duplicate-a"),*b=session("duplicate-b");
        b->findChild<QLineEdit *>("nzbFileEdit")->setText(a->findChild<QLineEdit *>("nzbFileEdit")->text());
        auto offset=server.articles.size(); w->startSessions({a,b}); until(idle); check(server.articles.size()==offset+2,"duplicate output reserved safely");
        auto *invalid=session("missing"); QFile::remove(dir+"/missing.txt"); offset=server.articles.size();
        w->startSessions({invalid}); until(idle); check(server.articles.size()==offset,"invalid source skipped");
        answer.stop();
        server.hold=true; auto *stopped=session("stop"); w->startSessions({stopped});
        until([&]{return !w->property("sessionBatchBusy").toBool();});
        QElapsedTimer stop; stop.start(); stopped->postFiles(true); until(idle,5000);
        check(stop.elapsed()<1000,"stop responds within one second"); server.hold=false;
        auto *restart=session("after-stop"); offset=server.articles.size(); w->startSessions({restart}); until(idle);
        check(server.articles.size()==offset+1,"queue works after cancellation");
        // Exercise actual external-process success, launch failure and a tool that needs forced cancellation.
        const auto rarPath=base+"/rar.exe";
        const auto originalRar=read(rarPath);
        auto compressionSession=[&](const QString &name,const QString &password) {
            auto *p=session(name); p->findChild<QCheckBox *>("compressCB")->setChecked(true);
            p->findChild<QLineEdit *>("compressPathEdit")->setText(dir);
            p->findChild<QCheckBox *>("nzbPassCB")->setChecked(true);
            p->findChild<QLineEdit *>("nzbPassEdit")->setText(password);
            return p;
        };
        auto *packedA=compressionSession("packed-a","001-first!");
        auto *packedB=compressionSession("packed-b","002-second!");
        offset=server.articles.size(); w->startSessions({packedA,packedB}); until(idle);
        check(server.articles.size()>=offset+2,"real archive jobs uploaded");
        packedA->findChild<QPushButton *>("copyPasswordButton")->click(); check(QApplication::clipboard()->text()=="001-first!","first snapshot password after compression");
        packedB->findChild<QPushButton *>("copyPasswordButton")->click(); check(QApplication::clipboard()->text()=="002-second!","second snapshot password after compression");
        write(rarPath,"invalid executable for launch failure test");
        auto *failed=compressionSession("failed-tool",""); offset=server.articles.size(); w->startSessions({failed}); until(idle,5000);
        check(server.articles.size()==offset,"failed external process completes without upload");
        write(rarPath,read(QCoreApplication::applicationFilePath()));
        auto *slow=compressionSession("cancel-tool",""); QString prepareError;
        QPointer<PostingJob> slowJob=slow->preparePosting(prepareError); check(slowJob,"prepare slow tool");
        int finished=0; QObject::connect(slowJob,&PostingJob::postingFinished,w,[&]{++finished;});
        slow->enqueuePrepared(slowJob);
        until([&]{ auto *process=slowJob?slowJob->findChild<QProcess *>():nullptr; return process && process->state()==QProcess::Running; });
        tick.restart(); maxGap=0; slow->postFiles(true); until(idle,6000);
        check(finished==1,"external cancellation finishes once"); check(maxGap<500,"external cancellation does not block GUI");
        write(rarPath,originalRar);
        heartbeat.stop(); QApplication::clipboard()->clear();
        std::cout<<"PASS: metadata, 5/100 sequential jobs, double click, conflicts, duplicate outputs, missing source, stop/restart, real RAR, failed tool and asynchronous tool cancellation."<<std::endl;
        return 0;
    } catch(const std::exception &e) { std::cerr<<"FAIL: "<<e.what()<<std::endl; return 1; }
}
