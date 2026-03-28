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

#include "PostingWidget.h"
#include "ui_PostingWidget.h"
#include "MainWindow.h"
#include "AboutNgPost.h"
#include "NgPost.h"
#include "PostingJob.h"
#include "nntp/NntpFile.h"

#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QFrame>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QClipboard>
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


PostingWidget::PostingWidget(NgPost *ngPost, MainWindow *hmi, uint jobNumber) :
    QWidget(hmi),
    _ui(new Ui::PostingWidget),
    _hmi(hmi),
    _ngPost(ngPost),
    _jobNumber(jobNumber),
    _postingJob(nullptr),
    _state(STATE::IDLE),
    _postingFinished(false),
    _advancedToggleButton(nullptr),
    _advancedVisible(false)
{
    _ui->setupUi(this);
    _ui->filesList->setSignature(QString("<pre>%1</pre>").arg(_ngPost->escapeXML(NgPost::brandedAsciiArt())));
    _ui->aboutButton->setToolTip(tr("About ngPost").replace("ngPost", NgPost::displayName()));

    connect(_ui->postButton, &QAbstractButton::clicked, this, &PostingWidget::onPostFiles);
    connect(_ui->nzbPassCB,  &QAbstractButton::toggled, this, &PostingWidget::onNzbPassToggled);
    connect(_ui->genPass,    &QAbstractButton::clicked, this, &PostingWidget::onGenNzbPassword);
    connect(_ui->filesList, &SignedListWidget::rightClick, this, &PostingWidget::onSelectFilesClicked);
}

PostingWidget::~PostingWidget()
{
    delete _ui;
}

