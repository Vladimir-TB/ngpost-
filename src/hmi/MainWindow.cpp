//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3..
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>
//
//========================================================================

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "PostingWidget.h"
#include "AutoPostWidget.h"
#include "NgPost.h"
#include "nntp/NntpServerParams.h"
#include "nntp/NntpArticle.h"

#include <QDebug>
#include <QProgressBar>
#include <QLabel>
#include <QKeySequence>
#include <QScreen>
#include <QMessageBox>
#include <QStyleFactory>
#include <QColorDialog>
#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitterHandle>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace {
bool isColorDark(const QColor &color)
{
    const int luminance = (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
    return luminance < 128;
}

QColor blendColor(const QColor &base, const QColor &accent, double accentRatio)
{
    const double baseRatio = 1.0 - accentRatio;
    return QColor(
        static_cast<int>(base.red() * baseRatio + accent.red() * accentRatio + 0.5),
        static_cast<int>(base.green() * baseRatio + accent.green() * accentRatio + 0.5),
        static_cast<int>(base.blue() * baseRatio + accent.blue() * accentRatio + 0.5));
}

QString scaledStyleText(const QString &style, double scale)
{
    if (qFuzzyCompare(scale, 1.0))
        return style;

    QString scaled = style;
    QRegularExpression pxRe("(\\d+(?:\\.\\d+)?)px");
    QRegularExpressionMatchIterator pxIt = pxRe.globalMatch(style);
    while (pxIt.hasNext())
    {
        const QRegularExpressionMatch match = pxIt.next();
        const double value = match.captured(1).toDouble();
        const QString replacement = QString("%1px").arg(qMax(0, qRound(value * scale)));
        scaled.replace(match.captured(0), replacement);
    }

    QRegularExpression ptRe("(\\d+(?:\\.\\d+)?)pt");
    QRegularExpressionMatchIterator ptIt = ptRe.globalMatch(scaled);
    while (ptIt.hasNext())
    {
        const QRegularExpressionMatch match = ptIt.next();
        const double value = match.captured(1).toDouble();
        const QString replacement = QString("%1pt").arg(QString::number(value * scale, 'f', 1));
        scaled.replace(match.captured(0), replacement);
    }
    return scaled;
}

void detachLayoutItems(QLayout *layout)
{
    if (!layout)
        return;

    while (QLayoutItem *item = layout->takeAt(0))
    {
        if (QLayout *childLayout = item->layout())
        {
            detachLayoutItems(childLayout);
        }
        else if (QWidget *widget = item->widget())
        {
            widget->hide();
        }
        delete item;
    }
}
}


const QColor  MainWindow::sPostingColor = QColor(255,162, 0); // gold (#FFA200)
const QString MainWindow::sPostingIcon  = ":/icons/uploading.png";
const QColor  MainWindow::sPendingColor = Qt::darkBlue;
const QString MainWindow::sPendingIcon  = ":/icons/pending.png";
const QColor  MainWindow::sDoneOKColor  = Qt::darkGreen;
const QString MainWindow::sDoneOKIcon   = ":/icons/ok.png";
const QColor  MainWindow::sDoneKOColor  = Qt::darkRed;
const QString MainWindow::sDoneKOIcon   = ":/icons/ko.png";
const QColor  MainWindow::sArticlesFailedColor  = Qt::darkYellow;


const QList<const char *> MainWindow::sServerListHeaders = {
    QT_TRANSLATE_NOOP("MainWindow", "on"),
    QT_TRANSLATE_NOOP("MainWindow", "Host (name or IP)"),
    QT_TRANSLATE_NOOP("MainWindow", "Port"),
    QT_TRANSLATE_NOOP("MainWindow", "SSL"),
    QT_TRANSLATE_NOOP("MainWindow", "Connections"),
    QT_TRANSLATE_NOOP("MainWindow", "Username"),
    QT_TRANSLATE_NOOP("MainWindow", "Password"),
    "" // for the delete button
};
const QVector<int> MainWindow::sServerListSizes   = {30, 200, 50, 30, 100, 150, 150, sDeleteColumnWidth};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    _ui(new Ui::MainWindow),
    _ngPost(nullptr),
    _state(STATE::IDLE),
    _quickJobTab(nullptr),
    _autoPostTab(nullptr),
    _shellView(ShellView::Overview),
    _theme(Theme::Light),
    _defaultPalette(),
    _defaultHighlightColor(),
    _accentColor(),
    _defaultStyleName(),
    _defaultAppStyleSheet(),
    _defaultAppFont(),
    _baseWindowSize(),
    _uiScale(1.0),
    _serversDialog(nullptr),
    _preferencesDialog(nullptr),
    _helpDialog(nullptr),
    _helpBrowser(nullptr),
    _settingsMenu(nullptr),
    _viewMenu(nullptr),
    _scaleMenu(nullptr),
    _openServersAction(nullptr),
    _openPreferencesAction(nullptr),
    _saveConfigAction(nullptr),
    _toggleFullscreenAction(nullptr),
    _aboutMenuAction(nullptr),
    _helpMenuAction(nullptr),
    _scaleActionGroup(nullptr),
    _shellRoot(nullptr),
    _contentStack(nullptr),
    _overviewPage(nullptr),
    _workspacePage(nullptr),
    _activityDock(nullptr),
    _overviewStatsFrame(nullptr),
    _workspaceHeaderFrame(nullptr),
    _sidebarTaglineLabel(nullptr),
    _sidebarVersionLabel(nullptr),
    _sidebarFooterLabel(nullptr),
    _overviewNavButton(nullptr),
    _quickNavButton(nullptr),
    _autoNavButton(nullptr),
    _activityNavButton(nullptr),
    _sideServersButton(nullptr),
    _sidePreferencesButton(nullptr),
    _sideSaveButton(nullptr),
    _sideFullscreenButton(nullptr),
    _sideHelpButton(nullptr),
    _overviewEyebrowLabel(nullptr),
    _overviewTitleLabel(nullptr),
    _overviewSubtitleLabel(nullptr),
    _quickActionButton(nullptr),
    _autoActionButton(nullptr),
    _serversActionButton(nullptr),
    _preferencesActionButton(nullptr),
    _sessionCardTitleLabel(nullptr),
    _sessionCardValueLabel(nullptr),
    _serversCardTitleLabel(nullptr),
    _serversCardValueLabel(nullptr),
    _themeCardTitleLabel(nullptr),
    _themeCardValueLabel(nullptr),
    _archiverCardTitleLabel(nullptr),
    _archiverCardValueLabel(nullptr),
    _overviewGuideTitleLabel(nullptr),
    _overviewGuideLabel(nullptr),
    _workspaceTitleLabel(nullptr),
    _workspaceSubtitleLabel(nullptr)
{
    setAcceptDrops(true);

    _ui->setupUi(this);
    _defaultPalette = qApp->palette();
    _defaultHighlightColor = _defaultPalette.color(QPalette::Highlight);
    _defaultStyleName = qApp->style()->objectName();
    _defaultAppStyleSheet = qApp->styleSheet();
    _defaultAppFont = qApp->font();

    _ui->serverBox->setStyleSheet(sGroupBoxStyle);
    _ui->fileBox->setStyleSheet(sGroupBoxStyle);
    _ui->postingBox->setStyleSheet(sGroupBoxStyle);
    _ui->logBox->setStyleSheet(sGroupBoxStyle);

    _ui->hSplitter->setStretchFactor(0, 1);
    _ui->hSplitter->setStretchFactor(1, 1);

    _ui->vSplitter->setStretchFactor(0, 1);
    _ui->vSplitter->setStretchFactor(1, 3);
    _ui->vSplitter->setCollapsible(1, false);

    _ui->postSplitter->setOrientation(Qt::Vertical);
    _ui->postSplitter->setStretchFactor(0, 7);
    _ui->postSplitter->setStretchFactor(1, 2);
    _ui->postSplitter->setCollapsible(0, false);
    _ui->postSplitter->setCollapsible(1, false);
    _ui->postSplitter->setOpaqueResize(false);

    _ui->uploadFrame->setMaximumHeight(42);
    if (QHBoxLayout *uploadLayout = qobject_cast<QHBoxLayout*>(_ui->uploadFrame->layout()))
    {
        uploadLayout->setContentsMargins(8, 5, 8, 5);
        uploadLayout->setSpacing(8);
    }
    _ui->jobLabel->setAlignment(Qt::AlignCenter);
    _ui->jobLabel->setMinimumWidth(72);
    _ui->jobLabel->setProperty("shellRole", "statusBadge");
    _ui->pauseButton->setCursor(Qt::PointingHandCursor);
    _ui->pauseButton->setProperty("shellRole", "statusControl");
    _ui->pauseButton->setFixedSize(28, 28);
    _ui->pauseButton->setIconSize(QSize(14, 14));
    _ui->progressBar->setTextVisible(true);
    _ui->progressBar->setFormat("%p%");
    _ui->progressBar->setMinimumHeight(16);
    _ui->progressBar->setMaximumHeight(16);
    _ui->logBrowser->setMinimumHeight(56);
    _ui->logBrowser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    _ui->logBox->setMinimumHeight(148);
    _ui->uploadLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    _ui->uploadLbl->setMinimumWidth(220);
    _ui->uploadLbl->setProperty("shellRole", "statusMeta");

    _ui->progressBar->setRange(0, 100);
    updateProgressBar(0, 0, "");

    const QSize screenSize = qApp->screens()[0]->availableGeometry().size();
    const QSize initialSize(qMax(980, qMin(1360, screenSize.width() - 80)),
                            qMax(620, qMin(860, screenSize.height() - 80)));
    _baseWindowSize = initialSize;
    resize(initialSize);
    setMinimumSize(initialSize);
    setWindowIcon(QIcon(":/icons/ngPost.png"));
    setGeometry((screenSize.width() - width())/2,  (screenSize.height() - height())/2, width(), height());
    _ui->saveFromCB->setToolTip(tr("use this poster email everytime you launch ngPost").replace("ngPost", NgPost::displayName()));

    connect(_ui->clearLogButton, &QAbstractButton::clicked, _ui->logBrowser, &QTextEdit::clear);
    connect(_ui->debugBox,       &QAbstractButton::toggled, this,            &MainWindow::onDebugToggled);
    connect(_ui->pauseButton,    &QAbstractButton::clicked, this,            &MainWindow::onPauseClicked);

#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
    connect(_ui->debugSB, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &MainWindow::onDebugValue);
#else
    connect(_ui->debugSB,   qOverload<int>(&QSpinBox::valueChanged),   this,    &MainWindow::onDebugValue);
#endif

    _initSettingsDialogs();
    _initHelpDialog();
    _initSettingsMenu();
    _buildModernShell();
}

MainWindow::~MainWindow()
{
    delete _ui;
}

void MainWindow::init(NgPost *ngPost)
{
    _ngPost = ngPost;

    _quickJobTab = new PostingWidget(ngPost, this, 1);
    _autoPostTab = new AutoPostWidget(ngPost, this);

    _ui->debugBox->setChecked(_ngPost->debugMode());
    _ui->debugSB->setEnabled(_ngPost->debugMode());
    _ui->debugSB->setMinimumWidth(60);
    _ui->debugSB->setAlignment(Qt::AlignCenter);
    _ui->debugSB->setButtonSymbols(QAbstractSpinBox::UpDownArrows);

    QTabBar *tabBar = _ui->postTabWidget->tabBar();
    tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    tabBar->setElideMode(Qt::TextElideMode::ElideNone);
    tabBar->setIconSize({18, 18});

    _ui->postTabWidget->clear();
    _ui->postTabWidget->setDocumentMode(true);
    _ui->postTabWidget->setUsesScrollButtons(true);
    _ui->postTabWidget->setStyleSheet(sTabWidgetStyle);
    _ui->postTabWidget->addTab(_createScrollableTabPage(_quickJobTab), QIcon(":/icons/quick.png"), _ngPost->quickJobName());
    tabBar->setTabToolTip(0, tr("Default %1").arg(_ngPost->quickJobName()));
    _ui->postTabWidget->addTab(_createScrollableTabPage(_autoPostTab), QIcon(":/icons/auto.png"), _ngPost->folderMonitoringName());
    tabBar->setTabToolTip(1, _ngPost->folderMonitoringName());
    _ui->postTabWidget->addTab(new QWidget(_ui->postTabWidget), QIcon(":/icons/plus.png"), tr("New"));
    tabBar->setTabToolTip(2, tr("Create a new %1").arg(_ngPost->quickJobName()));

//    connect(_ui->postTabWidget,           &QTabWidget::currentChanged, this, &MainWindow::onJobTabClicked);
    connect(tabBar, &QTabBar::tabBarClicked,              this, &MainWindow::onJobTabClicked);
    connect(tabBar, &QWidget::customContextMenuRequested, this, &MainWindow::onTabContextMenu);
    connect(tabBar, &QTabBar::tabCloseRequested,          this, &MainWindow::onCloseJob);
    connect(_ui->postTabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        if (_shellView != ShellView::Overview)
        {
            if (index == 1)
                _shellView = ShellView::AutoPosting;
            else if (index < _ui->postTabWidget->count() - 1)
                _shellView = ShellView::QuickPost;

            if (_overviewNavButton)
                _overviewNavButton->setChecked(false);
            if (_quickNavButton)
                _quickNavButton->setChecked(_shellView == ShellView::QuickPost);
            if (_autoNavButton)
                _autoNavButton->setChecked(_shellView == ShellView::AutoPosting);
            if (_activityNavButton)
                _activityNavButton->setChecked(_shellView == ShellView::Activity);
        }
        _updateWorkspaceHeader();
        _refreshShellSummary();
    });
    _ui->postTabWidget->setTabsClosable(true);
    _ui->postTabWidget->installEventFilter(this);
//    _ui->postTabWidget->setCurrentIndex(1);

    setJobLabel(1);


    for (const QString &lang : _ngPost->languages())
        _ui->langCB->addItem(QIcon(QString(":/icons/flag_%1.png").arg(lang.toUpper())), lang.toUpper(), lang);
    _ui->langCB->setCurrentText(_ngPost->_lang.toUpper());
    connect(_ui->langCB, &QComboBox::currentTextChanged, this, &MainWindow::onLangChanged);

    _initServerBox();
    _initPostingBox();
    _quickJobTab->init();
    _autoPostTab->init();

    _ui->goCmdButton->hide();
//    connect(_ui->goCmdButton, &QAbstractButton::clicked, _ngPost, &NgPost::onGoCMD, Qt::QueuedConnection);

    _theme = _ngPost->_themeMode.compare("light", Qt::CaseInsensitive) == 0 ? Theme::Light : Theme::Dark;
    _uiScale = qBound(1.0, _ngPost->_uiScale > 0.0 ? _ngPost->_uiScale : 1.0, 1.5);

    updateProgressBar(0, 0);
    _applyUiScale(_uiScale, false);
    _setShellView(ShellView::Overview);
}


void MainWindow::updateProgressBar(uint nbArticlesTotal, uint nbArticlesUploaded, const QString &avgSpeed
                                   #ifdef __COMPUTE_IMMEDIATE_SPEED__
                                       , const QString &immediateSpeed
                                   #endif
                                   )
{
//    qDebug() << "[MainWindow::updateProgressBar] _nbArticlesUploaded: " << nbArticlesUploaded;
    _ui->progressBar->setValue(static_cast<int>(nbArticlesUploaded));

#ifdef __COMPUTE_IMMEDIATE_SPEED__
    _ui->uploadLbl->setText(QString("%5 (%1 / %2) %3: %4").arg(
                                nbArticlesUploaded).arg(
                                nbArticlesTotal).arg(
                                tr("avg speed")).arg(
                                avgSpeed).arg(
                                immediateSpeed));
#else
    _ui->uploadLbl->setText(QString("(%1 / %2) %3: %4").arg(
                                nbArticlesUploaded).arg(
                                nbArticlesTotal).arg(
                                tr("avg speed")).arg(
                                avgSpeed));
#endif
    _applyActivityPaneLayout();
    _refreshShellSummary();
}


void MainWindow::log(const QString &aMsg, bool newline) const
{
    if (newline)
        _ui->logBrowser->append(aMsg);
    else
    {
        _ui->logBrowser->insertPlainText(aMsg);
        _ui->logBrowser->moveCursor(QTextCursor::End);
    }
    const_cast<MainWindow*>(this)->_applyActivityPaneLayout();
}

void MainWindow::logError(const QString &error) const
{
    _ui->logBrowser->append(QString("<font color='red'>%1</font><br/>\n").arg(error));
    const_cast<MainWindow*>(this)->_applyActivityPaneLayout();
}

bool MainWindow::useFixedPassword() const
{
    return _ui->rarPassCB->isChecked();
}

bool MainWindow::hasAutoCompress() const
{
    return _ui->autoCompressCB->isChecked();
}

#include <QKeyEvent>
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && obj == _ui->postTabWidget)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        qDebug() << "[MainWindow] getting key event: " << keyEvent->key();
        int currentTabIdx = _ui->postTabWidget->currentIndex();
        if (currentTabIdx == 1)
            _autoPostTab->handleKeyEvent(keyEvent);
        else if (currentTabIdx == 0)
            _quickJobTab->handleKeyEvent(keyEvent);
        else if (currentTabIdx < _ui->postTabWidget->count() - 1)
        {
            if (PostingWidget *postWidget = _getPostWidget(currentTabIdx))
                postWidget->handleKeyEvent(keyEvent);
        }
    }
    return QObject::eventFilter(obj, event);
}

