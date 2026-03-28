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

#include "AboutNgPost.h"
#include "ui_AboutNgPost.h"
#include "NgPost.h"

#include <QApplication>
#include <QLayout>
#include <QPalette>

AboutNgPost::AboutNgPost(NgPost *ngPost, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutNgPost)
{
    ui->setupUi(this);
    setWindowTitle(tr("About ngPost").replace("ngPost", NgPost::displayName()));
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    setWindowFlag(Qt::FramelessWindowHint);
#endif

    if (QLayout *mainLayout = layout())
    {
        mainLayout->setSizeConstraint(QLayout::SetFixedSize);
        mainLayout->setContentsMargins(12, 10, 12, 10);
        mainLayout->setSpacing(8);
    }

    const QPalette pal = qApp->palette();
    const QColor windowColor = pal.color(QPalette::Window);
    const QColor borderColor = pal.color(QPalette::Mid);
    const QColor textColor = pal.color(QPalette::WindowText);
    const QColor mutedText = pal.color(QPalette::PlaceholderText).isValid()
            ? pal.color(QPalette::PlaceholderText)
            : textColor.darker(130);
    const QColor accentColor = pal.color(QPalette::Highlight);

    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString(
        "QDialog {"
        " background: %1;"
        " border: 1px solid %2;"
        " border-radius: 12px;"
        "}"
        "QLabel { color: %3; }"
        "QLabel#descLbl { color: %4; }"
        "QLabel#copyrightLbl { color: %5; }")
        .arg(windowColor.name(),
             borderColor.name(),
             textColor.name(),
             accentColor.name(),
             mutedText.name()));

    ui->logoLbl->setMaximumSize(72, 72);
    ui->titleLbl->setText(QString("<pre>%1</pre>").arg(ngPost->escapeXML(ngPost->brandedAsciiArtWithVersion())));
    ui->titleLbl->setFont(QFont("Segoe UI", 13, QFont::Bold));

    ui->copyrightLbl->setText("Copyright 2026 by spotnet.team (by tb). build met C++ / QT 6");
    ui->copyrightLbl->setFont(QFont("Segoe UI", 9, QFont::DemiBold));

    ui->descLbl->setTextFormat(Qt::RichText);
    ui->descLbl->setText(ngPost->desc(true));
    ui->descLbl->setOpenExternalLinks(true);
    ui->descLbl->setWordWrap(true);
    ui->descLbl->setMaximumWidth(420);
    ui->descLbl->setFont(QFont("Segoe UI", 10, QFont::Medium));
//    ui->cosi7->setFont(QFont( "DejaVu Serif", 28, QFont::Bold));

    ui->closeButton->setFixedSize(22, 22);
    ui->closeButton->setIconSize(QSize(12, 12));

    connect(ui->closeButton, &QAbstractButton::clicked, this, &QWidget::close);

    setMinimumSize(QSize(0, 0));
    resize(layout()->sizeHint().expandedTo(QSize(440, 250)).boundedTo(QSize(520, 320)));
}

AboutNgPost::~AboutNgPost()
{
    delete ui;
}

void AboutNgPost::keyPressEvent(QKeyEvent *e)
{
    Q_UNUSED(e)
    close();
}

#include <QMouseEvent>
void AboutNgPost::mousePressEvent(QMouseEvent *e)
{
    Q_UNUSED(e)
    if (e->button() == Qt::RightButton)
        close();
}