void PostingWidget::onFilePosted(QString filePath, uint nbArticles, uint nbFailed)
{
    int nbFiles = _ui->filesList->count();
    for (int i = 0 ; i < nbFiles ; ++i)
    {
        QListWidgetItem *item = _ui->filesList->item(i);
        if (item->text() == filePath)
        {
            QColor color(_hmi->sDoneOKColor);
            if (nbFailed == 0)
                item->setText(QString("%1 [%2 ok]").arg(filePath).arg(nbArticles));
            else
            {
                item->setText(QString("%1 [%2 err / %3]").arg(filePath).arg(nbFailed).arg(nbArticles));
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

void PostingWidget::onArchiveFileNames(QStringList paths)
{
    _ui->filesList->clear();
    for (const QString & path : paths)
        _ui->filesList->addPath(path);
}

void PostingWidget::onArticlesNumber(int nbArticles)
{
    Q_UNUSED(nbArticles);
    _hmi->setJobLabel(static_cast<int>(_jobNumber));
}

void PostingWidget::onPostingJobDone()
{
    // we could arrive here twice: from PostingJob::postingFinished or PostingJob::noMoreConnection
    // This could happen especially when we exceed the number of connections allowed by a provider
    if (!_postingJob)
        return;

    if (_postingJob->nbArticlesTotal() > 0)
    {
        if (_postingJob->nbArticlesFailed() > 0)
            _hmi->updateJobTab(this, _hmi->sDoneKOColor, QIcon(_hmi->sDoneKOIcon));
        else
            _hmi->updateJobTab(this, _hmi->sDoneOKColor, QIcon(_hmi->sDoneOKIcon));
    }
    else
        _hmi->clearJobTab(this);

    disconnect(_postingJob);
    _postingJob = nullptr; //!< we don't own it, NgPost will delete it
    _postingFinished = true;
    setIDLE();
}

void PostingWidget::onPostFiles()
{
    postFiles(true);
}

void PostingWidget::postFiles(bool updateMainParams)
{
    if (_state == STATE::IDLE)
    {
        if (_ui->filesList->count() == 0)
        {
            _hmi->logError(tr("There are no selected files to post..."));
            return;
        }

        QFileInfoList files;
        bool hasFolder = false;
        _buildFilesList(files, hasFolder);
        if (files.isEmpty())
        {
            _hmi->logError(tr("There are no existing files to post..."));
            return;
        }

        if (hasFolder && !_ui->compressCB->isChecked())
        {
            _hmi->logError(tr("You can't post folders without using compression..."));
            return;
        }


        if (updateMainParams)
        {
            _hmi->updateServers();
            _hmi->updateParams();
        }
        udatePostingParams();

        // check if the nzb file name already exist
        QString nzbPath = _ngPost->nzbPath();
        if (!nzbPath.endsWith(".nzb"))
            nzbPath += ".nzb";
        QFileInfo fiNzb(nzbPath);
        if (fiNzb.exists())
        {
            int overwrite = QMessageBox::question(nullptr,
                                                  tr("Overwrite existing nzb file?"),
                                                  tr("The nzb file '%1' already exists.\nWould you like to overwrite it ?").arg(nzbPath),
                                                  QMessageBox::Yes,
                                                  QMessageBox::No);
            if (overwrite == QMessageBox::No)
                return;
        }

        _postingFinished = false;
        _state = STATE::POSTING;
        _postingJob = new PostingJob(_ngPost, nzbPath, files, this,
                                     _ngPost->getPostingGroups(),
                                     _ngPost->from(),
                                     _ngPost->_obfuscateArticles, _ngPost->_obfuscateFileName,
                                     _ngPost->_tmpPath, _ngPost->_rarPath, _ngPost->_rarArgs,
                                     _ngPost->_rarSize, _ngPost->_useRarMax, _ngPost->_par2Pct,
                                     _ngPost->_doCompress, _ngPost->_doPar2,
                                     _ngPost->_rarName, _ngPost->_rarPass,
                                     _ngPost->_keepRar);

        bool hasStarted = _ngPost->startPostingJob(_postingJob);

        QString buttonTxt;
        QColor  tabColor;
        QString tabIcon;
        if (hasStarted)
        {
            buttonTxt = tr("Stop Posting");
            tabColor  = _hmi->sPostingColor;
            tabIcon   = _hmi->sPostingIcon;
        }
        else
        {
            buttonTxt = tr("Cancel Posting");
            tabColor  = _hmi->sPendingColor;
            tabIcon   = _hmi->sPendingIcon;
        }
        _ui->postButton->setText(buttonTxt);
        _hmi->updateJobTab(this, tabColor, QIcon(tabIcon), _postingJob->nzbName());
    }
    else  if (_state == STATE::POSTING)
    {
        _state = STATE::STOPPING;
        emit _postingJob->stopPosting();
    }
}


void PostingWidget::onNzbPassToggled(bool checked)
{
    if (_ngPost)
        _ngPost->_quickPostArchivePassword = checked;
    _ui->nzbPassEdit->setEnabled(checked);
    _ui->passLengthSB->setEnabled(checked);
    _ui->genPass->setEnabled(checked);
}

void PostingWidget::onGenNzbPassword()
{
    _ui->nzbPassEdit->setText(_ngPost->randomPass(static_cast<uint>(_ui->passLengthSB->value())));
}

void PostingWidget::onSelectFilesClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
                this,
                tr("Select one or more files to Post"),
                _ngPost->_inputDir);

    int currentNbFiles = _ui->filesList->count();
    for (const QString &file : files)
        addPath(file, currentNbFiles);
}

void PostingWidget::onSelectFolderClicked()
{
    QString folder = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ngPost->_inputDir,
                QFileDialog::ShowDirsOnly);

    if (!folder.isEmpty())
        addPath(folder, _ui->filesList->count(), true);
}

void PostingWidget::onClearFilesClicked()
{
    _ui->filesList->clear2();
    _ui->nzbFileEdit->clear();
    _ui->compressNameEdit->clear();
    if (_hmi->hasAutoCompress())
    {
        onGenCompressName();
        onGenNzbPassword();
    }
    else
        _ui->compressNameEdit->clear();

    _hmi->clearJobTab(this);
}

void PostingWidget::onCompressCB(bool checked)
{
    _ui->compressNameEdit->setEnabled(checked);
    _ui->nameLengthSB->setEnabled(checked);
    _ui->genCompressName->setEnabled(checked);
    _ui->keepRarCB->setEnabled(checked);
}

void PostingWidget::onPar2Toggled(bool checked)
{
    const bool usePercentage = checked && _ngPost && _ngPost->_par2Args.isEmpty();
    _ui->redundancyLbl->setEnabled(checked);
    _ui->redundancySB->setEnabled(usePercentage);
    if (checked && usePercentage && _ui->redundancySB->value() == 0)
        _ui->redundancySB->setValue(_ngPost && _ngPost->_par2Pct > 0 ? static_cast<int>(_ngPost->_par2Pct) : 10);
}

void PostingWidget::onGenCompressName()
{
    _ui->compressNameEdit->setText(_ngPost->randomPass(static_cast<uint>(_ui->nameLengthSB->value())));
}

void PostingWidget::onCompressPathClicked()
{
    QString path = QFileDialog::getExistingDirectory(
                this,
                tr("Select a Folder"),
                _ui->compressPathEdit->text(),
                QFileDialog::ShowDirsOnly);

    if (!path.isEmpty())
        _ui->compressPathEdit->setText(path);
}

void PostingWidget::onNzbFileClicked()
{
    QString path = QFileDialog::getSaveFileName(
                this,
                tr("Create nzb file"),
                _ngPost->_nzbPath,
                "*.nzb"
                );

    if (!path.isEmpty())
        _ui->nzbFileEdit->setText(path);
}

void PostingWidget::onRarPathClicked()
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

void PostingWidget::handleKeyEvent(QKeyEvent *keyEvent)
{
    qDebug() << "[PostingWidget::handleKeyEvent] key event: " << keyEvent->key();

    if(keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace)
    {
        for (QListWidgetItem *item : _ui->filesList->selectedItems())
        {
            qDebug() << "[PostingWidget::handleKeyEvent] remove item: " << item->text();
            _ui->filesList->removeItemWidget2(item);
            delete item;
        }
    }
    else if (keyEvent->matches(QKeySequence::Paste))
    {
        const QClipboard *clipboard = QApplication::clipboard();
        const QMimeData *mimeData = clipboard->mimeData();
        if (mimeData->hasImage()) {
            qDebug() << "[PostingWidget::handleKeyEvent] try to paste image...";
        } else if (mimeData->hasHtml()) {
            qDebug() << "[PostingWidget::handleKeyEvent] try to paste html: ";
        } else if (mimeData->hasText()) {
            QString txt = mimeData->text();
            qDebug() << "[PostingWidget::handleKeyEvent] paste text: " << txt;
            int currentNbFiles = _ui->filesList->count();
            for (const QString &path : txt.split(QRegularExpression("\n|\r|\r\n")))
            {
                QFileInfo fileInfo(path);
                if (!fileInfo.exists())
                    qDebug() << "[PostingWidget::handleKeyEvent] NOT A FILE: " << path;
                else
                    addPath(path, currentNbFiles, fileInfo.isDir());
//                        else if (fileInfo.isDir())
//                        {
//                            QDir dir(fileInfo.absoluteFilePath());
//                            for (const QFileInfo &subFile : dir.entryInfoList(QDir::Files, QDir::Name))
//                            {
//                                if (subFile.isReadable())
//                                    _addFile(subFile.absoluteFilePath(), currentNbFiles);
//                            }
//                        }
            }

        } else if (mimeData->hasUrls()) {
            qDebug() << "[PostingWidget::handleKeyEvent] paste urls...";

        } else {
            qDebug() << "[PostingWidget::handleKeyEvent] unknown type...";
        }
    }

}


void PostingWidget::handleDropEvent(QDropEvent *e)
{
    int currentNbFiles = _ui->filesList->count();
    for (const QUrl &url : e->mimeData()->urls())
    {
        QString fileName = url.toLocalFile();
        addPath(fileName, currentNbFiles, QFileInfo(fileName).isDir());
    }
}

void PostingWidget::_buildFilesList(QFileInfoList &files, bool &hasFolder)
{
    for (int i = 0 ; i < _ui->filesList->count() ; ++i)
    {
        QFileInfo fileInfo(_ui->filesList->item(i)->text());
        if (fileInfo.exists())
        {
            files << fileInfo;
            if (fileInfo.isDir())
                hasFolder = true;
        }
    }
}

void PostingWidget::init()
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

    _ui->nzbPassCB->setChecked(false);
    onNzbPassToggled(false);

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

    _ui->keepRarCB->setChecked(_ngPost->_keepRar);

    _ui->redundancySB->setRange(0, 100);
    _ui->redundancySB->setValue(static_cast<int>(_ngPost->_par2Pct));
    _ui->redundancySB->setEnabled(_ngPost->_par2Args.isEmpty());

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
    _ui->nzbFileButton->setText(tr("Browse"));
    _ui->nzbFileButton->setIcon(QIcon(":/icons/file.png"));
    compactButton(_ui->nzbFileButton, 66, 22);
    _ui->nzbFileEdit->setMinimumWidth(116);
    _ui->compressNameEdit->setMinimumWidth(88);
    _ui->genPass->setText(tr("Gen."));
    compactButton(_ui->genPass, 32, 20);
    _ui->genCompressName->setText(tr("Gen."));
    compactButton(_ui->genCompressName, 32, 20);
    compactButton(_ui->selectFilesButton, 96, 22);
    compactButton(_ui->selectFolderButton, 88, 22);
    compactButton(_ui->clearFilesButton, 76, 22);
    compactButton(_ui->postButton, 94, 23);
    _ui->aboutButton->hide();

    _ui->filesList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(_ui->selectFilesButton, &QAbstractButton::clicked, this, &PostingWidget::onSelectFilesClicked);
    connect(_ui->selectFolderButton,&QAbstractButton::clicked, this, &PostingWidget::onSelectFolderClicked);
    connect(_ui->clearFilesButton,  &QAbstractButton::clicked, this, &PostingWidget::onClearFilesClicked);
    connect(_ui->filesList,         &SignedListWidget::empty,  this, &PostingWidget::onClearFilesClicked, Qt::QueuedConnection);

    connect(_ui->compressCB,        &QAbstractButton::toggled, this, &PostingWidget::onCompressCB);
    connect(_ui->par2CB,            &QAbstractButton::toggled, this, &PostingWidget::onPar2Toggled);
    connect(_ui->genCompressName,   &QAbstractButton::clicked, this, &PostingWidget::onGenCompressName);


    connect(_ui->compressPathButton,&QAbstractButton::clicked, this, &PostingWidget::onCompressPathClicked);

    connect(_ui->nzbFileButton,     &QAbstractButton::clicked, this, &PostingWidget::onNzbFileClicked);

    connect(_ui->aboutButton,       &QAbstractButton::clicked, _ngPost, &NgPost::onAboutClicked);

    onCompressCB(_ngPost->_doCompress);
    if (_ngPost->_doCompress)
        _ui->compressCB->setChecked(true);
    if (_ngPost->_genName)
        onGenCompressName();
    if (_ngPost->_quickPostArchivePassword)
    {
        _ui->nzbPassCB->setChecked(true);
        if (!_ngPost->_rarPassFixed.isEmpty())
            _ui->nzbPassEdit->setText(_ngPost->_rarPassFixed);
        else if (!_ngPost->_rarPass.isEmpty())
            _ui->nzbPassEdit->setText(_ngPost->_rarPass);
    }
    if (_ngPost->_doPar2)
        _ui->par2CB->setChecked(true);
    if (_ngPost->_keepRar)
        _ui->keepRarCB->setChecked(true);
    onPar2Toggled(_ui->par2CB->isChecked());

    QString fixedPass = _hmi->fixedArchivePassword();
    if (_ngPost->_quickPostArchivePassword && !fixedPass.isEmpty())
        _ui->nzbPassEdit->setText(fixedPass);

    if (!_advancedToggleButton)
    {
        _advancedToggleButton = new QPushButton(this);
        _advancedToggleButton->setCheckable(true);
        _advancedToggleButton->setProperty("shellRole", "secondaryAction");
        _advancedToggleButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        _advancedToggleButton->setCursor(Qt::PointingHandCursor);
        connect(_advancedToggleButton, &QAbstractButton::toggled, this, &PostingWidget::_setAdvancedSectionVisible);
        if (_ui->horizontalLayout_4)
            _ui->horizontalLayout_4->insertWidget(2, _advancedToggleButton);
    }

    _setAdvancedSectionVisible(_ngPost && _ngPost->_quickAdvancedVisible);
}