#include <QMimeData>
void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    int currentTabIdx = _ui->postTabWidget->currentIndex();
    if (_shellView == ShellView::Overview)
    {
        _setShellView(currentTabIdx == 1 ? ShellView::AutoPosting : ShellView::QuickPost);
        currentTabIdx = _ui->postTabWidget->currentIndex();
    }
    if (currentTabIdx == 1)
        _autoPostTab->handleDropEvent(e);
    else if (currentTabIdx == 0)
        _quickJobTab->handleDropEvent(e);
    else if (currentTabIdx < _ui->postTabWidget->count() - 1)
    {
        if (PostingWidget *postWidget = _getPostWidget(currentTabIdx))
            postWidget->handleDropEvent(e);
    }
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    if (_ngPost->hasPostingJobs())
    {
        int res = QMessageBox::question(this,
                                        tr("close while still posting?"),
                                        tr("ngPost is currently posting.\nAre you sure you want to quit?").replace("ngPost", NgPost::displayName()),
                                        QMessageBox::Yes,
                                        QMessageBox::No);
        if (res == QMessageBox::Yes)
        {
            _ngPost->closeAllPostingJobs();
            event->accept();
        }
        else
            event->ignore();

    }
    else
        event->accept();
}

void MainWindow::changeEvent(QEvent *event)
{
    if(event)
    {
        QStringList serverTableHeader;
        switch(event->type()) {
        // this event is send if a translator is loaded
        case QEvent::LanguageChange:
            qDebug() << "MainWindow::changeEvent";
            _ui->retranslateUi(this);
            _ui->themeButton->setText(_theme == Theme::Dark ? tr("Dark mode") : tr("Light mode"));
            _ui->themeButton->setEnabled(true);
            _ui->themeButton->show();
            _ui->genPoster->setText(tr("Gen."));
            _ui->genPass->setText(tr("Gen."));
            _ui->nzbPathButton->setText(tr("Browse"));
            _ui->saveFromCB->setToolTip(tr("use this poster email everytime you launch ngPost").replace("ngPost", NgPost::displayName()));
#ifdef __COMPUTE_IMMEDIATE_SPEED__
            _ui->uploadLbl->setToolTip(tr("Immediate speed (avg on %1 sec) - (nb Articles uploaded / total number of Articles) - avg speed").arg(NgPost::immediateSpeedDuration()));
#endif
            _ui->shutdownCB->setToolTip(tr("Shutdown computer when all the current Posts are done (with command: %1)").arg(
                                            _ngPost->_shutdownCmd));

            _ui->serverBox->setTitle(tr("Servers"));
            _ui->fileBox->setTitle(tr("Files"));
            _ui->postingBox->setTitle(tr("Parameters"));
            _ui->logBox->setTitle(QString());
            if (_activityDock)
                _activityDock->setWindowTitle(tr("Activity"));
            if (_serversDialog)
                _serversDialog->setWindowTitle(tr("Server Settings"));
            if (_preferencesDialog)
                _preferencesDialog->setWindowTitle(tr("Preferences"));
            if (_settingsMenu)
                _settingsMenu->setTitle(tr("Settings"));
            if (_viewMenu)
                _viewMenu->setTitle(tr("View"));
            if (_scaleMenu)
                _scaleMenu->setTitle(tr("Scale"));
            if (_openServersAction)
                _openServersAction->setText(tr("Servers..."));
            if (_openPreferencesAction)
                _openPreferencesAction->setText(tr("Preferences..."));
            if (_saveConfigAction)
                _saveConfigAction->setText(tr("Save Config"));
            if (_aboutMenuAction)
                _aboutMenuAction->setText(tr("About"));
            if (_helpMenuAction)
                _helpMenuAction->setText(tr("Help"));
            if (_helpDialog)
                _helpDialog->setWindowTitle(tr("Help - Handleiding"));
            if (_helpBrowser)
                _helpBrowser->setHtml(_helpHtml());
            _updateSettingsContentHints();
            _fitDialogToContent(_serversDialog);
            _fitDialogToContent(_preferencesDialog);
            applyTheme(_theme);
            _refreshShellText();
            _refreshWindowModeUi();
            _refreshScaleMenu();
            _refreshWorkspaceTabBar();

            setJobLabel(_ui->postTabWidget->currentIndex());

            for (const char *header : sServerListHeaders)
                serverTableHeader << tr(header);
            _ui->serversTable->setHorizontalHeaderLabels(serverTableHeader);


            _quickJobTab->retranslate();
            _autoPostTab->retranslate();
            for (int i = 2 ; i < _ui->postTabWidget->count() - 1; ++i)
                _getPostWidget(i)->retranslate();
            break;

        case QEvent::WindowStateChange:
            _refreshWindowModeUi();
            break;

            // this event is send, if the system, language changes
        default:
            break;
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    _applyActivityPaneLayout();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    _applyActivityPaneLayout();
    QTimer::singleShot(0, this, [this]() { _applyActivityPaneLayout(); });
}



#include "CheckBoxCenterWidget.h"
void MainWindow::onAddServer()
{
    _addServer(nullptr);
    _refreshShellSummary();
}

void MainWindow::onDelServer()
{
    QObject *delButton = sender();
    int row = _serverRow(delButton);
    if (row < _ui->serversTable->rowCount())
        _ui->serversTable->removeRow(row);

    NntpServerParams *serverParam = static_cast<NntpServerParams*>(delButton->property("server").value<void*>());
    if (serverParam)
    {
        _ngPost->removeNntpServer(serverParam);
        delete serverParam;
    }
    _refreshShellSummary();
}

void MainWindow::onObfucateToggled(bool checked)
{
    bool enabled = !checked;
    _ui->fromEdit->setEnabled(enabled);
    _ui->genPoster->setEnabled(enabled);
    _ui->saveFromCB->setEnabled(enabled);
    _ui->uniqueFromCB->setEnabled(enabled);
}

void MainWindow::onTabContextMenu(const QPoint &point)
{
//    qDebug() << "MainWindow::onTabContextMenu: " << point;
    if (point.isNull())
        return;

//    QTabBar *tabBar = _ui->postTabWidget->tabBar();
//    int tabIndex = tabBar->tabAt(point);
//    PostingWidget *currentPostWidget = _getPostWidget(tabIndex);
    QMenu menu(tr("Quick Tabs Menu"), this);
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QAction *action = menu.addAction(QIcon(":/icons/clear.png"), tr("Close All finished Tabs"), this, &MainWindow::onCloseAllFinishedQuickTabs);
#else
    QAction *action = menu.addAction(QIcon(":/icons/clear.png"), tr("Close All finished Tabs"), this, SLOT(onCloseAllFinishedQuickTabs));
#endif
    action->setEnabled(hasFinishedPosts());
    menu.exec(QCursor::pos());
}

bool MainWindow::hasFinishedPosts() const
{
    for (int idx = 2 ; idx < _ui->postTabWidget->count() - 2 ; ++idx)
    {
        PostingWidget *postWidget = _getPostWidget(idx);
        if (postWidget && postWidget->isPostingFinished())
            return true;
    }
    return false;
}

void MainWindow::onCloseAllFinishedQuickTabs()
{
    // go backwards as we may delete the current tab ;)
    for (int idx = _ui->postTabWidget->count() - 2 ; idx > 1  ; --idx)
    {
        PostingWidget *postWidget = _getPostWidget(idx);
        if (postWidget && postWidget->isPostingFinished())
            onCloseJob(idx);
    }
}

void MainWindow::onSetProgressBarRange(int nbArticles)
{
    qDebug() << "MainWindow::onSetProgressBarRange: " << nbArticles;
    _ui->progressBar->setRange(0, nbArticles);
    _refreshShellSummary();
}


void MainWindow::_initServerBox()
{
    _ui->serversTable->verticalHeader()->hide();
    _ui->serversTable->setColumnCount(sServerListHeaders.size());

    int width = 2, col = 0;
    for (int size : sServerListSizes)
    {
        _ui->serversTable->setColumnWidth(col++, size);
        width += size;
    }
//    _ui->serversTable->setMaximumWidth(width);

    connect(_ui->addServerButton,   &QAbstractButton::clicked, this, &MainWindow::onAddServer);

    for (NntpServerParams *srv : _ngPost->_nntpServers)
        _addServer(srv);

    _updateSettingsContentHints();
}


void MainWindow::_initPostingBox()
{
    connect(_ui->shutdownCB,        &QAbstractButton::toggled, this, &MainWindow::onShutdownToggled);
    connect(_ui->saveButton,        &QAbstractButton::clicked, this, &MainWindow::onSaveConfig);

    connect(_ui->genPoster,         &QAbstractButton::clicked, this, &MainWindow::onGenPoster);
    connect(_ui->obfuscateMsgIdCB,  &QAbstractButton::toggled, this, &MainWindow::onObfucateToggled);
    connect(_ui->uniqueFromCB,      &QAbstractButton::toggled, this, &MainWindow::onUniqueFromToggled);
    connect(_ui->rarPassCB,         &QAbstractButton::toggled, this, &MainWindow::onRarPassToggled);
    connect(_ui->genPass,           &QAbstractButton::clicked, this, &MainWindow::onArchivePass);
    connect(_ui->autoCompressCB,    &QAbstractButton::toggled, this, &MainWindow::onAutoCompressToggled);
    connect(_ui->rarPassEdit,       &QLineEdit::textChanged,   this, &MainWindow::onRarPassUpdated);
    connect(_ui->themeButton,       &QAbstractButton::toggled, this, &MainWindow::onThemeToggled);

    _ui->themeColorLbl->hide();
    _ui->themeColorButton->hide();
    _ui->themeColorResetButton->hide();
    _ui->themeButton->setEnabled(true);
    _ui->themeButton->show();
    _ui->genPoster->setText(tr("Gen."));
    _ui->genPoster->setMaximumWidth(QWIDGETSIZE_MAX);
    _ui->genPoster->setMinimumWidth(54);
    _ui->genPass->setText(tr("Gen."));
    _ui->genPass->setMaximumWidth(QWIDGETSIZE_MAX);
    _ui->genPass->setMinimumWidth(54);

    _ui->fromEdit->setText(_ngPost->xml2txt(_ngPost->_from.c_str()));
    _ui->groupsEdit->setText(_ngPost->groups());
    _ui->uniqueFromCB->setChecked(_ngPost->_genFrom);
    _ui->saveFromCB->setChecked(_ngPost->_saveFrom);

    _ui->rarLengthSB->setValue(static_cast<int>(_ngPost->_lengthPass));
    if (_ngPost->_rarPassFixed.isEmpty())
    {
        _ui->rarPassCB->setChecked(false);
        onRarPassToggled(false);
    }
    else
    {
	// Issue #48 we should set the text first!
        _ui->rarPassEdit->setText(_ngPost->_rarPassFixed);
        _ui->rarPassCB->setChecked(true);
    }

    if (_ngPost->_packAuto && _ngPost->_packAutoKeywords.isEmpty())
        _ngPost->_packAutoKeywords = NgPost::defaultPackAutoKeywords();
    _ui->autoCompressCB->setChecked(_ngPost->_packAuto);
    _ui->autoCloseCB->setChecked(_ngPost->_autoCloseTabs);

    _ui->obfuscateMsgIdCB->setChecked(_ngPost->_obfuscateArticles);
    _ui->obfuscateFileNameCB->setChecked(_ngPost->_obfuscateFileName);

    _ui->articleSizeEdit->setText(QString::number(_ngPost->articleSize()));
    _ui->articleSizeEdit->setValidator(new QIntValidator(100000, 10000000, _ui->articleSizeEdit));

    _ui->nbRetrySB->setRange(0, 15);
    _ui->nbRetrySB->setValue(NntpArticle::nbMaxTrySending());
    _ui->nbRetrySB->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _ui->nbRetrySB->setAlignment(Qt::AlignCenter);

    _ui->threadSB->setRange(0, 50);
    _ui->threadSB->setValue(_ngPost->_nbThreads);
    _ui->threadSB->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _ui->threadSB->setAlignment(Qt::AlignCenter);
    _ui->rarLengthSB->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _ui->rarLengthSB->setAlignment(Qt::AlignCenter);

    _ui->nzbPathEdit->setText(_ngPost->_nzbPath);
    _ui->nzbPathEdit->setMinimumWidth(160);
    _ui->nzbPathButton->setText(tr("Browse"));
    _ui->nzbPathButton->setIcon(QIcon(":/icons/folder.png"));
    _ui->nzbPathButton->setMaximumWidth(QWIDGETSIZE_MAX);
    _ui->nzbPathButton->setMinimumWidth(96);
    connect(_ui->nzbPathButton, &QAbstractButton::clicked, this, &MainWindow::onNzbPathClicked);

    _rebuildPreferencesLayout();
    _updateSettingsContentHints();
}

void MainWindow::_rebuildPreferencesLayout()
{
    QVBoxLayout *rootLayout = qobject_cast<QVBoxLayout *>(_ui->postingBox->layout());
    if (!rootLayout)
        return;

    detachLayoutItems(rootLayout);
    rootLayout->setContentsMargins(18, 22, 18, 18);
    rootLayout->setSpacing(14);

    auto makeRowWidget = [this]() {
        QWidget *rowWidget = new QWidget(_ui->postingBox);
        rowWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        return rowWidget;
    };

    _ui->fromEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _ui->rarPassEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _ui->groupsEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _ui->groupsEdit->setMinimumHeight(58);
    _ui->groupsEdit->setMaximumHeight(78);
    _ui->nzbPathEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _ui->nzbPathButton->setMinimumWidth(116);
    _ui->themeButton->setMinimumWidth(136);
    _ui->saveButton->setMinimumWidth(136);
    _ui->langCB->setMinimumWidth(110);
    _ui->articleSizeEdit->setMaximumWidth(96);
    _ui->nbRetrySB->setMinimumWidth(76);
    _ui->threadSB->setMinimumWidth(76);
    _ui->rarLengthSB->setMinimumWidth(76);

    QWidget *identityWidget = makeRowWidget();
    QGridLayout *identityLayout = new QGridLayout(identityWidget);
    identityLayout->setContentsMargins(0, 0, 0, 0);
    identityLayout->setHorizontalSpacing(10);
    identityLayout->setVerticalSpacing(8);
    identityLayout->setColumnStretch(1, 1);

    QWidget *passToolsWidget = new QWidget(identityWidget);
    QHBoxLayout *passToolsLayout = new QHBoxLayout(passToolsWidget);
    passToolsLayout->setContentsMargins(0, 0, 0, 0);
    passToolsLayout->setSpacing(8);
    passToolsLayout->addWidget(_ui->rarLengthSB);
    passToolsLayout->addWidget(_ui->genPass);

    identityLayout->addWidget(_ui->fromLbl, 0, 0);
    identityLayout->addWidget(_ui->fromEdit, 0, 1);
    identityLayout->addWidget(_ui->genPoster, 0, 2);
    identityLayout->addWidget(_ui->saveFromCB, 1, 1, 1, 2);
    identityLayout->addWidget(_ui->uniqueFromCB, 2, 1, 1, 2);
    identityLayout->addWidget(_ui->rarPassCB, 3, 0);
    identityLayout->addWidget(_ui->rarPassEdit, 3, 1);
    identityLayout->addWidget(passToolsWidget, 3, 2);
    rootLayout->addWidget(identityWidget);

    QWidget *groupsWidget = makeRowWidget();
    QGridLayout *groupsLayout = new QGridLayout(groupsWidget);
    groupsLayout->setContentsMargins(0, 0, 0, 0);
    groupsLayout->setHorizontalSpacing(10);
    groupsLayout->setVerticalSpacing(8);
    groupsLayout->setColumnStretch(1, 1);
    groupsLayout->addWidget(_ui->groupsLbl, 0, 0, Qt::AlignTop);
    groupsLayout->addWidget(_ui->groupsEdit, 0, 1);
    rootLayout->addWidget(groupsWidget);

    rootLayout->addWidget(_ui->frame_3);

    QWidget *engineWidget = makeRowWidget();
    QGridLayout *engineLayout = new QGridLayout(engineWidget);
    engineLayout->setContentsMargins(0, 0, 0, 0);
    engineLayout->setHorizontalSpacing(12);
    engineLayout->setVerticalSpacing(10);
    engineLayout->setColumnStretch(1, 1);
    engineLayout->setColumnStretch(3, 1);
    engineLayout->addWidget(_ui->articleSizeLbl, 0, 0);
    engineLayout->addWidget(_ui->articleSizeEdit, 0, 1);
    engineLayout->addWidget(_ui->nbRetryLbl, 0, 2);
    engineLayout->addWidget(_ui->nbRetrySB, 0, 3);
    engineLayout->addWidget(_ui->threadLbl, 1, 0);
    engineLayout->addWidget(_ui->threadSB, 1, 1);
    rootLayout->addWidget(engineWidget);

    rootLayout->addWidget(_ui->frame_2);

    QWidget *optionsWidget = makeRowWidget();
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsWidget);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->setSpacing(8);
    optionsLayout->addWidget(_ui->obfuscateMsgIdCB);
    optionsLayout->addWidget(_ui->obfuscateFileNameCB);
    optionsLayout->addWidget(_ui->autoCompressCB);
    optionsLayout->addWidget(_ui->autoCloseCB);
    rootLayout->addWidget(optionsWidget);

    rootLayout->addWidget(_ui->frame_4);

    QWidget *outputWidget = makeRowWidget();
    QGridLayout *outputLayout = new QGridLayout(outputWidget);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setHorizontalSpacing(10);
    outputLayout->setVerticalSpacing(10);
    outputLayout->setColumnStretch(1, 1);

    outputLayout->addWidget(_ui->nzbPathLbl, 0, 0);
    outputLayout->addWidget(_ui->nzbPathEdit, 0, 1);
    outputLayout->addWidget(_ui->nzbPathButton, 0, 2);
    outputLayout->addWidget(_ui->shutdownCB, 1, 0, 1, 3);
    outputLayout->addWidget(_ui->langLbl, 2, 0);
    outputLayout->addWidget(_ui->langCB, 2, 1);

    QWidget *actionsWidget = new QWidget(outputWidget);
    QHBoxLayout *actionsLayout = new QHBoxLayout(actionsWidget);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(10);
    actionsLayout->addWidget(_ui->themeButton);
    actionsLayout->addWidget(_ui->saveButton);
    outputLayout->addWidget(actionsWidget, 2, 2);

    rootLayout->addWidget(outputWidget);
    rootLayout->addStretch(1);

    const QList<QWidget *> visibleWidgets = {
        _ui->fromLbl,
        _ui->fromEdit,
        _ui->genPoster,
        _ui->saveFromCB,
        _ui->uniqueFromCB,
        _ui->rarPassCB,
        _ui->rarPassEdit,
        _ui->rarLengthSB,
        _ui->genPass,
        _ui->groupsLbl,
        _ui->groupsEdit,
        _ui->frame_3,
        _ui->articleSizeLbl,
        _ui->articleSizeEdit,
        _ui->nbRetryLbl,
        _ui->nbRetrySB,
        _ui->threadLbl,
        _ui->threadSB,
        _ui->frame_2,
        _ui->obfuscateMsgIdCB,
        _ui->obfuscateFileNameCB,
        _ui->autoCompressCB,
        _ui->autoCloseCB,
        _ui->frame_4,
        _ui->nzbPathLbl,
        _ui->nzbPathEdit,
        _ui->nzbPathButton,
        _ui->shutdownCB,
        _ui->langLbl,
        _ui->langCB,
        _ui->themeButton,
        _ui->saveButton
    };
    for (QWidget *widget : visibleWidgets)
    {
        if (widget)
            widget->show();
    }
}

void MainWindow::_initSettingsDialogs()
{
    auto setupDialog = [this](QDialog *dialog, QGroupBox *groupBox, const QString &title, int minWidth) {
        dialog->setWindowTitle(title);
        dialog->setModal(false);
        dialog->setAttribute(Qt::WA_DeleteOnClose, false);
        dialog->setProperty("compactMinWidth", minWidth);
        dialog->setProperty("compactMinHeight", 420);

        QVBoxLayout *layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        QScrollArea *scrollArea = new QScrollArea(dialog);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setObjectName("settingsScrollArea");
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setWidget(groupBox);
        layout->addWidget(scrollArea, 1);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
        connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::hide);
        layout->addWidget(buttonBox);

        _fitDialogToContent(dialog);
    };

    _serversDialog = new QDialog(this);
    _preferencesDialog = new QDialog(this);

    setupDialog(_serversDialog, _ui->serverBox, tr("Server Settings"), 660);
    setupDialog(_preferencesDialog, _ui->postingBox, tr("Preferences"), 620);
    _preferencesDialog->setProperty("compactMinHeight", 560);

    _ui->hSplitter->hide();
    QList<int> sizes;
    sizes << 0 << 1;
    _ui->vSplitter->setSizes(sizes);
}

