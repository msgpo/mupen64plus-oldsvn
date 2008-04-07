/*
* Copyright (C) 2008 Louai Al-Khanji
*
* This program is free software; you can redistribute it and/
* or modify it under the terms of the GNU General Public Li-
* cence as published by the Free Software Foundation; either
* version 2 of the Licence, or any later version.
*
* This program is distributed in the hope that it will be use-
* ful, but WITHOUT ANY WARRANTY; without even the implied war-
* ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public Licence for more details.
*
* You should have received a copy of the GNU General Public
* Licence along with this program; if not, write to the Free
* Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
* USA.
*
*/

#include <QVBoxLayout>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QLabel>
#include <QHeaderView>

#include <KLineEdit>
#include <KLocale>

#include "mainwidget.h"
#include "rommodel.h"

MainWidget::MainWidget(QWidget* parent)
    : QWidget(parent)
    , m_treeView(0)
    , m_lineEdit(0)
    , m_proxyModel(0)
{   
    m_treeView = new QTreeView(this);
    m_lineEdit = new KLineEdit(this);
    m_proxyModel = new QSortFilterProxyModel(this);
    
    m_lineEdit->setClearButtonShown(true);
    
    m_proxyModel->setSourceModel(RomModel::self());
    m_proxyModel->setFilterKeyColumn(-1); // search all columns
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->setSortRole(RomModel::Sort);

    m_treeView->setRootIsDecorated(false);
    m_treeView->setSortingEnabled(true);
    m_treeView->setModel(m_proxyModel);
    m_treeView->sortByColumn(RomModel::GoodName, Qt::AscendingOrder);
    m_treeView->header()->resizeSections(QHeaderView::ResizeToContents);
    
    
    connect(m_lineEdit, SIGNAL(textChanged(QString)),
             m_proxyModel, SLOT(setFilterFixedString(QString)));
    connect(RomModel::self(), SIGNAL(modelReset()),
             this, SLOT(resizeHeaderSections()));
    connect(RomModel::self(), SIGNAL(dataChanged(QModelIndex, QModelIndex)),
             this, SLOT(resizeHeaderSections()));
    connect(RomModel::self(), SIGNAL(rowsInserted(QModelIndex, int, int)),
             this, SLOT(resizeHeaderSections()));

    QLabel* filterLabel = new QLabel(i18n("Filter:"), this);
    filterLabel->setBuddy(m_lineEdit);
    QHBoxLayout* filterLayout = new QHBoxLayout;
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_lineEdit);
    filterLayout->setMargin(0);
    
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addLayout(filterLayout);
    layout->addWidget(m_treeView);
    setLayout(layout);
    
    m_lineEdit->setFocus();
}

void MainWidget::resizeHeaderSections()
{
    m_treeView->header()->resizeSections(QHeaderView::ResizeToContents);
}

#include "mainwidget.moc"