void PostingWidget::applyUiScale(double scale)
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
    _ui->rarSizeEdit->setMinimumWidth(qRound(52 * clamped));
    _ui->nameLengthSB->setMinimumWidth(qRound(52 * clamped));
    _ui->passLengthSB->setMinimumWidth(qRound(52 * clamped));
    _ui->redundancySB->setMinimumWidth(qRound(52 * clamped));
    _ui->nzbFileEdit->setMinimumWidth(qRound(116 * clamped));
    _ui->compressNameEdit->setMinimumWidth(qRound(88 * clamped));
    compactButton(_ui->compressPathButton, 72, 22);
    compactButton(_ui->nzbFileButton, 72, 22);
    compactButton(_ui->genPass, 48, 24);
    compactButton(_ui->genCompressName, 48, 24);
    compactButton(_ui->selectFilesButton, 112, 24);
    compactButton(_ui->selectFolderButton, 106, 24);
    compactButton(_ui->clearFilesButton, 98, 24);
    compactButton(_ui->postButton, 110, 25);
    if (_advancedToggleButton)
    {
        _advancedToggleButton->setMinimumHeight(qRound(24 * clamped));
        _advancedToggleButton->setMaximumHeight(qRound(24 * clamped));
        _advancedToggleButton->setIconSize(QSize(qRound(13 * clamped), qRound(13 * clamped)));
        _refreshAdvancedToggleButtonGeometry();
    }
}

