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

#include "AutoPostWidget.h"
#include "ui_AutoPostWidget.h"
#include "PostingWidget.h"
#include "MainWindow.h"
#include "NgPost.h"
#include "FoldersMonitorForNewFiles.h"
#include <QFileDialog>
#include <QDebug>
#include <QFrame>
#include <QGridLayout>
#include <QLayout>
#include <QMessageBox>
#include <QKeyEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QMimeData>

namespace {
void clearLayoutItems(QLayout *layout)
{
    if (!layout)
        return;

    while (QLayoutItem *item = layout->takeAt(0))
    {
        if (QLayout *childLayout = item->layout())
        {
            clearLayoutItems(childLayout);
            delete childLayout;
        }
        delete item;
    }
}

void setLayoutWidgetsVisible(QLayout *layout, bool visible)
{
    if (!layout)
        return;

    for (int i = 0; i < layout->count(); ++i)
    {
        if (QLayoutItem *item = layout->itemAt(i))
        {
            if (QWidget *widget = item->widget())
                widget->setVisible(visible);
            else if (QLayout *childLayout = item->layout())
                setLayoutWidgetsVisible(childLayout, visible);
        }
    }
}
}

AutoPostWidget::AutoPostWidget(NgPost *ngPost, MainWindow *hmi) :
    QWidget(hmi),
    _ui(new Ui::AutoPostWidget),
    _hmi(hmi),
    _ngPost(ngPost),
    _isMonitoring(false),
    _currentPostIdx(1),
    _advancedToggleButton(nullptr),
    _advancedVisible(false)
{
    _ui->setupUi(this);
    _ui->filesList->setSignature(QString("<pre>%1</pre>").arg(_ngPost->escapeXML(NgPost::brandedAsciiArt())));
    _ui->aboutButton->setToolTip(tr("About ngPost").replace("ngPost", NgPost::displayName()));
    connect(_ui->filesList, &SignedListWidget::rightClick, this, &AutoPostWidget::onSelectFilesClicked);
}

AutoPostWidget::~AutoPostWidget()
{
    delete _ui;
}

