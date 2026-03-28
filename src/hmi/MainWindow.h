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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileInfoList>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QSize>
class NgPost;
class QAction;
class QActionGroup;
class QDialog;
class QDockWidget;
class QFrame;
class QLabel;
class QMenu;
class QPushButton;
class QResizeEvent;
class QShowEvent;
class QStackedWidget;
class QTextBrowser;
struct NntpServerParams;
class NntpFile;
class PostingWidget;
class AutoPostWidget;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    enum class STATE {IDLE, POSTING, STOPPING};
    enum class ShellView { Overview, QuickPost, AutoPosting, Activity };

    Ui::MainWindow *_ui;
    NgPost         *_ngPost;
    STATE           _state;
    PostingWidget  *_quickJobTab;
    AutoPostWidget *_autoPostTab;
    ShellView       _shellView;
    enum class Theme { Light, Dark };
    Theme _theme;
    QPalette _defaultPalette;
    QColor _defaultHighlightColor;
    QColor _accentColor;
    QString _defaultStyleName;
    QString _defaultAppStyleSheet;
    QFont _defaultAppFont;
    QSize _baseWindowSize;
    double _uiScale;
    QDialog *_serversDialog;
    QDialog *_preferencesDialog;
    QDialog *_helpDialog;
    QTextBrowser *_helpBrowser;
    QMenu *_settingsMenu;
    QMenu *_viewMenu;
    QMenu *_scaleMenu;
    QAction *_openServersAction;
    QAction *_openPreferencesAction;
    QAction *_saveConfigAction;
    QAction *_toggleFullscreenAction;
    QAction *_aboutMenuAction;
    QAction *_helpMenuAction;
    QActionGroup *_scaleActionGroup;
    QWidget *_shellRoot;
    QStackedWidget *_contentStack;
    QWidget *_overviewPage;
    QWidget *_workspacePage;
    QDockWidget *_activityDock;
    QFrame *_overviewStatsFrame;
    QFrame *_workspaceHeaderFrame;
    QLabel *_sidebarTaglineLabel;
    QLabel *_sidebarVersionLabel;
    QLabel *_sidebarFooterLabel;
    QPushButton *_overviewNavButton;
    QPushButton *_quickNavButton;
    QPushButton *_autoNavButton;
    QPushButton *_activityNavButton;
    QPushButton *_sideServersButton;
    QPushButton *_sidePreferencesButton;
    QPushButton *_sideSaveButton;
    QPushButton *_sideFullscreenButton;
    QPushButton *_sideHelpButton;
    QLabel *_overviewEyebrowLabel;
    QLabel *_overviewTitleLabel;
    QLabel *_overviewSubtitleLabel;
    QPushButton *_quickActionButton;
    QPushButton *_autoActionButton;
    QPushButton *_serversActionButton;
    QPushButton *_preferencesActionButton;
    QLabel *_sessionCardTitleLabel;
    QLabel *_sessionCardValueLabel;
    QLabel *_serversCardTitleLabel;
    QLabel *_serversCardValueLabel;
    QLabel *_themeCardTitleLabel;
    QLabel *_themeCardValueLabel;
    QLabel *_archiverCardTitleLabel;
    QLabel *_archiverCardValueLabel;
    QLabel *_overviewGuideTitleLabel;
    QLabel *_overviewGuideLabel;
    QLabel *_workspaceTitleLabel;
    QLabel *_workspaceSubtitleLabel;

    static const bool sDefaultServerSSL   = true;
    static const int  sDefaultConnections = 5;
    static const int  sDefaultServerPort  = 563;
    static const int  sDeleteColumnWidth  = 30;

    static const QList<const char *> sServerListHeaders;
    static const QVector<int> sServerListSizes;