void PostingWidget::genNameAndPassword(bool genName, bool genPass, bool doPar2, bool useRarMax)
{
    _ui->compressCB->setChecked(_ngPost->_doCompress);
    if (genName)
        onGenCompressName();
    if (genPass && _ngPost->_rarPassFixed.isEmpty())
    {
        _ui->nzbPassCB->setChecked(true);        
        onGenNzbPassword();
    }
    _ui->rarMaxCB->setChecked(useRarMax);

    if (doPar2)
        _ui->par2CB->setChecked(true);

    _ui->keepRarCB->setChecked(_ngPost->_keepRar);
}



void PostingWidget::udatePostingParams()
{
    if (!_ui->nzbFileEdit->text().isEmpty())
    {
        QFileInfo nzb(_ui->nzbFileEdit->text());
        if (!nzb.absolutePath().isEmpty())
            _ngPost->_nzbPath = nzb.absolutePath();
        _ngPost->setNzbName(nzb);
    }

    // fetch compression settings
    _ngPost->_tmpPath    = _ui->compressPathEdit->text();
    _ngPost->_doCompress = _ui->compressCB->isChecked();
    _ngPost->_quickPostArchivePassword = _ui->nzbPassCB->isChecked();
    _ngPost->_detectBundledArchiver();
    _ngPost->_rarName    = _ui->compressNameEdit->text();
    QString effectiveArchivePassword;
    if (_ui->nzbPassCB->isChecked())
        effectiveArchivePassword = _ui->nzbPassEdit->text();
    if (effectiveArchivePassword.isEmpty())
        effectiveArchivePassword = _hmi ? _hmi->fixedArchivePassword() : QString();
    _ngPost->_rarPass = effectiveArchivePassword;
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
    _ngPost->_useRarMax = _ui->rarMaxCB->isChecked();

    // fetch par2 settings
    _ngPost->_doPar2  = _ui->par2CB->isChecked();
    if (_ngPost->_par2Args.isEmpty())
        _ngPost->_par2Pct = static_cast<uint>(_ui->redundancySB->value());

    _ngPost->_keepRar = _ui->keepRarCB->isChecked();
}