void AutoPostWidget::init()
{
    auto compactButton = [](QWidget *button, int minWidth, int height) {
        if (!button)
            return;
        button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        button->setMinimumHeight(height);
        button->setMaximumHeight(height);
        if (minWidth > 0)
            button->setMinimumWidth(minWidth);
    };

    _ngPost->_detectBundledArchiver();
    _ui->rarMaxCB->setChecked(_ngPost->_useRarMax);

    _ui->keepRarCB->setChecked(_ngPost->_keepRar);

    _ui->compressPathEdit->setText(_ngPost->_tmpPath);
    _ui->compressPathEdit->setMinimumWidth(116);
    const QString archiverDisplay = _ngPost->_rarPath.isEmpty()
            ? tr("Automatic")
            : QFileInfo(_ngPost->_rarPath).fileName();
    const QString archiverToolTip = _ngPost->_rarPath.isEmpty()
            ? tr("The archiver is detected automatically next to ngPost.exe.")
            : tr("Detected automatically: %1").arg(_ngPost->_rarPath);
    _ui->rarLbl->setText(tr("archiver: "));
    _ui->rarLbl->setToolTip(archiverToolTip);
    _ui->rarEdit->setReadOnly(true);
    _ui->rarEdit->setText(archiverDisplay);
    _ui->rarEdit->setToolTip(archiverToolTip);
    _ui->rarEdit->setMinimumWidth(74);
    _ui->rarPathButton->hide();

    _ui->rarSizeEdit->setText(QString::number(_ngPost->_rarSize));
    _ui->rarSizeEdit->setValidator(new QIntValidator(1, 1000000, _ui->rarSizeEdit));

    _ui->redundancySB->setRange(0, 100);
    _ui->redundancySB->setValue(static_cast<int>(_ngPost->_par2Pct));
    _ui->redundancySB->setEnabled(_ngPost->_par2Args.isEmpty());

    _ui->autoDirEdit->setText(_ngPost->_inputDir);
    _ui->autoDirEdit->setMinimumWidth(116);
    _ui->nameLengthSB->setRange(5, 50);
    _ui->nameLengthSB->setValue(static_cast<int>(_ngPost->_lengthName));
    _ui->passLengthSB->setRange(5, 50);
    _ui->passLengthSB->setValue(static_cast<int>(_ngPost->_lengthPass));
    _ui->rarSizeEdit->setMinimumWidth(52);
    _ui->nameLengthSB->setMinimumWidth(52);
    _ui->passLengthSB->setMinimumWidth(52);
    _ui->redundancySB->setMinimumWidth(52);
    _ui->nameLengthSB->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _ui->passLengthSB->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _ui->redundancySB->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _ui->nameLengthSB->setAlignment(Qt::AlignCenter);
    _ui->passLengthSB->setAlignment(Qt::AlignCenter);
    _ui->redundancySB->setAlignment(Qt::AlignCenter);
    _ui->compressPathButton->setText(tr("Browse"));
    _ui->compressPathButton->setIcon(QIcon(":/icons/folder.png"));
    compactButton(_ui->compressPathButton, 66, 22);
    _ui->autoDirButton->setText(tr("Browse"));
    _ui->autoDirButton->setIcon(QIcon(":/icons/folder.png"));
    compactButton(_ui->autoDirButton, 66, 22);
    _ui->addMonitoringFolderButton->setText(tr("Extra folder"));
    compactButton(_ui->addMonitoringFolderButton, 76, 22);
    _ui->scanAutoDirButton->setText(tr("Scan Folder"));
    compactButton(_ui->scanAutoDirButton, 78, 22);
    compactButton(_ui->postButton, 86, 23);
    compactButton(_ui->monitorButton, 90, 23);
    _ui->aboutButton->hide();

    _ui->filesList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(_ui->compressPathButton,&QAbstractButton::clicked, this, &AutoPostWidget::onCompressPathClicked);
    connect(_ui->autoDirButton,     &QAbstractButton::clicked, this, &AutoPostWidget::onSelectAutoDirClicked);
    connect(_ui->scanAutoDirButton, &QAbstractButton::clicked, this, &AutoPostWidget::onScanAutoDirClicked);
    connect(_ui->compressCB,        &QAbstractButton::toggled, this, &AutoPostWidget::onCompressToggled);
    connect(_ui->par2CB,            &QAbstractButton::toggled, this, &AutoPostWidget::onGenPar2Toggled);

    setPackingAuto(_ngPost->_packAuto, _ngPost->_packAutoKeywords);
    if (_ngPost->_keepRar)
        _ui->keepRarCB->setChecked(true);
    _ui->startJobsCB->setChecked(true);
    onGenPar2Toggled(_ui->par2CB->isChecked());

    _ui->latestFilesFirstCB->setChecked(true);

    connect(_ui->postButton,   &QAbstractButton::clicked, this,    &AutoPostWidget::onGenQuickPosts);
    connect(_ui->aboutButton,  &QAbstractButton::clicked, _ngPost, &NgPost::onAboutClicked);
    connect(_ui->monitorButton, &QAbstractButton::clicked, this, &AutoPostWidget::onMonitoringClicked);

    connect(_ui->delFilesCB, &QAbstractButton::toggled, this, &AutoPostWidget::onDelFilesToggled);


    _ui->addMonitoringFolderButton->setEnabled(false);
    connect(_ui->addMonitoringFolderButton, &QAbstractButton::clicked, this, &AutoPostWidget::onAddMonitoringFolder);


    _ui->extensionFilterEdit->setText(_ngPost->_monitorExtensions.isEmpty()? "" : _ngPost->_monitorExtensions.join(","));
    _ui->dirAllowedCB->setChecked(!_ngPost->_monitorIgnoreDir);

    if (!_advancedToggleButton)
    {
        _advancedToggleButton = new QPushButton(this);
        _advancedToggleButton->setCheckable(true);
        _advancedToggleButton->setProperty("shellRole", "secondaryAction");
        _advancedToggleButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        _advancedToggleButton->setCursor(Qt::PointingHandCursor);
        connect(_advancedToggleButton, &QAbstractButton::toggled, this, &AutoPostWidget::_setAdvancedSectionVisible);
        _ui->verticalLayout->insertWidget(4, _advancedToggleButton, 0, Qt::AlignLeft);
    }

    _setAdvancedSectionVisible(_ngPost && _ngPost->_autoAdvancedVisible);
}