void MainWindow::_initSettingsMenu()
{
    _settingsMenu = menuBar()->addMenu(tr("Settings"));
    _openServersAction = _settingsMenu->addAction(tr("Servers..."), this, &MainWindow::onOpenServersDialog);
    _openPreferencesAction = _settingsMenu->addAction(tr("Preferences..."), this, &MainWindow::onOpenPreferencesDialog);
    _settingsMenu->addSeparator();
    _saveConfigAction = _settingsMenu->addAction(tr("Save Config"), this, &MainWindow::onSaveConfig);

    _viewMenu = menuBar()->addMenu(tr("View"));
    _toggleFullscreenAction = _viewMenu->addAction(tr("Full Screen"), this, &MainWindow::onToggleFullscreen);
    _toggleFullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    _toggleFullscreenAction->setShortcutContext(Qt::ApplicationShortcut);
    _toggleFullscreenAction->setCheckable(true);
    _viewMenu->addSeparator();

    _scaleMenu = _viewMenu->addMenu(tr("Scale"));
    _scaleActionGroup = new QActionGroup(this);
    _scaleActionGroup->setExclusive(true);

    const QList<int> scalePercents = {100, 110, 123, 150};
    for (int percent : scalePercents)
    {
        QAction *action = _scaleMenu->addAction(QString("%1%").arg(percent));
        action->setCheckable(true);
        action->setData(percent);
        _scaleActionGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, percent]() {
            _applyUiScale(percent / 100.0, true);
        });
    }

    _aboutMenuAction = menuBar()->addAction(tr("About"));
    connect(_aboutMenuAction, &QAction::triggered, this, &MainWindow::onOpenAboutDialog);

    _helpMenuAction = menuBar()->addAction(tr("Help"));
    connect(_helpMenuAction, &QAction::triggered, this, &MainWindow::onOpenHelpDialog);
}

void MainWindow::_buildModernShell()
{
    _ui->verticalLayout_5->removeWidget(_ui->vSplitter);
    _ui->verticalLayout_5->removeWidget(_ui->uploadFrame);

    _shellRoot = new QFrame(_ui->centralwidget);
    _shellRoot->setObjectName("appShell");
    _shellRoot->setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *shellLayout = new QVBoxLayout(_shellRoot);
    shellLayout->setContentsMargins(10, 10, 10, 8);
    shellLayout->setSpacing(8);

    QFrame *topHeader = new QFrame(_shellRoot);
    topHeader->setObjectName("topHeader");
    topHeader->setAttribute(Qt::WA_StyledBackground, true);

    auto createNavButton = [topHeader](const char *name) {
        QPushButton *button = new QPushButton(topHeader);
        button->setObjectName(QString::fromLatin1(name));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(20);
        button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        button->setProperty("shellRole", "nav");
        return button;
    };

    auto createUtilityButton = [topHeader](const char *name) {
        QPushButton *button = new QPushButton(topHeader);
        button->setObjectName(QString::fromLatin1(name));
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(20);
        button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        button->setProperty("shellRole", "utility");
        return button;
    };

    QVBoxLayout *topHeaderLayout = new QVBoxLayout(topHeader);
    topHeaderLayout->setContentsMargins(12, 10, 12, 8);
    topHeaderLayout->setSpacing(6);

    QHBoxLayout *brandRow = new QHBoxLayout();
    brandRow->setSpacing(8);

    QVBoxLayout *brandLayout = new QVBoxLayout();
    brandLayout->setSpacing(2);

    QHBoxLayout *brandTitleRow = new QHBoxLayout();
    brandTitleRow->setSpacing(8);

    QLabel *brandLabel = new QLabel(NgPost::displayName(), topHeader);
    brandLabel->setProperty("shellRole", "brand");
    brandTitleRow->addWidget(brandLabel);

    _sidebarVersionLabel = new QLabel(topHeader);
    _sidebarVersionLabel->setProperty("shellRole", "versionBadge");
    brandTitleRow->addWidget(_sidebarVersionLabel, 0, Qt::AlignVCenter);
    brandTitleRow->addStretch(1);
    brandLayout->addLayout(brandTitleRow);

    _sidebarTaglineLabel = new QLabel(topHeader);
    _sidebarTaglineLabel->setWordWrap(true);
    _sidebarTaglineLabel->setProperty("shellRole", "sidebarTagline");
    _sidebarTaglineLabel->hide();

    brandRow->addLayout(brandLayout, 4);
    brandRow->addStretch(1);

    QHBoxLayout *utilityLayout = new QHBoxLayout();
    utilityLayout->setSpacing(6);

    _sideServersButton = createUtilityButton("sideServersButton");
    _sidePreferencesButton = createUtilityButton("sidePreferencesButton");
    _sideSaveButton = createUtilityButton("sideSaveButton");
    _sideFullscreenButton = createUtilityButton("sideFullscreenButton");
    _sideHelpButton = createUtilityButton("sideHelpButton");
    utilityLayout->addWidget(_sideServersButton);
    utilityLayout->addWidget(_sidePreferencesButton);
    utilityLayout->addWidget(_sideSaveButton);
    utilityLayout->addWidget(_sideFullscreenButton);
    utilityLayout->addWidget(_sideHelpButton);
    brandRow->addLayout(utilityLayout, 0);
    topHeaderLayout->addLayout(brandRow);

    _overviewNavButton = createNavButton("overviewNavButton");
    _quickNavButton = createNavButton("quickNavButton");
    _autoNavButton = createNavButton("autoNavButton");
    _activityNavButton = createNavButton("activityNavButton");

    QHBoxLayout *navLayout = new QHBoxLayout();
    navLayout->setSpacing(6);
    navLayout->addWidget(_overviewNavButton);
    navLayout->addWidget(_autoNavButton);
    navLayout->addWidget(_activityNavButton);
    navLayout->addStretch(1);

    connect(_overviewNavButton, &QPushButton::clicked, this, [this]() { _setShellView(ShellView::Overview); });
    connect(_quickNavButton, &QPushButton::clicked, this, [this]() { _setShellView(ShellView::QuickPost); });
    connect(_autoNavButton, &QPushButton::clicked, this, [this]() { _setShellView(ShellView::AutoPosting); });
    connect(_activityNavButton, &QPushButton::clicked, this, [this]() { _setShellView(ShellView::Activity); });
    _quickNavButton->hide();

    connect(_sideServersButton, &QPushButton::clicked, this, &MainWindow::onOpenServersDialog);
    connect(_sidePreferencesButton, &QPushButton::clicked, this, &MainWindow::onOpenPreferencesDialog);
    connect(_sideSaveButton, &QPushButton::clicked, this, &MainWindow::onSaveConfig);
    connect(_sideFullscreenButton, &QPushButton::clicked, this, &MainWindow::onToggleFullscreen);
    connect(_sideHelpButton, &QPushButton::clicked, this, &MainWindow::onOpenHelpDialog);
    _sideFullscreenButton->hide();

    _sidebarFooterLabel = new QLabel(topHeader);
    _sidebarFooterLabel->setWordWrap(true);
    _sidebarFooterLabel->setProperty("shellRole", "sidebarFooter");
    _sidebarFooterLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    _sidebarFooterLabel->hide();

    auto createStatCard = [topHeader](QLabel *&titleLabel, QLabel *&valueLabel) {
        QFrame *card = new QFrame(topHeader);
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setProperty("shellSurface", "card");
        card->setProperty("cardVariant", "compact");
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        card->setMinimumHeight(34);
        card->setMaximumHeight(36);
        QHBoxLayout *layout = new QHBoxLayout(card);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(5);
        titleLabel = new QLabel(card);
        titleLabel->setProperty("shellRole", "cardTitle");
        valueLabel = new QLabel(card);
        valueLabel->setWordWrap(false);
        valueLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        valueLabel->setProperty("shellRole", "cardValue");
        layout->addWidget(titleLabel);
        layout->addWidget(valueLabel, 1);
        return card;
    };

    _overviewStatsFrame = new QFrame(topHeader);
    _overviewStatsFrame->setAttribute(Qt::WA_StyledBackground, true);
    _overviewStatsFrame->setProperty("shellSurface", "headerStats");
    _overviewStatsFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QGridLayout *statsLayout = new QGridLayout(_overviewStatsFrame);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setHorizontalSpacing(5);
    statsLayout->setVerticalSpacing(0);
    statsLayout->addWidget(createStatCard(_sessionCardTitleLabel, _sessionCardValueLabel), 0, 0);
    statsLayout->addWidget(createStatCard(_serversCardTitleLabel, _serversCardValueLabel), 0, 1);
    statsLayout->addWidget(createStatCard(_themeCardTitleLabel, _themeCardValueLabel), 0, 2);
    statsLayout->addWidget(createStatCard(_archiverCardTitleLabel, _archiverCardValueLabel), 0, 3);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(8);
    bottomRow->addLayout(navLayout, 2);
    bottomRow->addWidget(_overviewStatsFrame, 5);
    topHeaderLayout->addLayout(bottomRow);
    shellLayout->addWidget(topHeader);

    QFrame *contentCanvas = new QFrame(_shellRoot);
    contentCanvas->setObjectName("contentCanvas");
    contentCanvas->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout *contentLayout = new QVBoxLayout(contentCanvas);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);

    QFrame *contentHeader = new QFrame(contentCanvas);
    contentHeader->setAttribute(Qt::WA_StyledBackground, true);
    contentHeader->setProperty("shellSurface", "topBar");
    _workspaceHeaderFrame = contentHeader;
    QVBoxLayout *contentHeaderLayout = new QVBoxLayout(contentHeader);
    contentHeaderLayout->setContentsMargins(18, 16, 18, 16);
    contentHeaderLayout->setSpacing(4);

    _workspaceTitleLabel = new QLabel(contentHeader);
    _workspaceTitleLabel->setProperty("shellRole", "workspaceTitle");
    contentHeaderLayout->addWidget(_workspaceTitleLabel);

    _workspaceSubtitleLabel = new QLabel(contentHeader);
    _workspaceSubtitleLabel->setWordWrap(true);
    _workspaceSubtitleLabel->setProperty("shellRole", "workspaceSubtitle");
    contentHeaderLayout->addWidget(_workspaceSubtitleLabel);

    _contentStack = new QStackedWidget(contentCanvas);
    _contentStack->setObjectName("contentStack");
    contentLayout->addWidget(_contentStack, 1);
    shellLayout->addWidget(contentCanvas, 1);

    QScrollArea *overviewScroll = new QScrollArea(_contentStack);
    overviewScroll->setWidgetResizable(true);
    overviewScroll->setFrameShape(QFrame::NoFrame);
    overviewScroll->setObjectName("overviewScroll");

    QWidget *overviewContainer = new QWidget(overviewScroll);
    overviewContainer->setObjectName("overviewContainer");
    QVBoxLayout *overviewLayout = new QVBoxLayout(overviewContainer);
    overviewLayout->setContentsMargins(0, 0, 2, 0);
    overviewLayout->setSpacing(12);

    overviewLayout->addStretch(1);

    overviewScroll->setWidget(overviewContainer);
    _overviewPage = overviewScroll;

    _workspacePage = new QWidget(_contentStack);
    _workspacePage->setObjectName("workspacePage");
    QVBoxLayout *workspaceLayout = new QVBoxLayout(_workspacePage);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(8);
    workspaceLayout->addWidget(contentHeader);

    QFrame *workspaceSurface = new QFrame(_workspacePage);
    workspaceSurface->setAttribute(Qt::WA_StyledBackground, true);
    workspaceSurface->setProperty("shellSurface", "panel");
    QVBoxLayout *workspaceSurfaceLayout = new QVBoxLayout(workspaceSurface);
    workspaceSurfaceLayout->setContentsMargins(4, 4, 4, 4);
    workspaceSurfaceLayout->setSpacing(0);

    _ui->fileBox->setAttribute(Qt::WA_StyledBackground, true);
    _ui->logBox->setAttribute(Qt::WA_StyledBackground, true);
    _ui->fileBox->setParent(workspaceSurface);
    _ui->fileBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    workspaceSurfaceLayout->addWidget(_ui->fileBox, 1);
    workspaceLayout->addWidget(workspaceSurface, 1);

    _activityDock = new QDockWidget(this);
    _activityDock->setObjectName("activityDock");
    _activityDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    _activityDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    _activityDock->setAttribute(Qt::WA_StyledBackground, true);
    _ui->logBox->setParent(_activityDock);
    _activityDock->setWidget(_ui->logBox);
    addDockWidget(Qt::RightDockWidgetArea, _activityDock);
    connect(_activityDock, &QDockWidget::topLevelChanged, this, [this](bool) { _applyActivityPaneLayout(); });
    connect(_activityDock, &QDockWidget::dockLocationChanged, this, [this](Qt::DockWidgetArea) { _applyActivityPaneLayout(); });
    _ui->postSplitter->hide();

    _ui->uploadFrame->setObjectName("sessionStatusBar");
    _ui->uploadFrame->setAttribute(Qt::WA_StyledBackground, true);
    _ui->uploadFrame->setProperty("shellSurface", "status");
    workspaceLayout->addWidget(_ui->uploadFrame);

    _contentStack->addWidget(_overviewPage);
    _contentStack->addWidget(_workspacePage);
    _contentStack->setCurrentWidget(_workspacePage);

    _ui->vSplitter->hide();
    _ui->verticalLayout_5->insertWidget(0, _shellRoot, 1);

    _refreshShellText();
    _refreshShellSummary();
    _refreshWindowModeUi();
    _applyActivityPaneLayout();
}

void MainWindow::_initHelpDialog()
{
    const QSize helpSize(900, 640);
    _helpDialog = new QDialog(this);
    _helpDialog->setWindowTitle(tr("Help - Handleiding"));
    _helpDialog->setModal(false);
    _helpDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    _helpDialog->setProperty("compactMinWidth", helpSize.width());
    _helpDialog->setProperty("compactMinHeight", helpSize.height());
    _helpDialog->setMinimumSize(helpSize);

    QVBoxLayout *layout = new QVBoxLayout(_helpDialog);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    _helpBrowser = new QTextBrowser(_helpDialog);
    _helpBrowser->setOpenExternalLinks(true);
    _helpBrowser->setReadOnly(true);
    _helpBrowser->setHtml(_helpHtml());
    layout->addWidget(_helpBrowser);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, _helpDialog);
    connect(buttonBox, &QDialogButtonBox::rejected, _helpDialog, &QDialog::hide);
    layout->addWidget(buttonBox);

    _helpDialog->resize(helpSize);
}