void PostingWidget::retranslate()
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
    _ui->nzbFileButton->setText(tr("Browse"));
    _ui->genPass->setText(tr("Gen."));
    _ui->genCompressName->setText(tr("Gen."));
    _ui->aboutButton->hide();
    _ui->rarMaxCB->setToolTip(tr("limit the number of archive volume to %1 (cf config RAR_MAX)").arg(_ngPost->_rarMax));
    _ui->redundancySB->setToolTip(tr("Using PAR2_ARGS from config file: %1").arg(_ngPost->_par2Args));
    _ui->filesList->setToolTip(QString("%1<ul><li>%2</li><li>%3</li><li>%4</li></ul>%5").arg(
                                   tr("You can add files or folder by:")).arg(
                                   tr("Drag & Drop files/folders")).arg(
                                   tr("Right Click to add Files")).arg(
                                   tr("Click on Select Files/Folder buttons")).arg(
                                   tr("Bare in mind you can select items in the list and press DEL to remove them")));
    if (_advancedToggleButton)
    {
        _advancedToggleButton->setText(_advancedVisible ? tr("Geavanceerd verbergen") : tr("Geavanceerd tonen"));
        _refreshAdvancedToggleButtonGeometry();
    }
}

void PostingWidget::setNzbPassword(const QString &previousPass, const QString &pass)
{
    if (!_ui->nzbPassCB->isChecked())
        return;

    const QString currentPass = _ui->nzbPassEdit->text();
    if (pass.isEmpty())
    {
        if (currentPass.isEmpty() || currentPass == previousPass)
            _ui->nzbPassEdit->clear();
        return;
    }

    if (currentPass.isEmpty() || currentPass == previousPass)
        _ui->nzbPassEdit->setText(pass);
}