void AutoPostWidget::applyUiScale(double scale)
{
    const double clamped = qBound(1.0, scale, 1.5);
    auto compactButton = [clamped](QPushButton *button, int minWidth, int height, bool fixedWidth = true) {
        if (!button)
            return;
        button->ensurePolished();
        const int targetWidth = qMax(qRound(minWidth * clamped), button->sizeHint().width() + qRound(12 * clamped));
        const int targetHeight = qRound(height * clamped);
        button->setMinimumWidth(targetWidth);
        if (fixedWidth)
            button->setMaximumWidth(targetWidth);
        else
            button->setMaximumWidth(QWIDGETSIZE_MAX);
        button->setMinimumHeight(targetHeight);
        button->setMaximumHeight(targetHeight);
        button->setSizePolicy(fixedWidth ? QSizePolicy::Fixed : QSizePolicy::Minimum, QSizePolicy::Fixed);
        button->setIconSize(QSize(qRound(13 * clamped), qRound(13 * clamped)));
    };

    _ui->compressPathEdit->setMinimumWidth(qRound(116 * clamped));
    _ui->rarEdit->setMinimumWidth(qRound(74 * clamped));
    _ui->autoDirEdit->setMinimumWidth(qRound(116 * clamped));
    _ui->rarSizeEdit->setMinimumWidth(qRound(52 * clamped));
    _ui->nameLengthSB->setMinimumWidth(qRound(52 * clamped));
    _ui->passLengthSB->setMinimumWidth(qRound(52 * clamped));
    _ui->redundancySB->setMinimumWidth(qRound(52 * clamped));
    compactButton(_ui->compressPathButton, 72, 22);
    compactButton(_ui->autoDirButton, 72, 22);
    compactButton(_ui->addMonitoringFolderButton, 88, 22);
    compactButton(_ui->scanAutoDirButton, 96, 23);
    compactButton(_ui->postButton, 112, 25);
    compactButton(_ui->monitorButton, 108, 25);
    if (_advancedToggleButton)
    {
        _advancedToggleButton->setMinimumHeight(qRound(24 * clamped));
        _advancedToggleButton->setMaximumHeight(qRound(24 * clamped));
        _advancedToggleButton->setIconSize(QSize(qRound(13 * clamped), qRound(13 * clamped)));
        _refreshAdvancedToggleButtonGeometry();
    }
}


void AutoPostWidget::onGenQuickPosts()
{
    bool compress = _ui->compressCB->isChecked();

    QFileInfoList files;
    for (int i = 0 ; i < _ui->filesList->count() ; ++i)
    {
        QFileInfo fileInfo(_ui->filesList->item(i)->text());
        if (fileInfo.exists())
            files << fileInfo;
        if (!compress && fileInfo.isDir())
        {
            _ngPost->error(tr("You can't use auto posting without compression on folders... (%1)").arg(fileInfo.fileName()));
            return ;
        }
    }
    if (files.isEmpty())
    {
        QMessageBox::warning(nullptr,
                             tr("Nothing to post..."),
                             tr("There is nothing to post!\n\
Press the Scan button and remove what you don't want to post ;)\n\
(To remove files, select in the list and press DEL or BackSpace)"));
        return;
    }

    _hmi->updateServers();
    _hmi->updateParams();
    udatePostingParams();

    bool startPost = _ui->startJobsCB->isChecked(),
         useRarMax = _ui->rarMaxCB->isChecked();
    for (const QFileInfo &file : files)
    {
        PostingWidget *quickPostWidget = _hmi->addNewQuickTab(0, {file});
        quickPostWidget->init();
        quickPostWidget->genNameAndPassword(_ngPost->_genName, _ngPost->_genPass, _ngPost->_doPar2, useRarMax);

        if (startPost)
            quickPostWidget->postFiles(false);
    }
}


void AutoPostWidget::onCompressPathClicked()
{
    QString path = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ui->compressPathEdit->text(),
                QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
        _ui->compressPathEdit->setText(path);
}

void AutoPostWidget::onRarPathClicked()
{
    QString path = QFileDialog::getOpenFileName(
                this,
                tr("Select rar executable"),
                QFileInfo(_ngPost->_rarPath).absolutePath()
                );

    if (!path.isEmpty())
    {
        QFileInfo fi(path);
        if (fi.isFile() && fi.isExecutable())
            _ui->rarEdit->setText(path);
        else
            _ngPost->error(tr("the selected file is not executable..."));
    }
}