void MainWindow::_showDialog(QDialog *dialog)
{
    if (!dialog)
        return;

    _fitDialogToContent(dialog);

    QScreen *targetScreen = windowHandle() && windowHandle()->screen() ? windowHandle()->screen() : qApp->primaryScreen();
    const QRect available = targetScreen ? targetScreen->availableGeometry() : QRect(0, 0, width(), height());
    QPoint topLeft = geometry().center() - QPoint(dialog->width() / 2, dialog->height() / 2);
    topLeft.setX(qBound(available.left(), topLeft.x(), available.right() - dialog->width()));
    topLeft.setY(qBound(available.top(), topLeft.y(), available.bottom() - dialog->height()));
    dialog->move(topLeft);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::_fitDialogToContent(QDialog *dialog) const
{
    if (!dialog)
        return;

    dialog->ensurePolished();
    if (dialog->layout())
        dialog->layout()->activate();
    dialog->adjustSize();

    const int minWidth = dialog->property("compactMinWidth").toInt();
    const int minHeight = dialog->property("compactMinHeight").toInt();
    QSize desiredSize = dialog->sizeHint().expandedTo(QSize(minWidth, minHeight));

    if (QScrollArea *scrollArea = dialog->findChild<QScrollArea*>("settingsScrollArea"))
    {
        if (QWidget *content = scrollArea->widget())
        {
            content->ensurePolished();
            if (content->layout())
            {
                content->layout()->setSizeConstraint(QLayout::SetMinimumSize);
                content->layout()->activate();
            }

            QSize contentSize = content->minimumSizeHint().expandedTo(content->sizeHint());
            int width = contentSize.width() + scrollArea->frameWidth() * 2;
            int height = contentSize.height() + scrollArea->frameWidth() * 2;

            if (dialog->layout())
            {
                const QMargins margins = dialog->layout()->contentsMargins();
                width += margins.left() + margins.right();
                height += margins.top() + margins.bottom();
                height += dialog->layout()->spacing();
            }

            if (QDialogButtonBox *buttonBox = dialog->findChild<QDialogButtonBox*>())
                height += buttonBox->sizeHint().height();

            desiredSize = desiredSize.expandedTo(QSize(width + 6, height + 6));
        }
    }

    QScreen *targetScreen = windowHandle() && windowHandle()->screen() ? windowHandle()->screen() : qApp->primaryScreen();
    if (targetScreen)
    {
        const QSize maxSize = targetScreen->availableGeometry().size() - QSize(120, 120);
        desiredSize = desiredSize.boundedTo(maxSize);
    }

    dialog->resize(desiredSize);
    dialog->setMinimumSize(desiredSize);
}

void MainWindow::_updateSettingsContentHints()
{
    if (_ui->serverBox && _ui->serverBox->layout())
        _ui->serverBox->layout()->setSizeConstraint(QLayout::SetMinimumSize);
    if (_ui->postingBox && _ui->postingBox->layout())
        _ui->postingBox->layout()->setSizeConstraint(QLayout::SetMinimumSize);

    if (_ui->serversTable)
    {
        const int tableWidth = _ui->serversTable->horizontalHeader()->length()
                             + _ui->serversTable->frameWidth() * 2
                             + 6;
        _ui->serversTable->setMinimumWidth(tableWidth);
        _ui->serverBox->setMinimumWidth(tableWidth + 28);
    }

    if (_ui->postingBox && _ui->postingBox->layout())
    {
        const int postingWidth = _ui->postingBox->layout()->minimumSize().width() + 28;
        _ui->postingBox->setMinimumWidth(postingWidth);
    }
}

QString MainWindow::_helpHtml() const
{
    const QString appName = NgPost::displayName();
    const bool darkMode = (_theme == Theme::Dark);
    const QColor accent = currentAccentColor();
    const QString bodyBg = darkMode ? "#0F1722" : "#F8FBFF";
    const QString cardBg = darkMode ? "#182434" : "#FFFFFF";
    const QString textColor = darkMode ? "#EAF2FB" : "#1C2733";
    const QString mutedColor = darkMode ? "#B9CAD9" : "#55667A";
    const QString borderColor = darkMode ? "#36506A" : "#C7D7E6";
    const QString codeBg = darkMode ? "#111C29" : "#EEF4FB";
    const QString noteBg = darkMode ? "#17364C" : "#E6F5FF";
    const QString noteBorder = darkMode ? "#3EC9C1" : accent.name();
    const QString codeStyle = QString("background:%1; border:1px solid %2; border-radius:4px; padding:1px 4px;")
                                  .arg(codeBg, borderColor);
    const QString settingsLabel = tr("Settings");
    const QString serversLabel = tr("Servers...");
    const QString preferencesLabel = tr("Preferences...");
    const QString saveConfigLabel = tr("Save Config");
    const QString quickPostLabel = _ngPost ? _ngPost->quickJobName() : tr("Quick Post");
    const QString autoPostLabel = _ngPost ? _ngPost->folderMonitoringName() : tr("Auto Posting");
    auto paragraph = [](const QString &text) {
        return QString("<p>%1</p>").arg(text);
    };
    auto item = [](const QString &text) {
        return QString("<li>%1</li>").arg(text);
    };

    QString html = "<html><head><style>";
    html += QString("body { font-family: 'Segoe UI'; font-size: 10pt; line-height: 1.5; color: %1; background: %2; margin: 0; padding: 0; }")
                .arg(textColor, bodyBg);
    html += QString(".page { max-width: 980px; margin: 0 auto; background: %1; border: 1px solid %2; border-radius: 16px; padding: 22px 24px; }")
                .arg(cardBg, borderColor);
    html += QString("h1 { font-size: 20pt; margin: 0 0 8px 0; color: %1; }").arg(textColor);
    html += QString("h2 { font-size: 13.5pt; margin-top: 18px; margin-bottom: 6px; color: %1; border-bottom: 1px solid %2; padding-bottom: 4px; }")
                .arg(textColor, borderColor);
    html += QString("p, li { color: %1; }").arg(textColor);
    html += "ol, ul { margin-top: 4px; margin-bottom: 10px; padding-left: 20px; }";
    html += "li { margin: 4px 0; }";
    html += QString("code { background:%1; border:1px solid %2; border-radius:4px; padding:1px 4px; color:%3; }")
                .arg(codeBg, borderColor, textColor);
    html += QString(".note { background:%1; border:1px solid %2; padding:12px 14px; border-radius:10px; color:%3; }")
                .arg(noteBg, noteBorder, textColor);
    html += QString(".subtle { color:%1; }").arg(mutedColor);
    html += "</style></head><body>";

    html += "<div class='page'>";
    html += QString("<h1>%1</h1>").arg(tr("%1 - Gebruikershandleiding").arg(appName));
    html += paragraph(tr("Met %1 kun je bestanden of mappen comprimeren, optioneel PAR2-bestanden maken, posten naar Usenet en automatisch een NZB-bestand laten schrijven.").arg(appName));
    html += QString("<div class='note'><b>%1</b><br/>%2</div>")
                .arg(tr("Belangrijk"))
                .arg(tr("De standaard release levert <code style=\"%1\">rar.exe</code> al mee en gebruikt die automatisch naast <code style=\"%1\">ngPost.exe</code>. Je hoeft in de GUI dus geen RAR-pad meer te kiezen.").arg(codeStyle));

    html += QString("<h2>%1</h2>").arg(tr("Snelle start"));
    html += "<ol>";
    html += item(tr("Start de standaard release; daarin wordt <code style=\"%1\">rar.exe</code> al meegeleverd en automatisch gebruikt.").arg(codeStyle));
    html += item(tr("Open <b>%1 &gt; %2</b> en vul je Usenet-provider in.").arg(settingsLabel, serversLabel));
    html += item(tr("Open <b>%1 &gt; %2</b> en stel poster, groups, threads en NZB-pad in.").arg(settingsLabel, preferencesLabel));
    html += item(tr("Gebruik de tab <b>%1</b> om handmatig bestanden te posten, of <b>%2</b> om een map te scannen of te monitoren.").arg(quickPostLabel, autoPostLabel));
    html += item(tr("Klik op <b>%1 &gt; %2</b> zodat je instellingen bewaard blijven.").arg(settingsLabel, saveConfigLabel));
    html += "</ol>";

    html += QString("<h2>%1</h2>").arg(tr("1. Benodigde bestanden"));
    html += "<ul>";
    html += item(tr("In de standaard release staat <code style=\"%1\">rar.exe</code> al naast <code style=\"%1\">ngPost.exe</code>. Alleen bij een losse portable build moet je zelf een ondersteunde archiver meegeven.").arg(codeStyle));
    html += item(tr("Gebruik je <code style=\"%1\">7z.exe</code>, zet dan meestal ook <code style=\"%1\">7z.dll</code> in dezelfde map.").arg(codeStyle));
    html += item(tr("Wil je PAR2 gebruiken, zorg dan dat een werkende <code style=\"%1\">par2.exe</code> beschikbaar is.").arg(codeStyle));
    html += item(tr("Je NZB-uitvoerpad en tijdelijke compressiemap moeten schrijfbaar zijn."));
    html += "</ul>";

    html += QString("<h2>%1</h2>").arg(tr("2. Servers instellen"));
    html += paragraph(tr("Ga naar <b>%1 &gt; %2</b>. Voeg daar minimaal een server toe.").arg(settingsLabel, serversLabel));
    html += "<ul>";
    html += item(tr("<b>Host</b>: servernaam van je provider, bijvoorbeeld <code style=\"%1\">news.provider.tld</code>.").arg(codeStyle));
    html += item(tr("<b>Port</b>: vaak <code style=\"%1\">563</code> voor SSL of <code style=\"%1\">119</code> zonder SSL, maar volg altijd de gegevens van je provider.").arg(codeStyle));
    html += item(tr("<b>SSL</b>: inschakelen als je provider een beveiligde verbinding ondersteunt."));
    html += item(tr("<b>Connections</b>: gebruik niet meer verbindingen dan je provider toestaat."));
    html += item(tr("<b>Username / Password</b>: vul je accountgegevens in als authenticatie verplicht is."));
    html += item(tr("Laat de server <b>aan</b> staan in de eerste kolom, anders wordt hij niet gebruikt."));
    html += "</ul>";

    html += QString("<h2>%1</h2>").arg(tr("3. Voorkeuren instellen"));
    html += paragraph(tr("Ga naar <b>%1 &gt; %2</b>. Hier stel je de algemene werking van %3 in.").arg(settingsLabel, preferencesLabel, appName));
    html += "<ul>";
    html += item(tr("<b>Poster</b>: e-mailadres dat in headers en NZB gebruikt wordt."));
    html += item(tr("<b>Groups</b>: de nieuwsgroepen waar je wilt posten, komma-gescheiden, bijvoorbeeld <code style=\"%1\">alt.binaries.test</code>.").arg(codeStyle));
    html += item(tr("<b>Archive password</b>: optioneel vast wachtwoord voor alle posts."));
    html += item(tr("<b>Article size</b>: grootte van Usenet-artikelen. Laat dit staan als je provider hier geen speciale eisen voor heeft."));
    html += item(tr("<b>Retry</b>: aantal nieuwe pogingen per mislukt artikel."));
    html += item(tr("<b>Threads</b>: aantal uploadthreads. Meer is niet altijd sneller."));
    html += item(tr("<b>NZB path</b>: map waar NZB-bestanden standaard moeten worden opgeslagen."));
    html += item(tr("<b>Auto compress</b>: laat nieuwe quick posts automatisch compressie/par2/wachtwoordlogica overnemen."));
    html += "</ul>";

    html += QString("<h2>%1</h2>").arg(tr("4. Quick Post gebruiken"));
    html += paragraph(tr("De tab <b>%1</b> is voor handmatig posten van losse bestanden of mappen.").arg(quickPostLabel));
    html += "<ol>";
    html += item(tr("Kies bestanden via <b>Select Files</b> of een map via <b>Select Folder</b>."));
    html += item(tr("Voor mappen moet <b>compress</b> aan staan; zonder compressie kun je geen map posten."));
    html += item(tr("Kies een tijdelijke map bij <b>compress path</b>. Daar worden archieven en eventuele PAR2-bestanden gemaakt."));
    html += item(tr("Stel optioneel een archiefnaam, wachtwoord, volume-grootte en PAR2-redundantie in."));
    html += item(tr("Controleer het NZB-bestandspad en klik op <b>Post Files</b>."));
    html += "</ol>";

    html += QString("<h2>%1</h2>").arg(tr("5. Auto Posting / Monitoring"));
    html += paragraph(tr("De tab <b>%1</b> is bedoeld voor bulkwerk en automatisch verwerken.").arg(autoPostLabel));
    html += "<ol>";
    html += item(tr("Stel eerst <b>compress path</b> in."));
    html += item(tr("Kies bij <b>Auto Dir</b> de map die je wilt scannen of monitoren."));
    html += item(tr("Gebruik <b>Scan</b> om bestaande bestanden op te halen en desnoods handmatig uit de lijst te verwijderen."));
    html += item(tr("Gebruik <b>Generate Posts</b> om voor elk bestand of mapje een quick post-tab aan te maken."));
    html += item(tr("Gebruik <b>Monitor Folder</b> om nieuwe binnenkomende bestanden automatisch op te pakken."));
    html += item(tr("Opties zoals <b>generate random name</b>, <b>generate random password</b>, <b>generate par2</b> en <b>delete files once posted</b> werken hier op alle nieuwe jobs."));
    html += item(tr("Gebruik <b>delete files once posted</b> alleen als je zeker weet dat de bronbestanden na succes weg mogen."));
    html += "</ol>";

    html += QString("<h2>%1</h2>").arg(tr("6. Config opslaan"));
    html += paragraph(tr("Als alles goed staat, kies <b>%1 &gt; %2</b>. Daarmee worden je serverinstellingen, voorkeuren en relevante paden opgeslagen voor een volgende start.").arg(settingsLabel, saveConfigLabel));
    html += paragraph(tr("De meegeleverde archiver hoeft niet handmatig in het configbestand te staan; %1 gebruikt die automatisch.").arg(appName));
    html += paragraph(tr("Bij een normale installatie wordt de config opgeslagen in je gebruikersprofiel. Kies je in de installer voor <b>portable mode</b>, dan vraagt de setup ook om een portable map en wordt <code style=\"%1\">ngPost.conf</code> lokaal naast <code style=\"%1\">ngPost.exe</code> opgeslagen.").arg(codeStyle));

    html += QString("<h2>%1</h2>").arg(tr("7. Veelvoorkomende problemen"));
    html += "<ul>";
    html += item(tr("<b>Geen archiver gevonden</b>: controleer of de meegeleverde <code style=\"%1\">rar.exe</code> nog naast <code style=\"%1\">ngPost.exe</code> staat. Gebruik je een losse portable build, zet dan <code style=\"%1\">rar.exe</code> of een ondersteunde 7-Zip-binary in dezelfde map.").arg(codeStyle));
    html += item(tr("<b>Kan niet verbinden</b>: controleer host, port, SSL, gebruikersnaam, wachtwoord en het toegestane aantal connecties."));
    html += item(tr("<b>Map kan niet gepost worden</b>: zet compressie aan voordat je een map probeert te posten."));
    html += item(tr("<b>NZB wordt niet weggeschreven</b>: controleer of het NZB-pad bestaat en schrijfbaar is."));
    html += item(tr("<b>PAR2 werkt niet</b>: controleer of je een bruikbare <code style=\"%1\">par2.exe</code> beschikbaar hebt.").arg(codeStyle));
    html += item(tr("<b>Provider weigert uploads</b>: verlaag eventueel het aantal connecties of controleer je accountstatus."));
    html += "</ul>";

    html += QString("<h2>%1</h2>").arg(tr("8. Aanbevolen werkwijze"));
    html += "<ul>";
    html += item(tr("Test eerst met een klein bestand in <b>%1</b>.").arg(quickPostLabel));
    html += item(tr("Controleer daarna of het NZB-bestand wordt aangemaakt en of je provider geen fouten geeft in het logvenster."));
    html += item(tr("Pas daarna volume-groottes, PAR2 en monitoring verder aan voor grotere workloads."));
    html += item(tr("Bewaar je config zodra een werkende combinatie is gevonden."));
    html += "</ul>";

    html += "</div></body></html>";
    return html;
}

QString MainWindow::_overviewGuideHtml() const
{
    const QString quickPostName = _ngPost ? _ngPost->quickJobName() : tr("Quick Post");
    const QString autoPostName = _ngPost ? _ngPost->folderMonitoringName() : tr("Auto Posting");
    const QString settingsLabel = tr("Settings");
    const QString serversLabel = tr("Servers...");
    const QString preferencesLabel = tr("Preferences...");
    const QString saveConfigLabel = tr("Save Config");
    const bool hasArchiver = _ngPost && !_ngPost->_rarPath.isEmpty();
    const QString archiver = hasArchiver
        ? QFileInfo(_ngPost->_rarPath).fileName()
        : tr("none detected yet");
    const QString textColor = (_theme == Theme::Dark) ? "#EAF2FB" : "#1C2733";
    const QString mutedColor = (_theme == Theme::Dark) ? "#B8C8D8" : "#526171";
    const QString accentColor = currentAccentColor().name();

    QString html;
    html += QString("<div style='font-size:10pt; line-height:1.6; color:%1;'>").arg(textColor);
    html += "<ol style='margin:0 0 10px 18px; padding:0;'>";
    html += QString("<li style='margin:0 0 6px 0; color:%1;'>%2</li>").arg(textColor,
                 tr("Open <b>%1 &gt; %2</b> and add at least one enabled Usenet provider.").arg(settingsLabel, serversLabel));
    html += QString("<li style='margin:0 0 6px 0; color:%1;'>%2</li>").arg(textColor,
                 tr("Open <b>%1 &gt; %2</b> and confirm groups, threads, NZB path and posting defaults.").arg(settingsLabel, preferencesLabel));
    html += QString("<li style='margin:0 0 6px 0; color:%1;'>%2</li>").arg(textColor,
                 tr("Use %1 for one-off uploads or %2 for folder scanning and monitoring.")
                                                                  .arg(quickPostName, autoPostName));
    html += QString("<li style='margin:0; color:%1;'>%2</li>").arg(textColor,
                 tr("Save your configuration once the workflow behaves the way you want via <b>%1 &gt; %2</b>.").arg(settingsLabel, saveConfigLabel));
    html += "</ol>";
    html += QString("<p style='margin:8px 0 0 0; color:%1;'><b style='color:%2;'>%3</b> %4</p>")
                .arg(mutedColor,
                     accentColor,
                     tr("Archiver status:"),
                     hasArchiver
                         ? tr("%1 is currently the active bundled archiver.").arg(archiver)
                         : tr("No bundled archiver detected yet. The standard release should already include rar.exe next to ngPost.exe."));
    html += "</div>";
    return html;
}

void MainWindow::_refreshShellText()
{
    if (!_shellRoot)
        return;

    const QString quickPostName = _ngPost ? _ngPost->quickJobName() : tr("Quick Post");
    const QString autoPostName = _ngPost ? _ngPost->folderMonitoringName() : tr("Auto Posting");

    if (_sidebarVersionLabel)
        _sidebarVersionLabel->setText(QString("v%1").arg(NgPost::sVersion));
    if (_sidebarTaglineLabel)
        _sidebarTaglineLabel->setText(tr("Modern posting cockpit for quick uploads, automation and live activity."));
    if (_sidebarFooterLabel)
        _sidebarFooterLabel->setText(tr("Everything important stays close: settings, workspace, logs and help."));

    if (_overviewNavButton)
        _overviewNavButton->setText(quickPostName);
    if (_quickNavButton)
        _quickNavButton->setText(quickPostName);
    if (_autoNavButton)
        _autoNavButton->setText(autoPostName);
    if (_activityNavButton)
        _activityNavButton->setText(tr("Activity"));

    if (_sideServersButton)
        _sideServersButton->setText(tr("Servers"));
    if (_sidePreferencesButton)
        _sidePreferencesButton->setText(tr("Preferences"));
    if (_sideSaveButton)
        _sideSaveButton->setText(tr("Save Config"));
    if (_sideHelpButton)
        _sideHelpButton->setText(tr("Help"));

    if (_overviewEyebrowLabel)
        _overviewEyebrowLabel->setText(tr("READY TO SHIP"));
    if (_overviewTitleLabel)
        _overviewTitleLabel->setText(tr("A cleaner, modern control center for posting and monitoring."));
    if (_overviewSubtitleLabel)
        _overviewSubtitleLabel->setText(tr("%1 keeps the proven posting engine intact, but wraps it in a faster workflow shell with clearer navigation and a more modern visual language.")
                                        .arg(NgPost::displayName()));

    if (_quickActionButton)
        _quickActionButton->setText(tr("Open %1").arg(quickPostName));
    if (_autoActionButton)
        _autoActionButton->setText(tr("Open %1").arg(autoPostName));
    if (_serversActionButton)
        _serversActionButton->setText(tr("Server Settings"));
    if (_preferencesActionButton)
        _preferencesActionButton->setText(tr("Preferences"));

    if (_sessionCardTitleLabel)
        _sessionCardTitleLabel->setText(tr("Session"));
    if (_serversCardTitleLabel)
        _serversCardTitleLabel->setText(tr("Servers"));
    if (_themeCardTitleLabel)
        _themeCardTitleLabel->setText(tr("Theme"));
    if (_archiverCardTitleLabel)
        _archiverCardTitleLabel->setText(tr("Archiver"));
    if (_overviewGuideTitleLabel)
        _overviewGuideTitleLabel->setText(tr("Recommended Workflow"));
    if (_overviewGuideLabel)
        _overviewGuideLabel->setText(_overviewGuideHtml());

    _ui->serverBox->setTitle(tr("Servers"));
    _ui->postingBox->setTitle(tr("Preferences"));
    _ui->fileBox->setTitle(tr("Workspace"));
    _ui->logBox->setTitle(QString());
    if (_activityDock)
        _activityDock->setWindowTitle(tr("Activity"));

    _refreshWindowModeUi();
    _updateWorkspaceHeader();
    _refreshWorkspaceTabBar();
    _refreshShellSummary();
}

void MainWindow::_refreshWindowModeUi()
{
    const bool fullscreen = isFullScreen();
    const QString fullscreenLabel = fullscreen ? tr("Exit Full Screen") : tr("Full Screen");

    if (_viewMenu)
        _viewMenu->setTitle(tr("View"));
    if (_toggleFullscreenAction)
    {
        _toggleFullscreenAction->setText(fullscreenLabel);
        _toggleFullscreenAction->setChecked(fullscreen);
    }
    if (_sideFullscreenButton)
        _sideFullscreenButton->setText(fullscreenLabel);
}

void MainWindow::_refreshScaleMenu()
{
    if (!_scaleMenu || !_scaleActionGroup)
        return;

    _scaleMenu->setTitle(tr("Scale"));
    const int currentPercent = qRound(_uiScale * 100.0);
    for (QAction *action : _scaleActionGroup->actions())
    {
        const int percent = action->data().toInt();
        action->setText(QString("%1%").arg(percent));
        action->setChecked(percent == currentPercent);
    }
}

void MainWindow::_applyUiScale(double scale, bool persist)
{
    const double previousScale = (_uiScale > 0.0 ? _uiScale : 1.0);
    const QSize previousSize = size();
    const bool canResizeWindow = !isMaximized() && !isFullScreen();
    _uiScale = qBound(1.0, scale, 1.5);

    const int uploadBarHeight = qRound(42 * _uiScale);
    const int pauseButtonSize = qRound(28 * _uiScale);
    const int pauseIconSize = qRound(14 * _uiScale);
    const int progressHeight = qRound(16 * _uiScale);

    _ui->uploadFrame->setMaximumHeight(uploadBarHeight);
    _ui->jobLabel->setMinimumWidth(qRound(72 * _uiScale));
    _ui->pauseButton->setFixedSize(pauseButtonSize, pauseButtonSize);
    _ui->pauseButton->setIconSize(QSize(pauseIconSize, pauseIconSize));
    _ui->progressBar->setMinimumHeight(progressHeight);
    _ui->progressBar->setMaximumHeight(progressHeight);
    _ui->uploadLbl->setMinimumWidth(qRound(220 * _uiScale));
    _ui->postTabWidget->tabBar()->setIconSize(QSize(qRound(18 * _uiScale), qRound(18 * _uiScale)));

    if (_helpDialog)
    {
        const QSize helpSize(qRound(900 * _uiScale), qRound(640 * _uiScale));
        _helpDialog->setMinimumSize(helpSize);
        if (_helpDialog->size().width() < helpSize.width() || _helpDialog->size().height() < helpSize.height())
            _helpDialog->resize(helpSize);
    }

    if (_ngPost)
    {
        _ngPost->_uiScale = _uiScale;
        if (persist)
            _ngPost->saveUiScale();
    }

    applyTheme(_theme);
    if (_quickJobTab)
        _quickJobTab->applyUiScale(_uiScale);
    if (_autoPostTab)
        _autoPostTab->applyUiScale(_uiScale);
    for (int i = 2; i < _ui->postTabWidget->count() - 1; ++i)
    {
        if (PostingWidget *postWidget = _getPostWidget(i))
            postWidget->applyUiScale(_uiScale);
    }
    _updateSettingsContentHints();

    if (_shellRoot)
        _shellRoot->updateGeometry();
    if (_contentStack)
        _contentStack->updateGeometry();
    if (_workspacePage)
        _workspacePage->updateGeometry();
    if (_ui->fileBox)
        _ui->fileBox->updateGeometry();
    if (_ui->logBox)
        _ui->logBox->updateGeometry();

    if (QWidget *mainCentral = centralWidget())
    {
        if (QLayout *centralLayout = mainCentral->layout())
            centralLayout->activate();
        mainCentral->updateGeometry();
    }

    const QSize scaledMin(qRound(_baseWindowSize.width() * _uiScale),
                          qRound(_baseWindowSize.height() * _uiScale));
    setMinimumSize(scaledMin);

    if (canResizeWindow)
    {
        const double resizeRatio = previousScale > 0.0 ? (_uiScale / previousScale) : 1.0;
        QSize targetSize(qRound(previousSize.width() * resizeRatio),
                         qRound(previousSize.height() * resizeRatio));
        targetSize = targetSize.expandedTo(scaledMin);

        QScreen *targetScreen = windowHandle() && windowHandle()->screen() ? windowHandle()->screen() : qApp->primaryScreen();
        if (targetScreen)
        {
            const QSize maxSize = targetScreen->availableGeometry().size() - QSize(24, 24);
            targetSize = targetSize.boundedTo(maxSize);
        }

        resize(targetSize);
    }

    _refreshScaleMenu();
    _applyActivityPaneLayout();
    updateGeometry();
}

void MainWindow::_refreshQuickTabLabels()
{
    if (!_ngPost)
        return;

    QTabBar *bar = _ui->postTabWidget->tabBar();
    if (!bar)
        return;

    const int lastTabIdx = _ui->postTabWidget->count() - 1;
    if (lastTabIdx < 0)
        return;

    int sessionNumber = 1;
    for (int i = 0; i < lastTabIdx; ++i)
    {
        if (i == 1)
        {
            const QString autoLabel = _ngPost->folderMonitoringName();
            _ui->postTabWidget->setTabText(i, autoLabel);
            bar->setTabToolTip(i, autoLabel);
            continue;
        }

        const QString sessionLabel = tr("Session %1").arg(sessionNumber++);
        _ui->postTabWidget->setTabText(i, sessionLabel);
        bar->setTabToolTip(i, sessionLabel);
    }

    _ui->postTabWidget->setTabText(lastTabIdx, tr("New session"));
    bar->setTabToolTip(lastTabIdx, tr("Create a new %1 session").arg(_ngPost->quickJobName()));
}

void MainWindow::_refreshWorkspaceTabBar()
{
    QTabBar *bar = _ui->postTabWidget->tabBar();
    if (!bar)
        return;

    _refreshQuickTabLabels();

    const bool showQuickSessions = (_ui->postTabWidget->currentIndex() != 1);
    const int tabCount = _ui->postTabWidget->count();
    for (int i = 0; i < tabCount; ++i)
    {
        bar->setTabVisible(i, showQuickSessions ? (i != 1) : false);
        const bool showCloseButton = showQuickSessions && i > 1 && i < tabCount - 1;
        if (QWidget *rightButton = bar->tabButton(i, QTabBar::RightSide))
            rightButton->setVisible(showCloseButton);
        if (QWidget *leftButton = bar->tabButton(i, QTabBar::LeftSide))
            leftButton->setVisible(showCloseButton);
    }

    bar->setExpanding(false);
    bar->setUsesScrollButtons(true);
    bar->setMovable(false);

    if (showQuickSessions)
        bar->show();
    else
        bar->hide();
}

void MainWindow::_refreshShellSummary()
{
    if (!_sessionCardValueLabel)
        return;

    auto setSummaryLabel = [](QLabel *label, const QString &text) {
        if (!label)
            return;
        label->setText(text);
        label->setToolTip(text);
    };

    if (!_ngPost)
    {
        setSummaryLabel(_sessionCardValueLabel, tr("Booting up"));
        setSummaryLabel(_serversCardValueLabel, tr("Not loaded"));
        setSummaryLabel(_themeCardValueLabel, tr("Dark default"));
        setSummaryLabel(_archiverCardValueLabel, tr("Waiting for config"));
        return;
    }

    QString sessionState = tr("Ready");
    if (_ngPost->isPosting())
    {
        sessionState = tr("Posting");
        if (_ui->progressBar->maximum() > 0)
            sessionState = tr("Posting (%1/%2)").arg(_ui->progressBar->value()).arg(_ui->progressBar->maximum());
    }
    else if (_ngPost->hasMonitoringPostingJobs())
        sessionState = tr("Monitoring");
    else if (_ngPost->hasPostingJobs())
        sessionState = tr("Queued");

    const int workspaceCount = qMax(0, _ui->postTabWidget->count() - 1);
    setSummaryLabel(_sessionCardValueLabel, QString("%1 / %2")
                    .arg(sessionState)
                    .arg(tr("%1 active workspaces").arg(workspaceCount)));

    int totalServers = 0;
    int enabledServers = 0;
    for (NntpServerParams *server : _ngPost->_nntpServers)
    {
        ++totalServers;
        if (server && server->enabled)
            ++enabledServers;
    }
    if (totalServers == 0)
        setSummaryLabel(_serversCardValueLabel, tr("No servers configured"));
    else
        setSummaryLabel(_serversCardValueLabel, QString("%1 / %2")
                        .arg(tr("%1 enabled").arg(enabledServers))
                        .arg(tr("%1 total").arg(totalServers)));

    const QString themeMode = (_theme == Theme::Dark) ? tr("Dark mode") : tr("Light mode");
    setSummaryLabel(_themeCardValueLabel, QString("%1 / %2").arg(themeMode, tr("High-contrast palette")));

    if (_ngPost->_rarPath.isEmpty())
        setSummaryLabel(_archiverCardValueLabel, tr("No archiver detected / The standard release should already include rar.exe"));
    else
        setSummaryLabel(_archiverCardValueLabel, QString("%1 / %2")
                        .arg(QFileInfo(_ngPost->_rarPath).fileName())
                        .arg(tr("Bundled auto-detected")));
}

void MainWindow::_updateWorkspaceHeader()
{
    if (!_workspaceTitleLabel || !_workspaceSubtitleLabel)
        return;

    QString title;
    QString subtitle;

    if (_shellView == ShellView::Overview)
    {
        title = _ngPost ? _ngPost->quickJobName() : tr("Quick Post");
        subtitle = tr("Prepare manual uploads, duplicate tabs when needed and keep your live log close.");
    }
    else if (_shellView == ShellView::Activity)
    {
        title = tr("Activity Console");
        subtitle = tr("Keep provider feedback, retries and upload throughput visible while jobs are running.");
    }
    else
    {
        const int currentIndex = _ui->postTabWidget->currentIndex();
        if (currentIndex == 1)
        {
            title = _ngPost ? _ngPost->folderMonitoringName() : tr("Auto Posting");
            subtitle = tr("Scan folders, monitor incoming files and generate new posting jobs in bulk.");
        }
        else if (currentIndex > 1 && currentIndex < _ui->postTabWidget->count() - 1)
        {
            title = tr("Quick Post Session");
            subtitle = tr("Fine-tune archive options and launch one-off uploads without leaving the workspace.");
        }
        else
        {
            title = _ngPost ? _ngPost->quickJobName() : tr("Quick Post");
            subtitle = tr("Prepare manual uploads, duplicate tabs when needed and keep your live log close.");
        }
    }

    _workspaceTitleLabel->setText(title);
    _workspaceSubtitleLabel->setText(subtitle);
}

void MainWindow::_setShellView(ShellView view)
{
    _shellView = view;
    if (!_contentStack)
        return;

    _contentStack->setCurrentWidget(_workspacePage);

    if (_overviewStatsFrame)
        _overviewStatsFrame->setVisible(true);
    if (_workspaceHeaderFrame)
        _workspaceHeaderFrame->setVisible(false);
    _ui->fileBox->setVisible(true);
    _ui->logBox->setVisible(true);
    if (_activityDock)
    {
        _activityDock->show();
        if (view == ShellView::Activity && _activityDock->isFloating())
        {
            _activityDock->raise();
            _activityDock->activateWindow();
        }
    }

    if ((view == ShellView::QuickPost || view == ShellView::Overview) && _ui->postTabWidget->count() > 0)
    {
        const int currentIndex = _ui->postTabWidget->currentIndex();
        const int lastTabIdx = _ui->postTabWidget->count() - 1;
        if (currentIndex == 1 || currentIndex == lastTabIdx)
            _ui->postTabWidget->setCurrentIndex(0);
    }
    else if (view == ShellView::AutoPosting && _ui->postTabWidget->count() > 1)
        _ui->postTabWidget->setCurrentIndex(1);

    if (_overviewNavButton)
        _overviewNavButton->setChecked(view == ShellView::Overview);
    if (_quickNavButton)
        _quickNavButton->setChecked(view == ShellView::QuickPost);
    if (_autoNavButton)
        _autoNavButton->setChecked(view == ShellView::AutoPosting);
    if (_activityNavButton)
        _activityNavButton->setChecked(view == ShellView::Activity);

    _applyActivityPaneLayout();
    _updateWorkspaceHeader();
    _refreshWorkspaceTabBar();
    _refreshShellSummary();
}

void MainWindow::_applyActivityPaneLayout()
{
    if (!_ui || !_ui->fileBox || !_ui->logBox || !_activityDock)
        return;

    const bool sideBySide = width() >= qRound(1180 * _uiScale);
    const bool postingActive = _ngPost && (_ngPost->isPosting()
                                           || _ngPost->hasPostingJobs()
                                           || _ngPost->hasMonitoringPostingJobs());
    if (_activityDock->isFloating())
    {
        _ui->logBrowser->setMinimumWidth(qRound(240 * _uiScale));
        _ui->logBrowser->setMinimumHeight(qRound(140 * _uiScale));
        _ui->logBox->setMinimumWidth(qRound(300 * _uiScale));
        _ui->logBox->setMaximumWidth(QWIDGETSIZE_MAX);
        _ui->logBox->setMinimumHeight(qRound(220 * _uiScale));
        _ui->logBox->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }

    const Qt::DockWidgetArea targetArea = sideBySide ? Qt::RightDockWidgetArea : Qt::BottomDockWidgetArea;
    if (dockWidgetArea(_activityDock) != targetArea)
        addDockWidget(targetArea, _activityDock);

    if (sideBySide)
    {
        int activityWidth = qRound(250 * _uiScale);
        if (_shellView == ShellView::Activity)
            activityWidth = qBound(qRound(300 * _uiScale), width() / 4, qRound(400 * _uiScale));
        else if (postingActive)
            activityWidth = qBound(qRound(250 * _uiScale), width() / 5, qRound(340 * _uiScale));
        else
            activityWidth = qBound(qRound(230 * _uiScale), width() / 6, qRound(320 * _uiScale));

        _ui->logBrowser->setMinimumWidth(qRound(180 * _uiScale));
        _ui->logBrowser->setMinimumHeight(0);
        _ui->logBox->setMinimumWidth(activityWidth);
        _ui->logBox->setMaximumWidth(qRound(420 * _uiScale));
        _ui->logBox->setMinimumHeight(0);
        _ui->logBox->setMaximumHeight(QWIDGETSIZE_MAX);
        resizeDocks({_activityDock}, {activityWidth}, Qt::Horizontal);
        return;
    }

    int drawerHeight = qRound(102 * _uiScale);
    if (_shellView == ShellView::Activity)
        drawerHeight = qBound(qRound(180 * _uiScale), height() / 4, qRound(260 * _uiScale));
    else if (postingActive)
        drawerHeight = qBound(qRound(118 * _uiScale), height() / 6, qRound(158 * _uiScale));

    const bool compactDrawer = (_shellView != ShellView::Activity);
    _ui->logBrowser->setMinimumWidth(0);
    _ui->logBrowser->setMinimumHeight(compactDrawer ? qRound(56 * _uiScale) : qRound(110 * _uiScale));
    _ui->logBox->setMinimumWidth(0);
    _ui->logBox->setMaximumWidth(QWIDGETSIZE_MAX);
    _ui->logBox->setMinimumHeight(compactDrawer ? qRound(148 * _uiScale) : qRound(220 * _uiScale));
    _ui->logBox->setMaximumHeight(compactDrawer ? qRound(170 * _uiScale) : QWIDGETSIZE_MAX);
    resizeDocks({_activityDock}, {drawerHeight}, Qt::Vertical);
}

void MainWindow::updateServers()
{
    qDeleteAll(_ngPost->_nntpServers);
    _ngPost->_nntpServers.clear();

    int nbRows = _ui->serversTable->rowCount();
    for (int row = 0 ; row < nbRows; ++row)
    {
        int col = 0;
        bool isEnabled =  static_cast<CheckBoxCenterWidget*>(_ui->serversTable->cellWidget(row, col++))->isChecked();

        QLineEdit *hostEdit = static_cast<QLineEdit*>(_ui->serversTable->cellWidget(row, col++));
        if (hostEdit->text().isEmpty())
            continue;

        QLineEdit *portEdit = static_cast<QLineEdit*>(_ui->serversTable->cellWidget(row, col++));
        CheckBoxCenterWidget *sslCb = static_cast<CheckBoxCenterWidget*>(_ui->serversTable->cellWidget(row, col++));
        QLineEdit *nbConsEdit = static_cast<QLineEdit*>(_ui->serversTable->cellWidget(row, col++));
        QLineEdit *userEdit = static_cast<QLineEdit*>(_ui->serversTable->cellWidget(row, col++));
        QLineEdit *passEdit = static_cast<QLineEdit*>(_ui->serversTable->cellWidget(row, col++));

        NntpServerParams *srvParams = new NntpServerParams(hostEdit->text(), portEdit->text().toUShort());
        srvParams->useSSL = sslCb->isChecked();
        srvParams->nbCons = nbConsEdit->text().toInt();
        srvParams->enabled = isEnabled;
        const QString user = userEdit->text();
        const QString pass = passEdit->text();
        if (!user.isEmpty() || !pass.isEmpty())
        {
            srvParams->auth = true;
            srvParams->user = user.toUtf8().toStdString();
            srvParams->pass = pass.toUtf8().toStdString();
        }

        _ngPost->_nntpServers << srvParams;
    }
    _refreshShellSummary();
}

void MainWindow::updateParams()
{
    QString from = _ui->fromEdit->text();
    if (!from.isEmpty())
    {
        QRegularExpression email("\\w+@\\w+\\.\\w+");
        if (!email.match(from).hasMatch())
            from += QString("@%1.com").arg(_ngPost->aticleSignature().c_str());
        _ngPost->_from   = _ngPost->escapeXML(from).toStdString();
    }
    _ngPost->_genFrom  = _ui->uniqueFromCB->isChecked();
    _ngPost->_saveFrom = _ui->saveFromCB->isChecked();

    if (_ui->rarPassCB->isChecked())
    {
        _ngPost->_rarPassFixed = _ui->rarPassEdit->text();
        _ngPost->_rarPass      = _ngPost->_rarPassFixed;
    }
    else
    {
        _ngPost->_rarPassFixed.clear();
        _ngPost->_rarPass.clear();
    }

    if (_ui->autoCompressCB->isChecked() && _ngPost->_packAutoKeywords.isEmpty())
        _ngPost->_packAutoKeywords = NgPost::defaultPackAutoKeywords();
    _ngPost->enableAutoPacking(_ui->autoCompressCB->isChecked());
    _ngPost->_autoCloseTabs = _ui->autoCloseCB->isChecked();

    _ngPost->updateGroups(_ui->groupsEdit->toPlainText());

    _ngPost->_obfuscateArticles = _ui->obfuscateMsgIdCB->isChecked();
    _ngPost->_obfuscateFileName = _ui->obfuscateFileNameCB->isChecked();

    bool ok = false;
    uint articleSize = _ui->articleSizeEdit->text().toUInt(&ok);
    if (ok)
        NgPost::sArticleSize = articleSize;

    NntpArticle::setNbMaxRetry(static_cast<ushort>(_ui->nbRetrySB->value()));

    _ngPost->_nbThreads = _ui->threadSB->value();
    if (_ngPost->_nbThreads < 1)
        _ngPost->_nbThreads = 1;

    QFileInfo nzbPath(_ui->nzbPathEdit->text());
    if (nzbPath.exists() && nzbPath.isDir() && nzbPath.isWritable())
        _ngPost->_nzbPath = nzbPath.absoluteFilePath();
    _refreshShellSummary();
}

void MainWindow::updateAutoPostingParams()
{
    updateServers();
    updateParams();
    _autoPostTab->udatePostingParams();
}

QString MainWindow::fixedArchivePassword() const
{
    if (_ui->rarPassCB->isChecked())
    {
        QString pass = _ui->rarPassEdit->text();
        if (!pass.isEmpty())
            return pass;
    }
    return QString();
}

QWidget *MainWindow::_createScrollableTabPage(QWidget *contentWidget) const
{
    QScrollArea *scrollArea = new QScrollArea(_ui->postTabWidget);
    scrollArea->setObjectName("tabScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    contentWidget->setParent(scrollArea);
    contentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    contentWidget->setMinimumWidth(0);
    scrollArea->setWidget(contentWidget);
    return scrollArea;
}

QWidget *MainWindow::_tabPageContent(int tabIndex) const
{
    if (tabIndex < 0 || tabIndex >= _ui->postTabWidget->count())
        return nullptr;

    if (QScrollArea *scrollArea = qobject_cast<QScrollArea*>(_ui->postTabWidget->widget(tabIndex)))
        return scrollArea->widget();

    return _ui->postTabWidget->widget(tabIndex);
}

PostingWidget *MainWindow::addNewQuickTab(int lastTabIdx, const QFileInfoList &files)
{
    if (!lastTabIdx)
        lastTabIdx = _ui->postTabWidget->count() -1;
    PostingWidget *newPostingWidget = new PostingWidget(_ngPost, this, static_cast<uint>(lastTabIdx));
    newPostingWidget->init();
    newPostingWidget->applyUiScale(_uiScale);
    QString tabName = QString("%1 #%2").arg(_ngPost->quickJobName()).arg(lastTabIdx);
    _ui->postTabWidget->insertTab(lastTabIdx,
                                  _createScrollableTabPage(newPostingWidget),
                                  QIcon(":/icons/quick.png"),
                                  tabName);
    _ui->postTabWidget->setTabToolTip(lastTabIdx, tabName);
    _ui->postTabWidget->setCurrentIndex(lastTabIdx);
    _shellView = ShellView::QuickPost;
    _refreshWorkspaceTabBar();

    for (const QFileInfo &file : files)
        newPostingWidget->addPath(file.absoluteFilePath(), 0, file.isDir());

    return newPostingWidget;
}

void MainWindow::setTab(QWidget *postWidget)
{
    int nbJob = _ui->postTabWidget->count() -1;
    for (int i = 0 ; i < nbJob ; ++i)
    {
        if (_tabPageContent(i) == postWidget)
        {
            _ui->postTabWidget->setCurrentIndex(i);
            break;
        }
    }
}

void MainWindow::clearJobTab(QWidget *postWidget)
{
    int nbJob = _ui->postTabWidget->count() -1;
    for (int i = 0 ; i < nbJob ; ++i)
    {
        if (_tabPageContent(i) == postWidget)
        {
            QTabBar *bar = _ui->postTabWidget->tabBar();
            bar->setTabToolTip(i, "");
            bar->setTabTextColor(i, qApp->palette().color(QPalette::WindowText));
            bar->setTabIcon(i, QIcon(":/icons/quick.png"));
        }
    }
}

void MainWindow::updateJobTab(QWidget *postWidget, const QColor &color, const QIcon &icon, const QString &tooltip)
{
    int nbJob = _ui->postTabWidget->count() -1;
    for (int i = 0 ; i < nbJob ; ++i)
    {
        if (_tabPageContent(i) == postWidget)
        {
            QTabBar *bar = _ui->postTabWidget->tabBar();
            if (!tooltip.isEmpty())
                bar->setTabToolTip(i, tooltip);
            bar->setTabTextColor(i, color);
            bar->setTabIcon(i, icon);
            break;
        }
    }
}

void MainWindow::setJobLabel(int jobNumber)
{
    const QString label = jobNumber > 1 ? QString::number(jobNumber) : tr("Auto");
    _ui->jobLabel->setText(tr("Post %1").arg(label));
    _refreshShellSummary();
}


void MainWindow::_addServer(NntpServerParams *serverParam)
{
    int nbRows = _ui->serversTable->rowCount(), col = 0;
    _ui->serversTable->setRowCount(nbRows+1);

    _ui->serversTable->setCellWidget(nbRows, col++,
                                     new CheckBoxCenterWidget(_ui->serversTable,
                                                              serverParam ? serverParam->enabled : true));

    QLineEdit *hostEdit = new QLineEdit(_ui->serversTable);
    if (serverParam)
        hostEdit->setText(serverParam->host);
    hostEdit->setFrame(false);
    _ui->serversTable->setCellWidget(nbRows, col++, hostEdit);

    QLineEdit *portEdit = new QLineEdit(_ui->serversTable);
    portEdit->setFrame(false);
    portEdit->setValidator(new QIntValidator(1, 99999, portEdit));
    portEdit->setText(QString::number(serverParam ? serverParam->port : sDefaultServerPort));
    portEdit->setAlignment(Qt::AlignCenter);
    _ui->serversTable->setCellWidget(nbRows, col++, portEdit);

    _ui->serversTable->setCellWidget(nbRows, col++,
                                     new CheckBoxCenterWidget(_ui->serversTable,
                                                              serverParam ? serverParam->useSSL : sDefaultServerSSL));

    QLineEdit *nbConsEdit = new QLineEdit(_ui->serversTable);
    nbConsEdit->setFrame(false);
    nbConsEdit->setValidator(new QIntValidator(1, 99999, nbConsEdit));
    nbConsEdit->setText(QString::number(serverParam ? serverParam->nbCons : sDefaultConnections));
    nbConsEdit->setAlignment(Qt::AlignCenter);
    _ui->serversTable->setCellWidget(nbRows, col++, nbConsEdit);


    QLineEdit *userEdit = new QLineEdit(_ui->serversTable);
    if (serverParam)
        userEdit->setText(QString::fromUtf8(serverParam->user.c_str()));
    userEdit->setFrame(false);
    _ui->serversTable->setCellWidget(nbRows, col++, userEdit);

    QLineEdit *passEdit = new QLineEdit(_ui->serversTable);
    passEdit->setEchoMode(QLineEdit::EchoMode::PasswordEchoOnEdit);
    if (serverParam)
        passEdit->setText(QString::fromUtf8(serverParam->pass.c_str()));
    passEdit->setFrame(false);
    _ui->serversTable->setCellWidget(nbRows, col++, passEdit);

    QPushButton *delButton = new QPushButton(_ui->serversTable);
    delButton->setProperty("server", QVariant::fromValue(static_cast<void*>(serverParam)));
    delButton->setIcon(QIcon(":/icons/clear.png"));
    delButton->setMaximumWidth(sDeleteColumnWidth);
    connect(delButton, &QAbstractButton::clicked, this, &MainWindow::onDelServer);
    _ui->serversTable->setCellWidget(nbRows, col++, delButton);
}

int MainWindow::_serverRow(QObject *delButton)
{
    int nbRows = _ui->serversTable->rowCount(), delCol =_ui->serversTable->columnCount()-1;
    for (int row = 0 ; row < nbRows; ++row)
    {
        if (_ui->serversTable->cellWidget(row, delCol) == delButton)
            return row;
    }
    return nbRows;
}

PostingWidget *MainWindow::_getPostWidget(int tabIndex) const
{
    if(tabIndex > 1 && tabIndex < _ui->postTabWidget->count() - 1)
        return qobject_cast<PostingWidget*>(_tabPageContent(tabIndex));
    else
        return nullptr;
}

int MainWindow::_getPostWidgetIndex(PostingWidget *postWidget) const
{
    int nbJob = _ui->postTabWidget->count() -1;
    for (int i = 2; i < nbJob ; ++i)
    {
        if (_tabPageContent(i) == postWidget)
            return i;
    }
    return 0;
}

void MainWindow::_syncFixedPasswordWidgets(const QString &previousPass, const QString &pass)
{
    if (_quickJobTab)
        _quickJobTab->setNzbPassword(previousPass, pass);

    const int nbJob = _ui->postTabWidget->count() - 1;
    for (int i = 2; i < nbJob; ++i)
    {
        if (PostingWidget *postWidget = _getPostWidget(i))
            postWidget->setNzbPassword(previousPass, pass);
    }
}



void MainWindow::onGenPoster()
{
    _ui->fromEdit->setText(_ngPost->randomFrom());
}

void MainWindow::onUniqueFromToggled(bool checked)
{
    bool enabled = !checked;
    _ui->genPoster->setEnabled(enabled);
    _ui->fromEdit->setEnabled(enabled);
    _ui->saveFromCB->setEnabled(enabled);
}

void MainWindow::onRarPassToggled(bool checked)
{
    _ui->rarPassEdit->setEnabled(checked);
    _ui->rarLengthSB->setEnabled(checked);
    _ui->genPass->setEnabled(checked);
    const QString previousPass = _ngPost->_rarPassFixed;
    if (checked)
        onRarPassUpdated(_ui->rarPassEdit->text());
    else
    {
        _ngPost->_rarPassFixed.clear();
        _ngPost->_rarPass.clear();
        _syncFixedPasswordWidgets(previousPass, QString());
    }
}

void MainWindow::onRarPassUpdated(const QString &fixedPass)
{
    const QString previousPass = _ngPost->_rarPassFixed;
    _ngPost->_rarPassFixed = fixedPass;
    _syncFixedPasswordWidgets(previousPass, fixedPass);
}

void MainWindow::onArchivePass()
{
    _ui->rarPassEdit->setText(_ngPost->randomPass(static_cast<uint>(_ui->rarLengthSB->value())));
}

void MainWindow::onAutoCompressToggled(bool checked)
{
    if (checked && _ngPost->_packAutoKeywords.isEmpty())
        _ngPost->_packAutoKeywords = NgPost::defaultPackAutoKeywords();
    _ngPost->enableAutoPacking(checked);
    _autoPostTab->setPackingAuto(checked, _ngPost->_packAutoKeywords);
    if (!_quickJobTab->isPosting())
        _quickJobTab->setPackingAuto(checked, _ngPost->_packAutoKeywords);
    PostingWidget *currentQuickPost = _getPostWidget(_ui->postTabWidget->currentIndex());
    if (currentQuickPost && !currentQuickPost->isPosting())
        currentQuickPost->setPackingAuto(checked, _ngPost->_packAutoKeywords);
}

void MainWindow::onDebugToggled(bool checked)
{
#ifdef __DEBUG__
    qDebug() << "Debug mode: " << checked;
#endif
    if (checked)
    {
        if (!_ui->debugSB->value())
            _ui->debugSB->setValue(1);
        _ngPost->setDebug(static_cast<ushort>(_ui->debugSB->value()));
    }
    else
        _ngPost->setDebug(0);
    _ui->debugSB->setEnabled(checked);
}

void MainWindow::onDebugValue(int value)
{
    _ngPost->setDebug(static_cast<ushort>(value));
}


void MainWindow::onSaveConfig()
{
    updateServers();
    updateParams();
    int currentTabIdx = _ui->postTabWidget->currentIndex();
    if (currentTabIdx == 0)
        _quickJobTab->udatePostingParams();
    else if (currentTabIdx == 1)
        _autoPostTab->udatePostingParams();
    else
    {
        PostingWidget *postWidget = _getPostWidget(currentTabIdx);
        if (postWidget)
            postWidget->udatePostingParams();
    }
    _ngPost->saveConfig();
    _refreshShellSummary();
    QMessageBox::information(this,
                             tr("Settings saved"),
                             tr("Your settings have been saved."));
}

void MainWindow::onJobTabClicked(int index)
{
    int nbJob = _ui->postTabWidget->count() -1;
    qDebug() << "Click on tab: " << index << ", count: " << nbJob;
    if (index == nbJob) // click on the last tab
        addNewQuickTab(nbJob);
    _updateWorkspaceHeader();
    _refreshShellSummary();
}

void MainWindow::onCloseJob(int index)
{
    int nbJob = _ui->postTabWidget->count() -1;
    qDebug() << "onCloseJob on tab: " << index << ", count: " << nbJob;
    if (index > 1 && index < nbJob )
    {
        PostingWidget *postWidget = _getPostWidget(index);
        if (postWidget->isPosting())
        {
            QMessageBox::warning(this,
                                 tr("Quick Post is working.."),
                                 tr("The Quick post is currentling uploading.\n Please Stop it before closing it.."));
        }
        else
        {
            QWidget *tabPage = _ui->postTabWidget->widget(index);
            _ui->postTabWidget->removeTab(index);
            delete tabPage;

            if (index == nbJob - 1)
                _ui->postTabWidget->setCurrentIndex(_ui->postTabWidget->count() - 2);
            _refreshWorkspaceTabBar();
            _refreshShellSummary();
        }
    }
}

void MainWindow::closeTab(PostingWidget *postWidget)
{
    int index = _getPostWidgetIndex(postWidget);
    if (index)
    {
        int nbJob = _ui->postTabWidget->count() -1;
        QWidget *tabPage = _ui->postTabWidget->widget(index);
        _ui->postTabWidget->removeTab(index);
        delete tabPage;

        if (index == nbJob - 1)
            _ui->postTabWidget->setCurrentIndex(_ui->postTabWidget->count() - 2);
        _refreshWorkspaceTabBar();
        _refreshShellSummary();
    }
}


void MainWindow::toBeImplemented()
{
    QMessageBox::information(nullptr, "To be implemented...", "To be implemented...", QMessageBox::Ok);
}

#include <QFileDialog>
void MainWindow::onNzbPathClicked()
{
    QString path = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ui->nzbPathEdit->text(),
                QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
        _ui->nzbPathEdit->setText(path);
}

void MainWindow::onLangChanged(const QString &lang)
{
    qDebug() << "Changing lang to " << lang;
    _ngPost->changeLanguage(lang.toLower());
}

void MainWindow::onShutdownToggled(bool checked)
{
    if (checked)
    {
        int res = QMessageBox::question(this,
                                        tr("Automatic Shutdown?"),
                                        QString("%1\n%2").arg(
                                            tr("You're about to schedule the shutdown of the computer once all the current Postings will be finished")).arg(
                                            tr("Are you sure you want to switch off the computer?")),
                                        QMessageBox::Yes,
                                        QMessageBox::No);
        if (res == QMessageBox::Yes)
            _ngPost->_doShutdownWhenDone = checked;
        else
            _ui->shutdownCB->setChecked(false);
    }
    else
        _ngPost->_doShutdownWhenDone = false;
}

void MainWindow::setPauseIcon(bool pause)
{
    if (pause)
        _ui->pauseButton->setIcon(QIcon(":/icons/pause.png"));
    else
        _ui->pauseButton->setIcon(QIcon(":/icons/play.png"));
}

void MainWindow::onPauseClicked()
{
    if (_ngPost->isPosting())
    {
        if (_ngPost->isPaused())
            _ngPost->resume();
        else
            _ngPost->pause();
    }
}

void MainWindow::onThemeToggled(bool checked)
{
    if (_ngPost)
    {
        _ngPost->_themeMode = checked ? QString() : QString("light");
        _ngPost->saveThemeMode();
    }
    applyTheme(checked ? Theme::Dark : Theme::Light);
    _refreshShellSummary();
}

void MainWindow::onThemeColorClicked()
{
    const QColor initial = currentAccentColor();
    const QColor chosen = QColorDialog::getColor(initial, this, tr("Select theme color"));
    if (chosen.isValid())
    {
        _accentColor = chosen;
        if (_ngPost)
        {
            _ngPost->_themeColor = _accentColor.name().toUpper();
            _ngPost->saveThemeColor();
        }
        applyTheme(_theme);
        _refreshShellSummary();
    }
}

void MainWindow::onThemeColorResetClicked()
{
    _accentColor = QColor();
    if (_ngPost)
    {
        _ngPost->_themeColor.clear();
        _ngPost->saveThemeColor();
    }
    applyTheme(_theme);
    _refreshShellSummary();
}

void MainWindow::onToggleFullscreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();

    _refreshWindowModeUi();
}

void MainWindow::onOpenServersDialog()
{
    _showDialog(_serversDialog);
}

void MainWindow::onOpenPreferencesDialog()
{
    _showDialog(_preferencesDialog);
}

void MainWindow::onOpenAboutDialog()
{
    if (_ngPost)
        _ngPost->onAboutClicked();
}

void MainWindow::onOpenHelpDialog()
{
    if (_helpBrowser)
        _helpBrowser->setHtml(_helpHtml());
    _showDialog(_helpDialog);
}

void MainWindow::applyTheme(Theme theme)
{
    _theme = theme;
    const QSignalBlocker blocker(_ui->themeButton);
    _ui->themeButton->setChecked(_theme == Theme::Dark);
    _ui->themeButton->setEnabled(true);
    _ui->themeButton->show();

    const QColor accent = currentAccentColor();
    const QColor accentText = Qt::white;
    QString appStyle = _defaultAppStyleSheet;
    QColor shellBgStart;
    QColor shellBgEnd;
    QColor railColor;
    QColor panelColor;
    QColor cardColor;
    QColor borderColor;
    QColor strongTextColor;
    QColor mutedTextColor;
    QColor inputBgColor;
    QColor inputBorderColor;
    QColor heroStartColor;
    QColor heroEndColor;
    QColor progressBgColor;
    QColor railEndColor;
    QColor accentAltColor;
    QColor successColor;
    QColor successAltColor;
    QColor dangerColor;

    QStyle *style = QStyleFactory::create(_defaultStyleName);
    if (!style)
        style = QStyleFactory::create("Fusion");
    if (style)
        qApp->setStyle(style);

    QPalette pal = _defaultPalette;
    if (_theme == Theme::Dark)
    {
        pal.setColor(QPalette::Window, QColor(13, 20, 30));
        pal.setColor(QPalette::WindowText, QColor(235, 244, 252));
        pal.setColor(QPalette::Base, QColor(16, 25, 38));
        pal.setColor(QPalette::AlternateBase, QColor(23, 35, 50));
        pal.setColor(QPalette::ToolTipBase, QColor(24, 37, 56));
        pal.setColor(QPalette::ToolTipText, QColor(235, 244, 252));
        pal.setColor(QPalette::Text, QColor(235, 244, 252));
        pal.setColor(QPalette::Button, QColor(24, 37, 56));
        pal.setColor(QPalette::ButtonText, QColor(235, 244, 252));
        pal.setColor(QPalette::Highlight, accent);
        pal.setColor(QPalette::Link, accent);
        pal.setColor(QPalette::HighlightedText, accentText);
        pal.setColor(QPalette::PlaceholderText, QColor(171, 188, 206));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(137, 153, 173));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor(137, 153, 173));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(137, 153, 173));

        shellBgStart = QColor(7, 14, 23);
        shellBgEnd = QColor(20, 31, 47);
        railColor = QColor(17, 28, 42);
        railEndColor = QColor(22, 36, 54);
        panelColor = QColor(18, 30, 44);
        cardColor = QColor(24, 37, 56);
        borderColor = QColor(58, 84, 111);
        strongTextColor = QColor(235, 244, 252);
        mutedTextColor = QColor(182, 198, 214);
        inputBgColor = QColor(11, 19, 30);
        inputBorderColor = QColor(70, 99, 128);
        heroStartColor = QColor(18, 55, 92);
        heroEndColor = QColor(17, 97, 94);
        progressBgColor = QColor(12, 20, 31);
        accentAltColor = QColor(44, 199, 194);
        successColor = QColor(19, 171, 120);
        successAltColor = QColor(25, 198, 138);
        dangerColor = QColor(221, 88, 123);
        _ui->themeButton->setText(tr("Dark mode"));
    }
    else
    {
        pal.setColor(QPalette::Window, QColor(236, 241, 246));
        pal.setColor(QPalette::WindowText, QColor(21, 31, 42));
        pal.setColor(QPalette::Base, QColor(255, 255, 255));
        pal.setColor(QPalette::AlternateBase, QColor(244, 247, 250));
        pal.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
        pal.setColor(QPalette::ToolTipText, QColor(21, 31, 42));
        pal.setColor(QPalette::Text, QColor(21, 31, 42));
        pal.setColor(QPalette::Button, QColor(255, 255, 255));
        pal.setColor(QPalette::ButtonText, QColor(21, 31, 42));
        pal.setColor(QPalette::Highlight, accent);
        pal.setColor(QPalette::Link, accent);
        pal.setColor(QPalette::HighlightedText, accentText);
        pal.setColor(QPalette::PlaceholderText, QColor(101, 118, 136));

        shellBgStart = QColor(242, 248, 255);
        shellBgEnd = QColor(229, 239, 251);
        railColor = QColor(250, 252, 255);
        railEndColor = QColor(229, 242, 255);
        panelColor = QColor(241, 247, 253);
        cardColor = QColor(255, 255, 255);
        borderColor = QColor(192, 205, 220);
        strongTextColor = QColor(21, 31, 42);
        mutedTextColor = QColor(85, 99, 114);
        inputBgColor = QColor(255, 255, 255);
        inputBorderColor = QColor(176, 191, 208);
        heroStartColor = QColor(212, 234, 255);
        heroEndColor = QColor(216, 247, 241);
        progressBgColor = QColor(232, 240, 248);
        accentAltColor = QColor(0, 181, 173);
        successColor = QColor(14, 163, 116);
        successAltColor = QColor(12, 188, 134);
        dangerColor = QColor(205, 66, 99);
        _ui->themeButton->setText(tr("Light mode"));
    }
    qApp->setPalette(pal);

    QFont appFont = _defaultAppFont;
    const qreal baseFontSize = _defaultAppFont.pointSizeF() > 0 ? _defaultAppFont.pointSizeF() : 9.0;
    appFont.setPointSizeF(baseFontSize * _uiScale);
    qApp->setFont(appFont);

    QFont logFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    logFont.setPointSizeF(8.6 * _uiScale);
    _ui->logBrowser->document()->setDefaultFont(logFont);
    _ui->logBrowser->setFont(logFont);

    const QString hoverColor = QString("rgba(%1,%2,%3,%4)")
            .arg(accent.red())
            .arg(accent.green())
            .arg(accent.blue())
            .arg(28);
    const QColor subtleButtonStartColor = blendColor(cardColor, accent, 0.04);
    const QColor subtleButtonEndColor = blendColor(cardColor, accentAltColor, 0.08);
    const QColor subtleButtonHoverStartColor = blendColor(cardColor, accent, 0.10);
    const QColor subtleButtonHoverEndColor = blendColor(cardColor, accentAltColor, 0.14);
    const QString sideRailGradient = QString("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2)")
                                         .arg(railColor.name(), railEndColor.name());
    const QString tabSelectedGradient = QString("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 %2)")
                                            .arg(accent.name(), accentAltColor.name());
    const QString utilityGradient = QString("qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2)")
                                        .arg(subtleButtonStartColor.name(), subtleButtonEndColor.name());
    const QString utilityHoverGradient = QString("qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2)")
                                             .arg(subtleButtonHoverStartColor.name(), subtleButtonHoverEndColor.name());
    const QString primaryGradient = QString("qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2)")
                                        .arg(accent.name(), accentAltColor.name());
    const QString primaryHoverGradient = QString("qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2)")
                                             .arg(accent.lighter(115).name(), accentAltColor.lighter(112).name());
    const QString successGradient = QString("qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2)")
                                        .arg(successColor.name(), successAltColor.name());
    const QString successHoverGradient = QString("qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2)")
                                             .arg(successColor.lighter(112).name(), successAltColor.lighter(110).name());
    const QString dangerGradient = QString("qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2)")
                                       .arg(dangerColor.name(), dangerColor.lighter(118).name());
    const QString dangerHoverGradient = QString("qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2)")
                                            .arg(dangerColor.lighter(110).name(), dangerColor.lighter(128).name());
    const QString groupStyle = QString(
        "QGroupBox {"
         " font: 600 9.5pt 'Segoe UI';"
        " border: 1px solid %1;"
        " border-radius: 16px;"
        " margin-top: 14px;"
        " background-color: %2;"
        " padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        " subcontrol-origin: margin;"
        " left: 14px;"
        " padding: 0 6px;"
        " color: %3;"
        " background: transparent;"
        "}")
        .arg(borderColor.name(), cardColor.name(), accent.name());

    const QString tabStyle = QString(
        "QTabWidget::pane { border: none; background: transparent; top: -1px; }"
        "QTabBar::tab {"
        " background: %1;"
        " border: 1px solid %2;"
        " color: %3;"
        " border-radius: 7px;"
        " min-width: 54px;"
        " min-height: 20px;"
        " padding: 2px 7px;"
        " margin-right: 4px;"
        " font: 600 8pt 'Segoe UI';"
        "}"
        "QTabBar::tab:selected { background: %4; border-color: %5; color: %6; }"
        "QTabBar::tab:hover { background: %7; border-color: %5; color: %6; }"
        "QTabBar::tab:!selected { margin-top: 2px; }"
        "QTabBar::close-button { image: url(:/icons/clear.png); width: 10px; height: 10px; margin-left: 6px; }"
        "QTabBar::close-button:hover { background: %7; border-radius: 6px; }"
        "QTabBar::scroller { width: 36px; }"
        "QTabBar QToolButton::right-arrow { image: url(:/icons/right.png); }"
        "QTabBar QToolButton::left-arrow { image: url(:/icons/left.png); }")
        .arg(cardColor.name(),
             borderColor.name(),
             mutedTextColor.name(),
             tabSelectedGradient,
             accentAltColor.name(),
             accentText.name(),
             utilityHoverGradient);

    const QString scaledGroupStyle = scaledStyleText(groupStyle, _uiScale);
    const QString scaledTabStyle = scaledStyleText(tabStyle, _uiScale);
    _ui->serverBox->setStyleSheet(scaledGroupStyle);
    _ui->fileBox->setStyleSheet(scaledGroupStyle);
    _ui->postingBox->setStyleSheet(scaledGroupStyle);
    _ui->logBox->setStyleSheet(scaledGroupStyle);
    _ui->postTabWidget->setStyleSheet(scaledTabStyle);

    if (!appStyle.isEmpty())
        appStyle += "\n";
    const QString shellStyle = QString(
        "QWidget#appShell {"
        " background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 %2);"
        "}"
        "QFrame#topHeader {"
        " background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %14, stop:1 %15);"
        " border: 1px solid %4;"
        " border-radius: 18px;"
        "}"
        "QFrame#contentCanvas { background: transparent; }"
        "QFrame[shellSurface=\"topBar\"] {"
        " background: %10;"
        " border: 1px solid %4;"
        " border-radius: 18px;"
        "}"
        "QPushButton[shellRole=\"nav\"] {"
        " background: transparent;"
        " border: 1px solid transparent;"
        " border-radius: 6px;"
        " color: %5;"
        " padding: 1px 6px;"
        " text-align: center;"
        " font: 600 7.7pt 'Segoe UI';"
        "}"
        "QPushButton[shellRole=\"nav\"]:hover { background-color: %6; border-color: %4; color: %7; }"
        "QPushButton[shellRole=\"nav\"]:checked { background: %8; border-color: %9; color: %7; }"
        "QPushButton[shellRole=\"utility\"] {"
        " background: %10;"
        " border: 1px solid %9;"
        " border-radius: 6px;"
        " color: %7;"
        " padding: 1px 6px;"
        " text-align: center;"
        " font: 600 7.5pt 'Segoe UI';"
        "}"
        "QPushButton[shellRole=\"utility\"]:hover { background-color: %6; border-color: %9; }"
        "QPushButton[shellRole=\"primaryAction\"] {"
        " background: %12;"
        " border: 1px solid %9;"
        " border-radius: 7px;"
        " color: %11;"
        " padding: 4px 8px;"
        " font: 700 8.2pt 'Segoe UI';"
        "}"
        "QPushButton[shellRole=\"primaryAction\"]:hover { background: %13; border-color: %9; }"
        "QPushButton[shellRole=\"secondaryAction\"] {"
        " background: %10;"
        " border: 1px solid %9;"
        " border-radius: 7px;"
        " color: %7;"
        " padding: 4px 8px;"
        " font: 600 8.2pt 'Segoe UI';"
        "}"
        "QPushButton[shellRole=\"secondaryAction\"]:hover { background-color: %6; border-color: %9; }"
        "QFrame[shellSurface=\"hero\"] {"
        " background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %14, stop:1 %15);"
        " border: 1px solid %4;"
        " border-radius: 24px;"
        "}"
        "QFrame[shellSurface=\"card\"],"
        "QFrame[shellSurface=\"panel\"],"
        "QFrame[shellSurface=\"status\"] {"
        " background: %10;"
        " border: 1px solid %4;"
        " border-radius: 18px;"
        "}"
        "QFrame[shellSurface=\"headerStats\"] { background: transparent; border: none; }"
        "QFrame[shellSurface=\"card\"][cardVariant=\"compact\"] { border-radius: 10px; }"
        "QFrame#sessionStatusBar { border-radius: 14px; }"
        "QLabel[shellRole=\"brand\"] { color: %7; font: 700 14pt 'Bahnschrift SemiBold'; }"
        "QLabel[shellRole=\"versionBadge\"] {"
        " color: %7;"
        " background: %8;"
        " border: 1px solid %9;"
        " border-radius: 8px;"
        " padding: 2px 7px;"
        " font: 600 7.4pt 'Segoe UI';"
        "}"
        "QLabel[shellRole=\"sidebarTagline\"],"
        "QLabel[shellRole=\"sidebarFooter\"],"
        "QLabel[shellRole=\"heroSubtitle\"],"
        "QLabel[shellRole=\"workspaceSubtitle\"],"
        "QLabel[shellRole=\"guideBody\"] { color: %5; }"
        "QLabel[shellRole=\"cardTitle\"] { color: %5; font: 600 6.8pt 'Segoe UI'; }"
        "QLabel[shellRole=\"heroEyebrow\"] { color: %9; font: 700 8.5pt 'Segoe UI'; }"
        "QLabel[shellRole=\"heroTitle\"] { color: %7; font: 700 19pt 'Bahnschrift SemiBold'; }"
        "QLabel[shellRole=\"sectionTitle\"],"
        "QLabel[shellRole=\"workspaceTitle\"] { color: %7; font: 700 13pt 'Segoe UI'; }"
        "QLabel[shellRole=\"cardValue\"] { color: %7; font: 600 7.3pt 'Segoe UI'; }"
        "QLabel[shellRole=\"statusBadge\"] {"
        " color: %7;"
        " background: %16;"
        " border: 1px solid %17;"
        " border-radius: 8px;"
        " padding: 2px 8px;"
        " font: 700 7.8pt 'Segoe UI';"
        "}"
        "QLabel[shellRole=\"statusMeta\"] { color: %5; font: 600 7.6pt 'Segoe UI'; }"
        "QMenuBar { background: %3; color: %7; border-bottom: 1px solid %4; }"
        "QMenuBar::item { padding: 6px 10px; background: transparent; }"
        "QMenuBar::item:selected { background: %6; border-radius: 8px; }"
        "QMenu { background: %10; color: %7; border: 1px solid %4; padding: 6px; }"
        "QMenu::item:selected { background-color: %6; border-radius: 6px; }"
        "QDockWidget { color: %7; }"
        "QDockWidget::title {"
        " background: %10;"
        " color: %7;"
        " border: 1px solid %4;"
        " border-bottom: none;"
        " border-top-left-radius: 12px;"
        " border-top-right-radius: 12px;"
        " padding: 5px 10px;"
        " font: 600 8.2pt 'Segoe UI';"
        " text-align: left;"
        "}"
        "QDialog, QMessageBox { background: %10; color: %7; }"
        "QToolTip { background: %10; color: %7; border: 1px solid %4; padding: 6px 8px; }"
        "QTextBrowser, QTextEdit, QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTableWidget {"
        " background-color: %16;"
        " border: 1px solid %17;"
        " border-radius: 8px;"
        " padding: 4px;"
        " color: %7;"
        " selection-background-color: %9;"
        " selection-color: %11;"
        "}"
        "QComboBox QAbstractItemView { background-color: %16; color: %7; selection-background-color: %9; selection-color: %11; }"
        "QTextBrowser#logBrowser {"
        " background-color: %16;"
        " border: 1px solid %17;"
        " border-radius: 16px;"
        " color: %7;"
        " padding: 8px;"
        "}"
        "QCheckBox, QRadioButton { color: %7; spacing: 6px; }"
        "QLabel { color: %7; }"
        "QTableWidget { gridline-color: %4; }"
        "QHeaderView::section {"
        " background: %10;"
        " color: %7;"
        " border: none;"
        " border-bottom: 1px solid %4;"
        " padding: 8px 6px;"
        " font: 600 9pt 'Segoe UI';"
        "}"
        "QPushButton {"
        " background: %10;"
        " border: 1px solid %4;"
        " border-radius: 7px;"
        " color: %7;"
        " padding: 3px 7px;"
        " font: 600 8pt 'Segoe UI';"
        "}"
        "QPushButton:hover { border-color: %9; }"
        "QPushButton:pressed { background-color: %6; }"
        "QScrollArea#overviewScroll { border: none; background: transparent; }"
        "QScrollArea#settingsScrollArea { border: none; background: transparent; }"
        "QScrollArea#tabScrollArea { border: none; background: transparent; }"
        "QWidget#overviewContainer, QWidget#workspacePage { background: transparent; }"
        "QProgressBar {"
        " background-color: %18;"
        " color: %7;"
        " border-radius: 10px;"
        " border: 1px solid %17;"
        " min-height: 18px;"
        " text-align: center;"
        " padding: 2px;"
        "}"
        "QProgressBar#progressBar {"
        " min-height: 16px;"
        " max-height: 16px;"
        " border-radius: 8px;"
        " font: 600 7.6pt 'Segoe UI';"
        "}"
        "QProgressBar::chunk { background-color: %9; border-radius: 8px; }"
        "QSplitter::handle { background: transparent; }"
        "QSplitter#workspaceSplitter::handle:vertical { height: 0px; }"
        "QSplitter#workspaceSplitter::handle:horizontal {"
        " width: 6px;"
        " margin: 10px 2px;"
        " border-radius: 3px;"
        " background: %6;"
        "}"
        "QSplitter#workspaceSplitter::handle:horizontal:hover { background: %9; }")
        .arg(shellBgStart.name(),
             shellBgEnd.name(),
             sideRailGradient,
             borderColor.name(),
             mutedTextColor.name(),
             hoverColor,
             strongTextColor.name(),
             tabSelectedGradient,
             accentAltColor.name(),
             utilityGradient,
             accentText.name(),
             primaryGradient,
             primaryHoverGradient,
             heroStartColor.name(),
             heroEndColor.name(),
             inputBgColor.name(),
             inputBorderColor.name(),
             progressBgColor.name());

    const QString buttonStyle = QString(
        "QPushButton {"
        " background: %1;"
        " border: 1px solid %2;"
        " border-radius: 7px;"
        " color: %3;"
        " padding: 3px 7px;"
        " font: 600 7.9pt 'Segoe UI';"
        "}"
        "QPushButton:hover { background: %4; border-color: %5; }"
        "QPushButton:pressed { background: %4; }"
        "QPushButton:disabled { color: %6; border-color: %7; background: %8; }"
        "QPushButton#postButton {"
        " background: %9;"
        " border: 1px solid %5;"
        " color: %10;"
        " padding: 4px 8px;"
        " font: 700 8.2pt 'Segoe UI';"
        "}"
        "QPushButton#postButton:hover { background: %11; border-color: %5; }"
        "QPushButton#monitorButton, QPushButton#saveButton {"
        " background: %12;"
        " border: 1px solid %13;"
        " color: %10;"
        " padding: 4px 8px;"
        " font: 700 8.2pt 'Segoe UI';"
        "}"
        "QPushButton#monitorButton:hover, QPushButton#saveButton:hover { background: %14; border-color: %13; }"
        "QPushButton#selectFilesButton, QPushButton#selectFolderButton, QPushButton#scanAutoDirButton,"
        "QPushButton#compressPathButton, QPushButton#autoDirButton, QPushButton#nzbFileButton,"
        "QPushButton#nzbPathButton, QPushButton#addServerButton, QPushButton#themeButton,"
        "QPushButton#pauseButton, QPushButton#addMonitoringFolderButton {"
        " background: %1;"
        " border: 1px solid %5;"
        " padding: 3px 7px;"
        "}"
        "QPushButton#selectFilesButton:hover, QPushButton#selectFolderButton:hover, QPushButton#scanAutoDirButton:hover,"
        "QPushButton#compressPathButton:hover, QPushButton#autoDirButton:hover, QPushButton#nzbFileButton:hover,"
        "QPushButton#nzbPathButton:hover, QPushButton#addServerButton:hover, QPushButton#themeButton:hover,"
        "QPushButton#pauseButton:hover, QPushButton#addMonitoringFolderButton:hover { background: %4; }"
        "QPushButton#pauseButton {"
        " min-width: 28px;"
        " max-width: 28px;"
        " min-height: 28px;"
        " max-height: 28px;"
        " padding: 0px;"
        " border-radius: 14px;"
        "}"
        "QPushButton#themeButton:checked { background: %9; color: %10; border-color: %5; }"
        "QPushButton#clearFilesButton, QPushButton#clearLogButton {"
        " background: %15;"
        " border: 1px solid %16;"
        " color: %10;"
        "}"
        "QPushButton#clearFilesButton:hover, QPushButton#clearLogButton:hover { background: %17; border-color: %16; }"
        "QPushButton#genPass, QPushButton#genCompressName, QPushButton#genPoster {"
        " background: %9;"
        " border: 1px solid %5;"
        " color: %10;"
        " padding: 1px 5px;"
        " min-width: 30px;"
        " min-height: 18px;"
        " border-radius: 6px;"
        " font: 700 7.4pt 'Segoe UI';"
        "}"
        "QPushButton#genPass:hover, QPushButton#genCompressName:hover, QPushButton#genPoster:hover { background: %11; }"
        "QLineEdit:focus, QTextEdit:focus, QTextBrowser:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QTableWidget:focus {"
        " border: 1px solid %5;"
        " background-color: %18;"
        "}"
        "QCheckBox::indicator, QRadioButton::indicator {"
        " width: 16px;"
        " height: 16px;"
        " border-radius: 5px;"
        " border: 1px solid %2;"
        " background: %8;"
        "}"
        "QCheckBox::indicator:checked, QRadioButton::indicator:checked {"
        " background: %9;"
        " border: 1px solid %5;"
        "}"
        "QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: %5; }")
        .arg(utilityGradient,
             borderColor.name(),
             strongTextColor.name(),
             utilityHoverGradient,
             accentAltColor.name(),
             mutedTextColor.name(),
             inputBorderColor.name(),
             cardColor.name(),
             primaryGradient,
             accentText.name(),
             primaryHoverGradient,
             successGradient,
             successAltColor.name(),
             successHoverGradient,
             dangerGradient,
             dangerColor.name(),
             dangerHoverGradient,
             blendColor(inputBgColor, accent, 0.06).name());

    const QString spinBoxStyle = QString(
        "QSpinBox, QDoubleSpinBox {"
        " padding-right: 24px;"
        " min-width: 56px;"
        " min-height: 24px;"
        " font: 600 8.2pt 'Segoe UI';"
        "}"
        "QSpinBox::up-button, QDoubleSpinBox::up-button {"
        " subcontrol-origin: border;"
        " subcontrol-position: top right;"
        " width: 18px;"
        " border-left: 1px solid %1;"
        " border-top-right-radius: 7px;"
        " background: %2;"
        "}"
        "QSpinBox::down-button, QDoubleSpinBox::down-button {"
        " subcontrol-origin: border;"
        " subcontrol-position: bottom right;"
        " width: 18px;"
        " border-left: 1px solid %1;"
        " border-top: 1px solid %3;"
        " border-bottom-right-radius: 7px;"
        " background: %2;"
        "}"
        "QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,"
        "QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {"
        " background: %4;"
        "}"
        "QSpinBox::up-button:disabled, QDoubleSpinBox::up-button:disabled,"
        "QSpinBox::down-button:disabled, QDoubleSpinBox::down-button:disabled {"
        " background: %5;"
        " border-left-color: %6;"
        "}"
        "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {"
        " image: url(:/icons/spin_plus.svg);"
        " width: 10px;"
        " height: 10px;"
        "}"
        "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {"
        " image: url(:/icons/spin_minus.svg);"
        " width: 10px;"
        " height: 10px;"
        "}")
        .arg(accentAltColor.name(),
             primaryGradient,
             blendColor(accentAltColor, borderColor, 0.35).name(),
             primaryHoverGradient,
             blendColor(cardColor, borderColor, 0.22).name(),
             inputBorderColor.name());

    appStyle += "\n";
    appStyle += scaledStyleText(shellStyle, _uiScale);
    appStyle += "\n";
    appStyle += scaledStyleText(buttonStyle, _uiScale);
    appStyle += "\n";
    appStyle += scaledStyleText(spinBoxStyle, _uiScale);
    qApp->setStyleSheet(appStyle);

    QTabBar *bar = _ui->postTabWidget->tabBar();
    const QColor defaultText = qApp->palette().color(QPalette::WindowText);
    for (int i = 0; i < bar->count(); ++i)
    {
        QColor current = bar->tabTextColor(i);
        if (!current.isValid() || current == Qt::black)
            bar->setTabTextColor(i, defaultText);
    }

    updateThemeColorButton();
    _refreshShellText();
    if (_helpBrowser)
        _helpBrowser->setHtml(_helpHtml());
}