void PostingWidget::setPackingAuto(bool enabled, const QStringList &keys)
{
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
    const bool useArchivePassword = _ngPost ? _ngPost->_quickPostArchivePassword : _ui->nzbPassCB->isChecked();
    _ui->nzbPassCB->setChecked(useArchivePassword);
    _ui->compressCB->setChecked(compress);
    _ui->par2CB->setChecked(doPar2);


    if (compress)
    {
        if (useArchivePassword && _ui->nzbPassEdit->text().isEmpty())
        {
            if (_hmi->useFixedPassword())
                _ui->nzbPassEdit->setText(_ngPost->_rarPassFixed);
            else if (enabled && genPass)
                onGenNzbPassword();
        }
        if (genName && _ui->compressNameEdit->text().isEmpty())
            onGenCompressName();
    }
}

void PostingWidget::_rebuildModernLayout()
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
    toolbarLayout->addWidget(_ui->selectFilesButton);
    toolbarLayout->addWidget(_ui->selectFolderButton);
    toolbarLayout->addWidget(_ui->clearFilesButton);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(_ui->postButton);
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

    QHBoxLayout *nzbRow = new QHBoxLayout();
    nzbRow->setSpacing(6);
    nzbRow->addWidget(_ui->nzbFileLbl);
    nzbRow->addWidget(_ui->nzbFileEdit, 1);
    nzbRow->addWidget(_ui->nzbFileButton);
    detailsLayout->addLayout(nzbRow);

    QHBoxLayout *passwordRow = new QHBoxLayout();
    passwordRow->setSpacing(6);
    passwordRow->addWidget(_ui->nzbPassCB);
    passwordRow->addWidget(_ui->nzbPassEdit, 1);
    passwordRow->addWidget(_ui->passLengthSB);
    passwordRow->addWidget(_ui->genPass);
    detailsLayout->addLayout(passwordRow);

    QHBoxLayout *packingRow = new QHBoxLayout();
    packingRow->setSpacing(6);
    packingRow->addWidget(_ui->compressCB);
    packingRow->addWidget(_ui->compressNameEdit, 1);
    packingRow->addWidget(_ui->nameLengthSB);
    packingRow->addWidget(_ui->genCompressName);
    detailsLayout->addLayout(packingRow);

    QHBoxLayout *flagsRow = new QHBoxLayout();
    flagsRow->setSpacing(10);
    flagsRow->addWidget(_ui->redundancyLbl);
    flagsRow->addWidget(_ui->redundancySB);
    flagsRow->addWidget(_ui->keepRarCB);
    flagsRow->addWidget(_ui->par2CB);
    flagsRow->addStretch(1);
    detailsLayout->addLayout(flagsRow);

    _ui->nzbFileEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _ui->nzbPassEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _ui->compressNameEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _ui->rarEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rootLayout->addWidget(detailsFrame);

    _ui->aboutButton->hide();
}

