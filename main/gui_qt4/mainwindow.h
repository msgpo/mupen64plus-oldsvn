/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mainwindow.h                                            *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Louai Al-Khanji                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef MUPEN_KDE4_MAINWINDOW_H
#define MUPEN_KDE4_MAINWINDOW_H

#include <QMainWindow>
#include <QEvent>
#include <QActionGroup>
#include <QPointer>

#include "ui_mainwindow.h"

class QLabel;

enum CustomEventTypes
{
    InfoEventType = QEvent::User,
    AlertEventType
};

class InfoEvent : public QEvent
{
    public:
        InfoEvent() : QEvent(Type(InfoEventType)) {}
        QString message;
};

class AlertEvent : public QEvent
{
    public:
        AlertEvent() : QEvent(Type(AlertEventType)) {}
        QString message;
};

class MainWindow : public QMainWindow, public Ui_MainWindow
{
    Q_OBJECT
    public:
        MainWindow();
        virtual ~MainWindow();

        void showInfoMessage(const QString& msg);
        void showAlertMessage(const QString& msg);
        bool confirmMessage(const QString& msg);
        void setState(unsigned char state);

    private slots:
        void romOpen();
        void romOpen(const QString& url);
        void romOpen(const QString& url, unsigned int archivefile);
        void romClose();

        void emulationStart();
        void emulationPauseContinue();
        void emulationStop();
        void saveStateSave();
        void saveStateSaveAs();
        void saveStateLoad();
        void saveStateLoadFrom();
        void savestateCheckSlot();
        void savestateSelectSlot(QAction* a);

        void fullScreenToggle();
        void configDialogShow();
        void itemCountUpdate(int count);
        void aboutDialogShow();

    protected:
        void customEvent(QEvent* event);
        void closeEvent(QCloseEvent* event);

    private:
        void startEmulation();
        void setupActions();
        QList<QAction*> slotActions;
        QLabel* m_statusBarLabel;
#ifdef __WIN32__
        QPointer<QWidget> m_renderWindow;

    protected:
        bool eventFilter(QObject *obj, QEvent *ev);
#endif
};

#endif // MUPEN_KDE4_MAINWINDOW_H