void AutoPostWidget::onSelectAutoDirClicked()
{
    QString path = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ui->autoDirEdit->text(),
                QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
        _ui->autoDirEdit->setText(path);
}

void AutoPostWidget::onScanAutoDirClicked()
{
    QDir autoDir(_ui->autoDirEdit->text());
    if (autoDir.exists())
    {
        _ui->filesList->clear2();
        QDir::SortFlags sort = _ui->latestFilesFirstCB->isChecked() ? QDir::Time : QDir::Name;
        for (const QFileInfo &file : autoDir.entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot, sort))
            _ui->filesList->addPathIfNotInList(file.absoluteFilePath(), 0, file.isDir());
    }
    else
        QMessageBox::warning(nullptr,
                             tr("No auto directory selected..."),
                             tr("There is no auto directory!\nPlease select one."));
}

void AutoPostWidget::onMonitoringClicked()
{
    if (_isMonitoring)
    {
        if (_ngPost->hasMonitoringPostingJobs())
        {
            int res = QMessageBox::question(this,
                                  tr("Ongoing Monitoring post"),
                                  tr("There are still ongoing or pending Monitoring Posts.\n We're going to stop all of them.\nAre you sure you want to proceed?")
                                  );
            if (res == QMessageBox::No)
                return;
            else
                _ngPost->closeAllMonitoringJobs();
        }
        _ui->filesList->clear2();
        _ngPost->_stopMonitoring();
        _ui->monitorButton->setText(tr("Monitor Folder"));
    }
    else
    {
        _currentPostIdx = 1;
        _ui->filesList->clear2();
        QString folderPath = _ui->autoDirEdit->text();
        QFileInfo fi(folderPath);
        if (!fi.exists() || !fi.isDir() || !fi.isReadable())
        {
            QMessageBox::warning(this, tr("Error accessing Auto Dir..."), tr("The auto directory must exist and be readable..."));
            return;
        }
        if (!_ui->compressCB->isChecked()) {
            QMessageBox::warning(this,
                                 tr("To be implemented..."),
                                 tr("You can't monitor a folder without compression using the GUI...\nIt's possible in command line if MONITOR_IGNORE_DIR is enabled in your configuration file."));
            return;
        }
        _ui->filesList->addItem(new QListWidgetItem(QIcon(":/icons/monitor.png"), tr("Monitoring %1").arg(folderPath)));
        _ngPost->_startMonitoring(folderPath);
        _ui->monitorButton->setText(tr("Stop Monitoring"));
    }

    _ui->compressPathEdit->setEnabled(_isMonitoring);
    _ui->compressPathButton->setEnabled(_isMonitoring);
    _ui->rarEdit->setEnabled(_isMonitoring);
    _ui->rarPathButton->setEnabled(_isMonitoring);
    _ui->rarSizeEdit->setEnabled(_isMonitoring);
    _ui->rarMaxCB->setEnabled(_isMonitoring);
    _ui->redundancySB->setEnabled(_isMonitoring);

    _ui->autoDirEdit->setEnabled(_isMonitoring);
    _ui->autoDirButton->setEnabled(_isMonitoring);
    _ui->latestFilesFirstCB->setEnabled(_isMonitoring);
    _ui->scanAutoDirButton->setEnabled(_isMonitoring);

    _ui->genNameCB->setEnabled(_isMonitoring);
    _ui->nameLengthSB->setEnabled(_isMonitoring);
    _ui->genPassCB->setEnabled(_isMonitoring);
    _ui->passLengthSB->setEnabled(_isMonitoring);

    _ui->par2CB->setEnabled(_isMonitoring);
//    _ui->delFilesCB->setEnabled(_isMonitoring);

    _ui->startJobsCB->setEnabled(_isMonitoring);
    _ui->postButton->setEnabled(_isMonitoring);

    _isMonitoring = !_isMonitoring;
    _ui->addMonitoringFolderButton->setEnabled(_isMonitoring);

    if (_isMonitoring)
        _ui->filesList->setSelectionMode(QAbstractItemView::NoSelection);
    else
        _ui->filesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void AutoPostWidget::newFileToProcess(const QFileInfo &fileInfo)
{
    QListWidgetItem *newItem = new QListWidgetItem(
                QIcon(fileInfo.isDir()?":/icons/folder.png":":/icons/file.png"),
                QString("- %1").arg(fileInfo.absoluteFilePath()));
    newItem->setForeground(_hmi->sPendingColor);
    _ui->filesList->addItem(newItem);
}

void AutoPostWidget::onDelFilesToggled(bool checked)
{
    if (checked)
        QMessageBox::warning(this,
                         tr("Deleting files/folders once posted"),
                         tr("You're about to delete files from your computer once they've been posted!\nUse it at your own risk!\nIt will be irreversible..."));
    _ngPost->setDelFilesAfterPosted(checked);
}

void AutoPostWidget::onAddMonitoringFolder()
{
    QString path = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Monitoring Folder to add"),
                _ui->autoDirEdit->text(),
                QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
    {
        QFileInfo newDir(path);
        if (newDir.exists() && newDir.isDir() && newDir.isReadable())
        {
            _ui->filesList->addItem(new QListWidgetItem(QIcon(":/icons/monitor.png"), tr("Monitoring %1").arg(newDir.absoluteFilePath())));
            _ngPost->addMonitoringFolder(newDir.absoluteFilePath());
        }
    }
}

void AutoPostWidget::onCompressToggled(bool checked)
{
    _ui->genNameCB->setEnabled(checked);
    _ui->nameLengthSB->setEnabled(checked);
    _ui->genPassCB->setEnabled(checked);
    _ui->passLengthSB->setEnabled(checked);
    _ui->keepRarCB->setEnabled(checked);
    if (!checked && !_ui->par2CB->isChecked())
        _ui->par2CB->setChecked(true);
}

void AutoPostWidget::onGenPar2Toggled(bool checked)
{
    if (!checked && !_ui->compressCB->isChecked())
        _ui->compressCB->setChecked(true);
    const bool usePercentage = checked && _ngPost && _ngPost->_par2Args.isEmpty();
    _ui->redundancyLbl->setEnabled(checked);
    _ui->redundancySB->setEnabled(usePercentage);
    if (checked && usePercentage && _ui->redundancySB->value() == 0)
        _ui->redundancySB->setValue(_ngPost && _ngPost->_par2Pct > 0 ? static_cast<int>(_ngPost->_par2Pct) : 10);
}

#include "PostingJob.h"
void AutoPostWidget::onMonitorJobStart()
{
    PostingJob *job = static_cast<PostingJob*>(sender());

    QString srcPath = QString("- %1").arg(job->getFirstOriginalFile());
    int nbFiles = _ui->filesList->count();
    for (int i = _currentPostIdx; i < nbFiles; ++i)
    {
        QListWidgetItem *item = _ui->filesList->item(i);
        if (item->text() == srcPath)
        {
            item->setForeground(_hmi->sPostingColor);
            _currentPostIdx = i;
            break;
        }
    }
}

void AutoPostWidget::onSelectFilesClicked()
{
    if (!_isMonitoring)
    {
        QStringList files = QFileDialog::getOpenFileNames(
                    this,
                    tr("Select one or more files"),
                    _ngPost->_inputDir);

        int currentNbFiles = _ui->filesList->count();
        for (const QString &file : files)
            _ui->filesList->addPathIfNotInList(file, currentNbFiles);
    }
}

void AutoPostWidget::udatePostingParams()
{
    // fetch compression settings
    _ngPost->_doCompress = _ui->compressCB->isChecked();
    _ngPost->_genName    = _ngPost->_doCompress ? _ui->genNameCB->isChecked() : false;
    _ngPost->_genPass    = _ngPost->_doCompress ? _ui->genPassCB->isChecked() : false;
    _ngPost->_doPar2     = _ui->par2CB->isChecked();

    _ngPost->_tmpPath    = _ui->compressPathEdit->text();
    _ngPost->_detectBundledArchiver();
    _ngPost->_lengthName = static_cast<uint>(_ui->nameLengthSB->value());
    _ngPost->_lengthPass = static_cast<uint>(_ui->passLengthSB->value());
    uint val = 0;
    bool ok  = true;
    _ngPost->_rarSize = 0;
    if (!_ui->rarSizeEdit->text().isEmpty())
    {
        val = _ui->rarSizeEdit->text().toUInt(&ok);
        if (ok)
            _ngPost->_rarSize = val;
    }

    // fetch par2 settings
    if (_ngPost->_par2Args.isEmpty())
        _ngPost->_par2Pct = static_cast<uint>(_ui->redundancySB->value());

    QFileInfo inputDir(_ui->autoDirEdit->text());
    if (inputDir.exists() && inputDir.isDir() && inputDir.isWritable())
        _ngPost->_inputDir = inputDir.absoluteFilePath();


    _ngPost->_monitorExtensions.clear();
    if (!_ui->extensionFilterEdit->text().isEmpty())
    {
        for (QString &extension : _ui->extensionFilterEdit->text().split(","))
        {
            if (!extension.trimmed().isEmpty())
                _ngPost->_monitorExtensions << extension.trimmed();
        }
    }

    _ngPost->_monitorIgnoreDir = !_ui->dirAllowedCB->isChecked();

    _ngPost->_keepRar = _ui->keepRarCB->isChecked();
}

void AutoPostWidget::updateFinishedJob(const QString &path, uint nbArticles, uint nbUploaded, uint nbFailed)
{
    QString srcPath = QString("- %1").arg(path);
    int nbFiles = _ui->filesList->count();
    for (int i = 1; i < nbFiles; ++i)
    {
        QListWidgetItem *item = _ui->filesList->item(i);
        if (item->text() == srcPath)
        {
            QColor color(_hmi->sDoneOKColor);
            if (nbUploaded > 0 && nbFailed == 0)
                item->setText(QString("%1 [%2 ok]").arg(srcPath).arg(nbArticles));
            else
            {
                item->setText(QString("%1 [%2 err / %3]").arg(srcPath).arg(nbFailed).arg(nbArticles));
                if (nbFailed == nbArticles)
                    color = _hmi->sDoneKOColor;
                else
                    color = _hmi->sArticlesFailedColor;
            }
            item->setForeground(color);
            break;
        }
    }
}

bool AutoPostWidget::deleteFilesOncePosted() const { return _ui->delFilesCB->isChecked(); }

void AutoPostWidget::retranslate()
{
    _ui->retranslateUi(this);
    const QString archiverToolTip = _ngPost->_rarPath.isEmpty()
            ? tr("The archiver is detected automatically next to ngPost.exe.")
            : tr("Detected automatically: %1").arg(_ngPost->_rarPath);
    _ui->aboutButton->setToolTip(tr("About ngPost").replace("ngPost", NgPost::displayName()));
    _ui->rarLbl->setText(tr("archiver: "));
    _ui->rarLbl->setToolTip(archiverToolTip);
    _ui->rarEdit->setToolTip(archiverToolTip);
    _ui->compressPathButton->setText(tr("Browse"));
    _ui->autoDirButton->setText(tr("Browse"));
    _ui->addMonitoringFolderButton->setText(tr("Extra folder"));
    _ui->scanAutoDirButton->setText(tr("Scan Folder"));
    _ui->aboutButton->hide();
    _ui->rarMaxCB->setToolTip(tr("limit the number of archive volume to %1 (cf config RAR_MAX)").arg(_ngPost->_rarMax));
    _ui->redundancySB->setToolTip(tr("Using PAR2_ARGS from config file: %1").arg(_ngPost->_par2Args));
    _ui->filesList->setToolTip(QString("%1<br/><br/>%2<ul><li>%3</li><li>%4</li><li>%5</li></ul>%6").arg(
                                   tr("You can use the <b>Monitor Mode</b>")).arg(
                                   tr("or <b>Generate Posts</b> by adding files:")).arg(
                                   tr("Drag & Drop files/folders")).arg(
                                   tr("Right Click to add Files")).arg(
                                   tr("Click on the Scan Button")).arg(
                                   tr("Bare in mind you can select items in the list and press DEL to remove them")));
    if (_advancedToggleButton)
    {
        _advancedToggleButton->setText(_advancedVisible ? tr("Geavanceerd verbergen") : tr("Geavanceerd tonen"));
        _refreshAdvancedToggleButtonGeometry();
    }
}

void AutoPostWidget::setPackingAuto(bool enabled, const QStringList &keys){
    bool compress = false, genName = false, genPass = false, doPar2 = false;
    if (enabled)
    {
        for (auto it = keys.cbegin(), itEnd = keys.cend(); it != itEnd; ++it)
        {
            QString keyWord = (*it).toLower();
            if (keyWord == NgPost::optionName(NgPost::Opt::COMPRESS))
                compress = true;
            else if (keyWord == NgPost::optionName(NgPost::Opt::GEN_NAME))
                genName = true;
            else if (keyWord == NgPost::optionName(NgPost::Opt::GEN_PASS))
                genPass = true;
            else if (keyWord == NgPost::optionName(NgPost::Opt::GEN_PAR2))
                doPar2 = true;
        }
    }
    _ui->compressCB->setChecked(compress);
    _ui->genNameCB->setChecked(genName);
    _ui->genPassCB->setChecked(genPass);
    _ui->par2CB->setChecked(doPar2);
    onCompressToggled(compress);
}

void AutoPostWidget::_rebuildModernLayout()
{
    QVBoxLayout *rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (!rootLayout)
        return;

    clearLayoutItems(rootLayout);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(8);

    auto createSurface = [this]() {
        QFrame *frame = new QFrame(this);
        frame->setAttribute(Qt::WA_StyledBackground, true);
        frame->setProperty("shellSurface", "card");
        return frame;
    };

    QFrame *toolbarFrame = createSurface();
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbarFrame);
    toolbarLayout->setContentsMargins(10, 8, 10, 8);
    toolbarLayout->setSpacing(6);
    toolbarLayout->addWidget(_ui->scanAutoDirButton);
    toolbarLayout->addWidget(_ui->postButton);
    toolbarLayout->addWidget(_ui->monitorButton);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(_ui->startJobsCB);
    toolbarLayout->addWidget(_ui->latestFilesFirstCB);
    rootLayout->addWidget(toolbarFrame);

    QFrame *listFrame = createSurface();
    QVBoxLayout *listLayout = new QVBoxLayout(listFrame);
    listLayout->setContentsMargins(12, 12, 12, 12);
    listLayout->setSpacing(0);
    listLayout->addWidget(_ui->filesList, 1);
    rootLayout->addWidget(listFrame, 1);

    QFrame *detailsFrame = createSurface();
    QVBoxLayout *detailsLayout = new QVBoxLayout(detailsFrame);
    detailsLayout->setContentsMargins(10, 8, 10, 8);
    detailsLayout->setSpacing(8);

    QHBoxLayout *compressPathRow = new QHBoxLayout();
    compressPathRow->setSpacing(6);
    compressPathRow->addWidget(_ui->compressPathLbl);
    compressPathRow->addWidget(_ui->compressPathEdit, 1);
    compressPathRow->addWidget(_ui->compressPathButton);
    detailsLayout->addLayout(compressPathRow);

    QHBoxLayout *archiverRow = new QHBoxLayout();
    archiverRow->setSpacing(6);
    archiverRow->addWidget(_ui->rarLbl);
    archiverRow->addWidget(_ui->rarEdit, 1);
    archiverRow->addWidget(_ui->rarSizeLbl);
    archiverRow->addWidget(_ui->rarSizeEdit);
    archiverRow->addWidget(_ui->rarMaxCB);
    archiverRow->addStretch(1);
    detailsLayout->addLayout(archiverRow);

    QHBoxLayout *autoDirRow = new QHBoxLayout();
    autoDirRow->setSpacing(6);
    autoDirRow->addWidget(_ui->autoDirLbl);
    autoDirRow->addWidget(_ui->autoDirEdit, 1);
    autoDirRow->addWidget(_ui->autoDirButton);
    autoDirRow->addWidget(_ui->addMonitoringFolderButton);
    detailsLayout->addLayout(autoDirRow);

    QHBoxLayout *filterRow = new QHBoxLayout();
    filterRow->setSpacing(6);
    filterRow->addWidget(_ui->extentionFilterLbl);
    filterRow->addWidget(_ui->extensionFilterEdit, 1);
    filterRow->addWidget(_ui->dirAllowedCB);
    detailsLayout->addLayout(filterRow);

    QHBoxLayout *optionsRow = new QHBoxLayout();
    optionsRow->setSpacing(6);
    optionsRow->addWidget(_ui->compressCB);
    optionsRow->addWidget(_ui->genNameCB);
    optionsRow->addWidget(_ui->nameLengthSB);
    optionsRow->addWidget(_ui->genPassCB);
    optionsRow->addWidget(_ui->passLengthSB);
    optionsRow->addStretch(1);
    detailsLayout->addLayout(optionsRow);

    QHBoxLayout *flagsRow = new QHBoxLayout();
    flagsRow->setSpacing(10);
    flagsRow->addWidget(_ui->redundancyLbl);
    flagsRow->addWidget(_ui->redundancySB);
    flagsRow->addWidget(_ui->keepRarCB);
    flagsRow->addWidget(_ui->par2CB);
    flagsRow->addWidget(_ui->delFilesCB);
    flagsRow->addStretch(1);
    detailsLayout->addLayout(flagsRow);

    _ui->compressPathEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _ui->rarEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _ui->autoDirEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _ui->extensionFilterEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rootLayout->addWidget(detailsFrame);

    _ui->aboutButton->hide();
}