QColor MainWindow::currentAccentColor() const
{
    return QColor(10, 102, 194);
}

void MainWindow::updateThemeColorButton()
{
    _ui->themeColorLbl->hide();
    _ui->themeColorButton->hide();
    _ui->themeColorResetButton->hide();
}

const QString MainWindow::sGroupBoxStyle =  "\
        QGroupBox {\
        font: bold; \
        border: 1px solid silver;\
        border-radius: 6px;\
        margin-top: 6px;\
        }\
        QGroupBox::title {\
        subcontrol-origin:  margin;\
        left: 7px;\
        padding: 0 5px 0 5px;\
        }";

const QString MainWindow::sGroupBoxStyleDark =  "\
        QGroupBox {\
        font: bold; \
        border: 1px solid #555555;\
        border-radius: 6px;\
        margin-top: 6px;\
        }\
        QGroupBox::title {\
        subcontrol-origin:  margin;\
        left: 7px;\
        padding: 0 5px 0 5px;\
        color: #e0e0e0;\
        }";

// from https://doc.qt.io/qt-5/stylesheet-examples.html#customizing-qtabwidget-and-qtabbar
const QString MainWindow::sTabWidgetStyle = "\
        QTabWidget::pane { /* The tab widget frame */\
            border-top: 2px solid #C2C7CB;\
        }\
        \
        QTabWidget::tab-bar {\
            left: 5px; /* move to the right by 5px */\
        }\
        \
        /* Style the tab using the tab sub-control. Note that\
            it reads QTabBar _not_ QTabWidget */ \
        QTabBar::tab { \
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,\
                                        stop: 0 #E1E1E1, stop: 0.4 #DDDDDD,\
                                        stop: 0.5 #D8D8D8, stop: 1.0 #D3D3D3);\
            border: 2px solid #C4C4C3;\
            border-bottom-color: #C2C7CB; /* same as the pane color */\
            border-top-left-radius: 4px;\
            border-top-right-radius: 4px;\
            min-width: 8ex;\
            padding: 2px;\
        }\
        \
        QTabBar::tab:selected, QTabBar::tab:hover {\
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,\
                                        stop: 0 #fafafa, stop: 0.4 #f4f4f4,\
                                        stop: 0.5 #e7e7e7, stop: 1.0 #fafafa);\
        }\
        \
        QTabBar::tab:selected {\
            border-color: #9B9B9B;\
            border-bottom-color: #C2C7CB; /* same as pane color */\
        }\
        \
        QTabBar::tab:!selected {\
            margin-top: 2px; /* make non-selected tabs look smaller */\
        }\
        \
        /* make use of negative margins for overlapping tabs */\
        QTabBar::tab:selected {\
            /* expand/overlap to the left and right by 4px */\
            margin-left: -4px;\
            margin-right: -4px;\
        }\
        \
        QTabBar::tab:first:selected {\
            margin-left: 0; /* the first selected tab has nothing to overlap with on the left */\
        }\
        \
        QTabBar::tab:last:selected {\
            margin-right: 0; /* the last selected tab has nothing to overlap with on the right */\
        }\
        \
        QTabBar::tab:only-one {\
            margin: 0; /* if there is only one tab, we don't want overlapping margins */\
        }\
        \
        QTabBar::scroller { /* the width of the scroll buttons */\
            width: 40px;\
        }\
        QTabBar QToolButton::right-arrow { /* the arrow mark in the tool buttons */\
            image: url(:/icons/right.png);\
        }\
        \
        QTabBar QToolButton::left-arrow {\
            image: url(:/icons/left.png);\
        }\
";