public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void init(NgPost *ngPost);

    void updateProgressBar(uint nbArticlesTotal, uint nbArticlesUploaded, const QString &avgSpeed = "0 B/s"
                       #ifdef __COMPUTE_IMMEDIATE_SPEED__
                           , const QString &immediateSpeed = "0 B/s"
                       #endif
                           );

    void updateServers();
    void updateParams();
    void updateAutoPostingParams();

    QString fixedArchivePassword() const;

    PostingWidget *addNewQuickTab(int lastTabIdx, const QFileInfoList &files = QFileInfoList());

    void setTab(QWidget *postWidget);
    void clearJobTab(QWidget *postWidget);
    void updateJobTab(QWidget *postWidget, const QColor &color, const QIcon &icon, const QString &tooltip = "");

    void setJobLabel(int jobNumber);

    void log(const QString &aMsg, bool newline = true) const; //!< log function for QString
    void logError(const QString &error) const; //!< log function for QString

    bool useFixedPassword() const;
    bool hasAutoCompress() const;


    bool hasFinishedPosts() const;

    inline AutoPostWidget *autoWidget() const;
    void closeTab(PostingWidget *postWidget);

    void setPauseIcon(bool pause);

    static const QColor  sPostingColor;
    static const QString sPostingIcon;
    static const QColor  sPendingColor;
    static const QString sPendingIcon;
    static const QColor  sDoneOKColor;
    static const QString sDoneOKIcon;
    static const QColor  sDoneKOColor;
    static const QString sDoneKOIcon;
    static const QColor  sArticlesFailedColor;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

public slots:
    void onSetProgressBarRange(int nbArticles);


private slots:
    void onAddServer();
    void onDelServer();

    void onObfucateToggled(bool checked);

    void onTabContextMenu(const QPoint &point);
    void onCloseAllFinishedQuickTabs();



    void onGenPoster();
    void onUniqueFromToggled(bool checked);
    void onRarPassToggled(bool checked);
    void onRarPassUpdated(const QString &fixedPass);
    void onArchivePass();
    void onAutoCompressToggled(bool checked);

    void onDebugToggled(bool checked);
    void onDebugValue(int value);

    void onSaveConfig();

    void onJobTabClicked(int index);
    void onCloseJob(int index);

    void toBeImplemented();

    void onNzbPathClicked();

    void onLangChanged(const QString &lang);

    void onShutdownToggled(bool checked);

    void onPauseClicked();

    void onThemeToggled(bool checked);
    void onThemeColorClicked();
    void onThemeColorResetClicked();
    void onOpenServersDialog();
    void onOpenPreferencesDialog();
    void onOpenAboutDialog();
    void onOpenHelpDialog();
    void onToggleFullscreen();

private:
    void _initServerBox();
    void _initPostingBox();
    void _rebuildPreferencesLayout();
    void _initSettingsDialogs();
    void _initHelpDialog();
    void _initSettingsMenu();
    void _buildModernShell();
    void _showDialog(QDialog *dialog);
    void _fitDialogToContent(QDialog *dialog) const;
    QWidget *_createScrollableTabPage(QWidget *contentWidget) const;
    QWidget *_tabPageContent(int tabIndex) const;
    QString _helpHtml() const;
    QString _overviewGuideHtml() const;
    void _refreshShellText();
    void _refreshQuickTabLabels();
    void _refreshWorkspaceTabBar();
    void _refreshShellSummary();
    void _refreshWindowModeUi();
    void _refreshScaleMenu();
    void _updateSettingsContentHints();
    void _updateWorkspaceHeader();
    void _setShellView(ShellView view);
    void _applyActivityPaneLayout();
    void _applyUiScale(double scale, bool persist);

    void _addServer(NntpServerParams *serverParam);
    int  _serverRow(QObject *delButton);
    PostingWidget *_getPostWidget(int tabIndex) const;
    int _getPostWidgetIndex(PostingWidget *postWidget) const;
    void _syncFixedPasswordWidgets(const QString &previousPass, const QString &pass);

    void applyTheme(Theme theme);
    QColor currentAccentColor() const;
    void updateThemeColorButton();

    static const QString sGroupBoxStyle;
    static const QString sTabWidgetStyle;
    static const QString sGroupBoxStyleDark;
    static const QString sTabWidgetStyleDark;

};

AutoPostWidget *MainWindow::autoWidget() const { return _autoPostTab; }

#endif // MAINWINDOW_H