void AutoPostWidget::_setAdvancedSectionVisible(bool visible)
{
    const bool shouldPersist = _ngPost && _ngPost->_autoAdvancedVisible != visible;
    if (_ngPost)
        _ngPost->_autoAdvancedVisible = visible;
    _advancedVisible = visible;
    if (_ui->advancedPanel)
    {
        _ui->advancedPanel->setMaximumHeight(visible ? QWIDGETSIZE_MAX : 0);
        _ui->advancedPanel->setMinimumHeight(0);
        _ui->advancedPanel->setSizePolicy(QSizePolicy::Preferred, visible ? QSizePolicy::Preferred : QSizePolicy::Fixed);
        _ui->advancedPanel->setVisible(visible);
        setLayoutWidgetsVisible(_ui->optionsStackLayout, visible);
        _ui->advancedTitleLabel->setVisible(visible);
        _ui->advancedPanel->updateGeometry();
    }

    if (layout())
        layout()->activate();
    adjustSize();
    updateGeometry();

    QWidget *parent = parentWidget();
    while (parent)
    {
        if (QScrollArea *scrollArea = qobject_cast<QScrollArea*>(parent))
        {
            if (visible)
            {
                scrollArea->ensureWidgetVisible(_ui->advancedPanel, 0, 24);
            }
            else
            {
                scrollArea->ensureVisible(0, 0);
            }
            break;
        }
        parent = parent->parentWidget();
    }

    if (_advancedToggleButton)
    {
        const QSignalBlocker blocker(_advancedToggleButton);
        _advancedToggleButton->setChecked(visible);
        _advancedToggleButton->setText(visible ? tr("Geavanceerd verbergen")
                                               : tr("Geavanceerd tonen"));
        _refreshAdvancedToggleButtonGeometry();
    }

    if (shouldPersist)
        _ngPost->saveConfig();
}