const QString MainWindow::sTabWidgetStyleDark = "\
        QTabWidget::pane { /* The tab widget frame */\
            border-top: 2px solid #3a3a3a;\
        }\
        \
        QTabWidget::tab-bar {\
            left: 5px; /* move to the right by 5px */\
        }\
        \
        /* Style the tab using the tab sub-control. Note that\
            it reads QTabBar _not_ QTabWidget */ \
        QTabBar::tab { \
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,\
                                        stop: 0 #3a3a3a, stop: 0.4 #333333,\
                                        stop: 0.5 #2f2f2f, stop: 1.0 #2a2a2a);\
            border: 2px solid #444444;\
            border-bottom-color: #3a3a3a; /* same as the pane color */\
            border-top-left-radius: 4px;\
            border-top-right-radius: 4px;\
            min-width: 8ex;\
            padding: 2px;\
            color: #e0e0e0;\
        }\
        \
        QTabBar::tab:selected, QTabBar::tab:hover {\
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,\
                                        stop: 0 #4a4a4a, stop: 0.4 #3f3f3f,\
                                        stop: 0.5 #3a3a3a, stop: 1.0 #4a4a4a);\
        }\
        \
        QTabBar::tab:selected {\
            border-color: #666666;\
            border-bottom-color: #3a3a3a; /* same as pane color */\
        }\
        \
        QTabBar::tab:!selected {\
            margin-top: 2px; /* make non-selected tabs look smaller */\
        }\
        \
        /* make use of negative margins for overlapping tabs */\
        QTabBar::tab:selected {\
            /* expand/overlap to the left and right by 4px */\
            margin-left: -4px;\
            margin-right: -4px;\
        }\
        \
        QTabBar::tab:first:selected {\
            margin-left: 0; /* the first selected tab has nothing to overlap with on the left */\
        }\
        \
        QTabBar::tab:last:selected {\
            margin-right: 0; /* the last selected tab has nothing to overlap with on the right */\
        }\
        \
        QTabBar::tab:only-one {\
            margin: 0; /* if there is only one tab, we don't want overlapping margins */\
        }\
        \
        QTabBar::scroller { /* the width of the scroll buttons */\
            width: 40px;\
        }\
        \
        QTabBar QToolButton::right-arrow { /* the arrow mark in the tool buttons */\
            image: url(:/icons/right.png);\
        }\
        \
        QTabBar QToolButton::left-arrow {\
            image: url(:/icons/left.png);\
        }\
";