void PostingWidget::_setAdvancedSectionVisible(bool visible)
{
    const bool shouldPersist = _ngPost && _ngPost->_quickAdvancedVisible != visible;
    if (_ngPost)
        _ngPost->_quickAdvancedVisible = visible;
    _advancedVisible = visible;
    _ui->advancedPanel->setMaximumHeight(visible ? QWIDGETSIZE_MAX : 0);
    _ui->advancedPanel->setMinimumHeight(0);
    _ui->advancedPanel->setSizePolicy(QSizePolicy::Preferred, visible ? QSizePolicy::Preferred : QSizePolicy::Fixed);
    _ui->advancedPanel->setVisible(visible);
    setLayoutWidgetsVisible(_ui->detailsStackLayout, visible);
    _ui->advancedTitleLabel->setVisible(visible);
    _ui->advancedPanel->updateGeometry();

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

void PostingWidget::_refreshAdvancedToggleButtonGeometry()
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

void PostingWidget::addPath(const QString &path, int currentNbFiles, int isDir)
{
    if (_ui->filesList->addPathIfNotInList(path, currentNbFiles, isDir))
    {
        QFileInfo fileInfo(path);
        if (_ui->nzbFileEdit->text().isEmpty())
        {
            _ngPost->setNzbName(fileInfo);
            _ui->nzbFileEdit->setText(QString("%1.nzb").arg(_ngPost->nzbPath()));
        }
        if (_ui->compressNameEdit->text().isEmpty())
            _ui->compressNameEdit->setText(_ngPost->_nzbName);
    }
}

bool PostingWidget::_fileAlreadyInList(const QString &fileName, int currentNbFiles) const
{
    for (int i = 0 ; i < currentNbFiles ; ++i)
    {
        if (_ui->filesList->item(i)->text() == fileName)
            return true;
    }
    return false;
}

void PostingWidget::setIDLE()
{
    _ui->postButton->setText(tr("Post Files"));
    _state = STATE::IDLE;
}

void PostingWidget::setPosting()
{
    _hmi->updateJobTab(this, _hmi->sPostingColor, QIcon(_hmi->sPostingIcon), _postingJob->nzbName());
    _ui->postButton->setText(tr("Stop Posting"));
    _state = STATE::POSTING;
}
