#pragma once
#include <QList>
class MainWindow;
class NgPost;
class PostingWidget;
void startSessionBatch(MainWindow *window, NgPost *app, const QList<PostingWidget *> &sessions, bool includeCompleted);
