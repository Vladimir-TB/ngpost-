#include "SessionBatch.h"
#include "MainWindow.h"
#include "PostingWidget.h"
#include "NgPost.h"
#include "PostingJob.h"
#include <QCheckBox>
#include <QDir>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QSet>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent>
#include <atomic>
#include <memory>

namespace {
QString key(const QString &path) {
    const QString normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
    return normalized.toCaseFolded();
#else
    return normalized;
#endif
}
struct Input { QFileInfoList files; QString output; bool compress; };
struct Validation { QString error; bool exists = false; };
struct Prepared { QPointer<PostingWidget> widget; PostingJob *job = nullptr; bool accepted = false; };

class SessionBatch : public QObject {
    Q_DECLARE_TR_FUNCTIONS(SessionBatch)
    MainWindow *window;
    NgPost *app;
    QList<QPointer<PostingWidget>> sessions;
    QList<Prepared> prepared;
    QList<Input> inputs;
    QList<Validation> validation;
    QSet<QString> reserved;
    QProgressDialog *progress;
    std::shared_ptr<std::atomic_bool> cancelled = std::make_shared<std::atomic_bool>(false);
    int index = 0, policy = -1, skipped = 0;
    bool includeCompleted;
    int submitIndex = 0;
public:
    SessionBatch(MainWindow *w, NgPost *a, const QList<PostingWidget *> &list, bool completed)
        : QObject(w), window(w), app(a), includeCompleted(completed) {
        for (auto *session : list) if (session->readyForBatch(completed)) sessions << session;
        window->setProperty("sessionBatchBusy", true);
        app->beginBatchPreparation();
        progress = new QProgressDialog(tr("Validating sessions..."), tr("Cancel"), 0, 0, w);
        progress->setObjectName("sessionBatchProgress");
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        connect(progress, &QProgressDialog::canceled, this, [this] { *cancelled = true; });
        progress->show();
        QTimer::singleShot(0, this, [this] { snapshot(); });
    }
    ~SessionBatch() override { for (auto &item : prepared) delete item.job; }
private:
    void finish(bool submit) {
        if (submit && !*cancelled && submitIndex < prepared.size()) {
            // The preparation guard defers queue-empty actions until every accepted job is registered.
            auto &item = prepared[submitIndex++];
            if (item.accepted && item.widget && item.widget->readyForBatch(includeCompleted)) {
                item.widget->enqueuePrepared(item.job);
                item.job = nullptr;
            }
            QTimer::singleShot(1, this, [this] { finish(true); });
            return;
        }
        progress->hide(); progress->deleteLater();
        window->setProperty("sessionBatchBusy", false);
        if (skipped) window->log(tr("%1 session(s) skipped during validation.").arg(skipped));
        app->endBatchPreparation();
        deleteLater();
    }
    void snapshot() {
        if (*cancelled) { finish(false); return; }
        if (index < sessions.size()) {
            auto widget = sessions.at(index++);
            if (widget && widget->readyForBatch(includeCompleted)) {
                QString error;
                if (auto *job = widget->preparePosting(error)) {
                    prepared << Prepared{widget, job, false};
                    inputs << Input{job->sourceFiles(), job->nzbFilePath(), job->hasCompressed()};
                } else ++skipped;
            }
            QTimer::singleShot(1, this, [this] { snapshot(); });
            return;
        }
        const auto work = inputs;
        const auto stop = cancelled;
        auto *watcher = new QFutureWatcher<QList<Validation>>(this);
        connect(watcher, &QFutureWatcher<QList<Validation>>::finished, this, [this, watcher] {
            validation = watcher->result(); watcher->deleteLater();
            for (const QString &path : app->reservedNzbPaths()) reserved.insert(key(path));
            index = 0;
            resolveNext();
        });
        watcher->setFuture(QtConcurrent::run([work, stop] {
            QList<Validation> results;
            for (const auto &input : work) {
                Validation result;
                if (*stop) break;
                for (const QFileInfo &file : input.files) {
                    if (!file.exists() || !file.isReadable() || (file.isDir() && !input.compress)) {
                        result.error = QStringLiteral("Missing/unreadable source or folder without compression"); break;
                    }
                }
                const QFileInfo output(input.output);
                if (!QFileInfo(output.absolutePath()).isDir() || !QFileInfo(output.absolutePath()).isWritable() || output.isDir())
                    result.error = QStringLiteral("Invalid NZB output directory");
                result.exists = output.exists();
                results << result;
            }
            return results;
        }));
    }
    void choose(int choice, bool duplicate) {
        if (choice == 3) { *cancelled = true; finish(false); return; }
        auto &item = prepared[index];
        if (choice == 0) { ++skipped; }
        else {
            QString path = item.job->nzbFilePath();
            // A reserved output can never overwrite another queued session, even with apply-to-all.
            if (choice == 2 || duplicate) {
                QFileInfo file(path);
                path = file.absolutePath() + "/" + file.completeBaseName() + "_" + QUuid::createUuid().toString(QUuid::Id128).left(8) + ".nzb";
            }
            item.job->setOutputTarget(path, choice == 1 && !duplicate);
            reserved.insert(key(path)); item.accepted = true;
        }
        ++index;
        QTimer::singleShot(0, this, [this] { resolveNext(); });
    }
    void resolveNext() {
        if (*cancelled) { finish(false); return; }
        if (index >= prepared.size()) { finish(true); return; }
        if (!prepared[index].widget || !validation[index].error.isEmpty()) {
            window->logError(tr("Session skipped: %1").arg(validation[index].error));
            ++skipped; ++index; QTimer::singleShot(0, this, [this] { resolveNext(); }); return;
        }
        const QString path = prepared[index].job->nzbFilePath();
        const bool duplicate = reserved.contains(key(path));
        if (!duplicate && !validation[index].exists) {
            reserved.insert(key(path)); prepared[index].accepted = true; ++index;
            QTimer::singleShot(0, this, [this] { resolveNext(); }); return;
        }
        if (policy >= 0) { choose(policy, duplicate); return; }
        auto *box = new QMessageBox(QMessageBox::Question, tr("NZB filename conflict"),
            tr("The NZB filename already exists or is reserved by another session:\n%1").arg(path), QMessageBox::NoButton, window);
        box->setObjectName("batchConflictDialog");
        auto *skip = box->addButton(tr("Skip"), QMessageBox::RejectRole); skip->setObjectName("skipConflict");
        auto *overwrite = box->addButton(tr("Overwrite"), QMessageBox::DestructiveRole); overwrite->setObjectName("overwriteConflict");
        overwrite->setEnabled(!duplicate);
        auto *unique = box->addButton(tr("Unique name"), QMessageBox::AcceptRole); unique->setObjectName("uniqueConflict");
        auto *cancel = box->addButton(tr("Cancel batch"), QMessageBox::RejectRole); cancel->setObjectName("cancelConflict");
        auto *all = new QCheckBox(tr("Apply to all conflicts"), box); box->setCheckBox(all);
        box->setDefaultButton(unique);
        box->setEscapeButton(cancel);
        connect(box, &QMessageBox::finished, this, [this, box, skip, overwrite, unique, all, duplicate] {
            const int choice = box->clickedButton() == skip ? 0 : box->clickedButton() == overwrite ? 1 : box->clickedButton() == unique ? 2 : 3;
            if (all->isChecked() && choice != 3) policy = choice;
            box->deleteLater(); choose(choice, duplicate);
        });
        box->open();
    }
};
}
void startSessionBatch(MainWindow *window, NgPost *app, const QList<PostingWidget *> &sessions, bool completed)
{
    new SessionBatch(window, app, sessions, completed);
}