void AutoPostWidget::_refreshAdvancedToggleButtonGeometry()
{
    if (!_advancedToggleButton)
        return;

    _advancedToggleButton->ensurePolished();
    const int iconWidth = _advancedToggleButton->icon().isNull() ? 0 : (_advancedToggleButton->iconSize().width() + 6);
    const int textWidth = _advancedToggleButton->fontMetrics().horizontalAdvance(_advancedToggleButton->text());
    const int targetWidth = qMax(_advancedToggleButton->sizeHint().width() + 18,
                                 textWidth + iconWidth + 28);
    _advancedToggleButton->setMinimumWidth(targetWidth);
    _advancedToggleButton->setMaximumWidth(targetWidth);
    _advancedToggleButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    _advancedToggleButton->updateGeometry();
}



void AutoPostWidget::handleKeyEvent(QKeyEvent *keyEvent)
{
    if (!_isMonitoring)
    {
        qDebug() << "[AutoPostWidget::handleKeyEvent] key event: " << keyEvent->key();
        if(keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace)
        {
            for (QListWidgetItem *item : _ui->filesList->selectedItems())
            {
                qDebug() << "[AutoPostWidget::handleKeyEvent] remove item: " << item->text();
                _ui->filesList->removeItemWidget2(item);
                delete item;
            }
        }
    }
}


void AutoPostWidget::handleDropEvent(QDropEvent *e)
{
    if (!_isMonitoring)
    {
        int currentNbFiles = _ui->filesList->count();
        for (const QUrl &url : e->mimeData()->urls())
        {
            QString fileName = url.toLocalFile();
            _ui->filesList->addPathIfNotInList(fileName, currentNbFiles, QFileInfo(fileName).isDir());
        }
    }
}
