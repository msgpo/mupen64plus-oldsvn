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

#include <KApplication>
#include <KStandardAction>
#include <KActionCollection>
#include <KAction>
#include <KLocale>
#include <KStatusBar>
#include <KFileDialog>
#include <KMessageBox>
#include <KRecentFilesAction>
#include <KConfigDialog>
#include <KDebug>
#include <KMenu>
#include <KMainWindow>
#include <KMenuBar>
#include <KLineEdit>

#include <QItemSelectionModel>
#include <QtGui>
#include <qdesktopwidget.h>

#include "mainwindow.h"
#include "mainwidget.h"
#include "globals.h"
#include "rommodel.h"
#include "plugins.h"

// Autogenerated files
#include "settings.h"
#include "ui_mainsettingswidget.h"
#include "ui_pluginssettingswidget.h"
#include "ui_rombrowsersettingswidget.h"

namespace core {
    extern "C" {
        #include "../main.h"
        #include "../plugin.h"
        #include "../savestates.h"
        #include "../rom.h"
        #include "../config.h"
    }
}

enum StatusBarFields { ItemCountField };

MainWindow::MainWindow() 
    : KMainWindow(0)
    , m_mainWidget(0)
    , m_actionRecentFiles(0)
{
    m_mainWidget = new MainWidget(this);
    setCentralWidget(m_mainWidget);
    createActions();
    createMenus();
    createToolBars();

    statusBar()->insertPermanentItem("", ItemCountField);

    connect(m_mainWidget, SIGNAL(itemCountChanged(int)),
             this, SLOT(updateItemCount(int)));
    connect(m_mainWidget, SIGNAL(romDoubleClicked(KUrl, unsigned int)),
             this, SLOT(romOpen(KUrl, unsigned int)));

    /*char resourcefilename[PATH_MAX];
    std::sprintf(resourcefilename, "%s%s", core::get_installpath(), "mupen64plusui.rc");
    QFile resourcefile;
    resourcefile.setFileName(resourcefilename);
    if(!resourcefile.open(QIODevice::ReadOnly))
        { printf("Error, unable to open Qt4 resource file %s.\n", resourcefilename); }
    resourcefile.close();
    */

   int i;

   if((i=core::config_get_bool("ToolbarVisible",2))==2)
        i = true;

    if(i)
        mainToolBar->show();
    else
        mainToolBar->hide();

    if((i=core::config_get_bool("FilterVisible",2))==2)
        i = true;

    if(i) {
        m_mainWidget->filterLabel->show();
        m_mainWidget->m_lineEdit->show();
    }
    else {
        m_mainWidget->filterLabel->hide();
        m_mainWidget->m_lineEdit->hide();
    }

    if((i=core::config_get_bool("StatusBarVisible",2))==2)
        i = true;

    if(i)
        statusBar()->show();
    else
        statusBar()->hide();


    QSize size(core::config_get_number("MainWindowWidth",600),
               core::config_get_number("MainWindowHeight",400));
    QPoint position(core::config_get_number("MainWindowXPosition",0),
                    core::config_get_number("MainWindowYPosition",0));

    QDesktopWidget *d = QApplication::desktop();
    QSize desktop(d->width(), d->height());

    if (position.x() > desktop.width()) {
        position.setX(0);
    }
    if (position.y() > desktop.height()) {
        position.setY(0);
    }

    if (size.width() > desktop.width()) {
        size.setWidth(600);
    }
    if (size.height() > desktop.height()) {
        size.setHeight(400);
    }

    if ((position.x() + size.width()) > desktop.width()) {
        position.setX(desktop.width() - size.width());
    }
    if ((position.y()+size.height())>desktop.height()) {
        position.setY(desktop.height() - size.height());
    }

    resize(size);
    move(position);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    core::config_put_number("MainWindowWidth", width());
    core::config_put_number("MainWindowHeight", height());
    core::config_put_number("MainWindowXPosition", x());
    core::config_put_number("MainWindowYPosition", y());
    core::config_put_bool("ToolbarVisible", mainToolBar->isVisible());
    core::config_put_bool("FilterVisible", m_mainWidget->filterLabel->isVisible());
    core::config_put_bool("StatusbarVisible", statusBar()->isVisible());
}

void MainWindow::showInfoMessage(const QString& msg)
{
    statusBar()->showMessage(msg);
}

void MainWindow::showAlertMessage(const QString& msg)
{
    KMessageBox::error(this, msg);
}

bool MainWindow::confirmMessage(const QString& msg)
{
    switch(KMessageBox::questionYesNo(this, msg)) {
        case KMessageBox::Yes:
            return true;
            break;
        default:
            return false;
            break;
    }
}

void MainWindow::romOpen()
{
    QString filter = RomExtensions.join(" ");
    QString filename = KFileDialog::getOpenFileName(KUrl(), filter);
    if (!filename.isEmpty()) {
        romOpen(filename, 0);
    }
}

void MainWindow::romOpen(const KUrl& url)
{
    romOpen(url, 0);
}

void MainWindow::romOpen(const KUrl& url, unsigned int archivefile)
{
    QString path = url.path();
    //if (url.isLocalFile()) {
       // m_actionRecentFiles->addUrl(url);
       // m_actionRecentFiles->saveEntries(KGlobal::config()->group("Recent Roms"));
       // KGlobal::config()->sync();
    if (core::open_rom(path.toLocal8Bit(), archivefile) == 0) {
        core::startEmulation();
    }
}

void MainWindow::romClose()
{
    core::close_rom();
}

void MainWindow::emulationStart()
{
    if(!core::rom) {
        QModelIndex index = m_mainWidget->getRomBrowserIndex();
        QString filename = index.data(RomModel::FullPath).toString();
        unsigned int archivefile = index.data(RomModel::ArchiveFile).toUInt();
        if (filename.isEmpty()) {
            const char* m = "There is no Rom loaded. Do you want to load one?";
            if (confirmMessage(i18n(m))) {
                romOpen();
            }
            return;
        }
        else
            romOpen(filename, archivefile);
    }

    core::startEmulation();
}

void MainWindow::emulationPauseContinue()
{
    core::pauseContinueEmulation();
}

void MainWindow::emulationStop()
{
    core::stopEmulation();
}

void MainWindow::viewFullScreen()
{
    core::changeWindow();
}

void MainWindow::saveStateSave()
{
    if (core::g_EmulationThread) {
        core::savestates_job |= SAVESTATE;
    }
}

void MainWindow::saveStateSaveAs()
{
    if (core::g_EmulationThread) {
        QString filename = KFileDialog::getSaveFileName();
        if (!filename.isEmpty()) {
            core::savestates_select_filename(filename.toLocal8Bit());
            core::savestates_job |= SAVESTATE;
        }
    } else {
        showAlertMessage(i18n("Emulation not running!"));
    }
}

void MainWindow::saveStateLoad()
{
    if (core::g_EmulationThread) {
        core::savestates_job |= LOADSTATE;
    }
}

void MainWindow::saveStateLoadAs()
{
    if (core::g_EmulationThread) {
        QString filename = KFileDialog::getOpenFileName();
        if (!filename.isEmpty()) {
            core::savestates_select_filename(filename.toLocal8Bit());
            core::savestates_job |= LOADSTATE;
        }
    } else {
        showAlertMessage(i18n("Emulation not running!"));
    }
}

void MainWindow::savestateCheckSlot()
{
    int slot = core::savestates_get_slot();
    QAction* a = slotActions.at(slot);
    a->setChecked(true);
}

void MainWindow::toggleCheckViewable()
{
    if(mainToolBar->isVisible())
        settings_show_toolbar->setChecked(true);
    else
        settings_show_toolbar->setChecked(false);

    if(m_mainWidget->filterLabel->isVisible())
        settings_show_filter->setChecked(true);
    else
        settings_show_filter->setChecked(false);

    if(statusBar()->isVisible())
        settings_show_statusbar->setChecked(true);
    else
        settings_show_statusbar->setChecked(false);
}

void MainWindow::savestateSelectSlot(QAction* a)
{
    bool ok = false;
    int slot = a->data().toInt(&ok);
    if (ok) {
        core::savestates_select_slot(slot);
    }
}

//Slot should be renamed.
void MainWindow::configDialogShow()
{
    if (KConfigDialog::showDialog("settings")) {
        return;
    }

    KConfigDialog* dialog = new KConfigDialog(this, "settings",
                                              Settings::self());

    // Main settings
    QWidget* mainSettingsWidget = new QWidget(dialog);
    Ui::MainSettings().setupUi(mainSettingsWidget);
    dialog->addPage(mainSettingsWidget, i18n("Main Settings"),
                     "preferences-system");

    // Plugin Settings
    QWidget* pluginsSettingsWidget = new QWidget(dialog);
    Plugins* plugins = Plugins::self();
    QSize labelIconSize(32, 32);
    Ui::PluginsSettings pluginsSettingsUi;

    pluginsSettingsUi.setupUi(pluginsSettingsWidget);

    // gfx plugin
    pluginsSettingsUi.kcfg_GraphicsPlugin->addItems(
        plugins->plugins(Plugins::Graphics)
    );
    pluginsSettingsUi.graphicsPluginIconLabel->setPixmap(
        KIcon("applications-graphics").pixmap(labelIconSize)
    );
    connect(pluginsSettingsUi.aboutGraphicsPluginButton, SIGNAL(clicked()),
             plugins, SLOT(aboutGraphicsPlugin()));
    connect(pluginsSettingsUi.configureGraphicsPluginButton, SIGNAL(clicked()),
             plugins, SLOT(configureGraphicsPlugin()));
    connect(pluginsSettingsUi.testGraphicsPluginButton, SIGNAL(clicked()),
             plugins, SLOT(testGraphicsPlugin()));

    // audio plugin
    pluginsSettingsUi.kcfg_AudioPlugin->addItems(
        plugins->plugins(Plugins::Audio)
    );
    pluginsSettingsUi.audioPluginIconLabel->setPixmap(
        KIcon("audio-card").pixmap(labelIconSize)
    );
    connect(pluginsSettingsUi.aboutAudioPluginButton, SIGNAL(clicked()),
             plugins, SLOT(aboutAudioPlugin()));
    connect(pluginsSettingsUi.configureAudioPluginButton, SIGNAL(clicked()),
             plugins, SLOT(configureAudioPlugin()));
    connect(pluginsSettingsUi.testAudioPluginButton, SIGNAL(clicked()),
             plugins, SLOT(testAudioPlugin()));

    // input plugin
    pluginsSettingsUi.kcfg_InputPlugin->addItems(
        plugins->plugins(Plugins::Input)
    );
    pluginsSettingsUi.inputPluginIconLabel->setPixmap(
        KIcon("input-gaming").pixmap(labelIconSize)
    );
    connect(pluginsSettingsUi.aboutInputPluginButton, SIGNAL(clicked()),
             plugins, SLOT(aboutInputPlugin()));
    connect(pluginsSettingsUi.configureInputPluginButton, SIGNAL(clicked()),
             plugins, SLOT(configureInputPlugin()));
    connect(pluginsSettingsUi.testInputPluginButton, SIGNAL(clicked()),
             plugins, SLOT(testInputPlugin()));

    // rsp plugin
    pluginsSettingsUi.kcfg_RspPlugin->addItems(
        plugins->plugins(Plugins::Rsp)
    );
    pluginsSettingsUi.rspPluginIconLabel->setPixmap(
        KIcon("cpu").pixmap(labelIconSize)
    );
    connect(pluginsSettingsUi.aboutRspPluginButton, SIGNAL(clicked()),
             plugins, SLOT(aboutRspPlugin()));
    connect(pluginsSettingsUi.configureRspPluginButton, SIGNAL(clicked()),
             plugins, SLOT(configureRspPlugin()));
    connect(pluginsSettingsUi.testRspPluginButton, SIGNAL(clicked()),
             plugins, SLOT(testRspPlugin()));

    dialog->addPage(pluginsSettingsWidget, plugins, i18n("Plugins"),
                     "applications-engineering");

    // Rom browser settings
    QWidget* romBrowserSettingsWidget = new QWidget(dialog);
    Ui::RomBrowserSettings().setupUi(romBrowserSettingsWidget);
    dialog->addPage(romBrowserSettingsWidget, i18n("Rom Browser"),
                     "preferences-system-network");

    connect(dialog, SIGNAL(settingsChanged(QString)),
             RomModel::self(), SLOT(settingsChanged()));

    dialog->show();
}

void MainWindow::updateItemCount(int count)
{
    statusBar()->changeItem(i18n("%0 roms").arg(count), ItemCountField);
}

bool MainWindow::event(QEvent* event)
{
    bool retval = false;
    switch (event->type()) {
        case InfoEventType:
            showInfoMessage(static_cast<InfoEvent*>(event)->message);
            retval = true;
            break;
        case AlertEventType:
            showAlertMessage(static_cast<AlertEvent*>(event)->message);
            retval = true;
            break;
        default:
            retval = KMainWindow::event(event);
            break;
    }
    return retval;
}

void MainWindow::toolbarIconOnly()
{
    mainToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
}

void MainWindow::toolbarTextOnly()
{
    mainToolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
}

void MainWindow::toolbarTextBeside()
{
    mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
}

void MainWindow::toolbarTextUnder()
{
    mainToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
}

void MainWindow::toggleToolBar()
{
    if(mainToolBar->isVisible())
        mainToolBar->hide();
    else
        mainToolBar->show();
}

void MainWindow::toggleStatusBar()
{
    if(statusBar()->isVisible())
        statusBar()->hide();
    else
        statusBar()->show();
}

void MainWindow::createMenus()
{
     fileMenu = menuBar()->addMenu(i18n("&File"));
     fileMenu->addAction(rom_open);
     fileMenu->addAction(rom_close);
     fileMenu->addSeparator();
     fileMenu->addAction(application_close);

     emulationMenu = menuBar()->addMenu(i18n("&Emulation"));
     emulationMenu->addAction(emulation_start);
     emulationMenu->addAction(emulation_pause);
     emulationMenu->addAction(emulation_stop);
     emulationMenu->addSeparator();
     emulationMenu->addAction(emulation_save_state);
     emulationMenu->addAction(emulation_save_state_as);
     emulationMenu->addAction(emulation_load_state);
     emulationMenu->addAction(emulation_load_state_as);
     emulationMenu->addSeparator();
     emulationMenu->addAction(emulation_current_slot);

     settingsMenu = menuBar()->addMenu(i18n("&Settings"));
     connect(settingsMenu, SIGNAL(aboutToShow()), this, SLOT(toggleCheckViewable()));
     settingsMenu->addAction(settings_show_toolbar);
     settingsMenu->addAction(settings_show_filter);
     settingsMenu->addAction(settings_show_statusbar);
     settingsMenu->addSeparator();
     settingsMenu->addAction(settings_fullscreen);
     settingsMenu->addSeparator();
     settingsMenu->addAction(settings_configure_graphics);
     settingsMenu->addAction(settings_configure_audio);
     settingsMenu->addAction(settings_configure_input);
     settingsMenu->addAction(settings_configure_rsp);
     settingsMenu->addSeparator();
     settingsMenu->addAction(settings_configure_mupen);
}

void MainWindow::createToolBars()
{
    mainToolBar = addToolBar(i18n("Show Toolbar"));
    mainToolBar->addAction(rom_open);
    mainToolBar->addSeparator();
    mainToolBar->addAction(emulation_start);
    mainToolBar->addAction(emulation_pause);
    mainToolBar->addAction(emulation_stop);
    mainToolBar->addSeparator();
    mainToolBar->addAction(settings_configure_mupen);
    mainToolBar->addAction(settings_fullscreen);

    //Get toolbar edge size in pixels
    int edgeSize = core::config_get_number("ToolbarSize",0);
    QSize toolBarSize(edgeSize, edgeSize);
    mainToolBar->setIconSize(toolBarSize);

    //Get toolbar style.
    //Key: 0 = icons only. 1 = text only. 2 = text under icons. 3 = text besides icons.
    //Note: Gtk GUI can't support style 3.
    int style = core::config_get_number("ToolbarStyle",0);

    switch(style)
        {
        case 0: mainToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly); break;
        case 1: mainToolBar->setToolButtonStyle(Qt::ToolButtonTextOnly); break;
        case 2: mainToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon); break; 
        case 3: mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); break;
        default: mainToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        }
}

void MainWindow::createActions()
{
    int i;

    //File Actions
    rom_open = new QAction(KIcon("document-open"), i18n("&Open Rom..."), this);
    rom_open->setShortcut(Qt::ControlModifier+Qt::Key_O);
    connect(rom_open, SIGNAL(triggered()), this, SLOT(romOpen()));

/*
   // m_actionRecentFiles = KStandardAction::openRecent(this, SLOT(romOpen(KUrl)),
                                                       actionCollection());
    //m_actionRecentFiles->loadEntries(KGlobal::config()->group("Recent Roms"));
*/

    rom_close = new QAction(KIcon("dialog-close"), i18n("&Close Rom"), this);
    rom_close->setShortcut(  Qt::ControlModifier+Qt::Key_W);
    connect(rom_close, SIGNAL(triggered()), this, SLOT(romClose()));

    application_close = new QAction(KIcon("application-exit"), i18n("&Quit"), this);
    application_close->setShortcut(  Qt::ControlModifier+Qt::Key_Q);
    connect(application_close, SIGNAL(triggered()), this, SLOT(close()));

    //Emulation Actions
    emulation_start = new QAction(KIcon("media-playback-start"), i18n("&Start"), this);
    connect(emulation_start, SIGNAL(triggered()), this, SLOT(emulationStart()));

    emulation_pause = new QAction(KIcon("media-playback-pause"), i18n("&Pause"), this);
    emulation_pause->setShortcut(Qt::Key_Pause);
    connect(emulation_pause, SIGNAL(triggered()), this, SLOT(emulationPauseContinue()));

    emulation_stop = new QAction(KIcon("media-playback-stop"), i18n("S&top"), this);
    emulation_stop->setShortcut(Qt::Key_Escape);
    connect(emulation_stop, SIGNAL(triggered()), this, SLOT(emulationStop()));

    emulation_load_state = new QAction(KIcon("document-revert"), i18n("&Load State"), this);
    emulation_load_state->setShortcut(Qt::Key_F5);
    connect(emulation_load_state, SIGNAL(triggered()), this, SLOT(saveStateSave()));

    emulation_save_state = new QAction(KIcon("document-save"), i18n("Sa&ve State"), this);
    emulation_save_state->setShortcut(Qt::Key_F7);
    connect(emulation_save_state, SIGNAL(triggered()), this, SLOT(saveStateLoad()));

    emulation_load_state_as = new QAction(KIcon("document-open"), i18n("L&oad State from..."), this);
    connect(emulation_load_state_as, SIGNAL(triggered()), this, SLOT(saveStateSaveAs()));

    emulation_save_state_as = new QAction(KIcon("document-save-as"), i18n("Save State &as..."), this);
    connect(emulation_save_state_as, SIGNAL(triggered()), this, SLOT(saveStateLoadAs()));

    emulation_current_slot =  new QAction(this);
    emulation_current_slot->setText(i18n("&Current Save State Slot"));
    QMenu* slotMenu = new QMenu(this);
    QActionGroup* slotActionGroup = new QActionGroup(emulation_current_slot);
    for (i = 0; i < 10; i++)
        {
        QAction* slot = slotMenu->addAction(i18n("Slot &%1", i)); 
        slot->setCheckable(true);
        slot->setData(i);
        slotActionGroup->addAction(slot);
        }
    emulation_current_slot->setMenu(slotMenu);
    slotActions = slotActionGroup->actions();
    connect(slotMenu, SIGNAL(aboutToShow()), this, SLOT(savestateCheckSlot()));
    connect(slotActionGroup, SIGNAL(triggered(QAction*)), this, SLOT(savestateSelectSlot(QAction*)));

    //Settings Actions
    settings_show_toolbar = new QAction(this);
    settings_show_toolbar->setText(i18n("Show &Toolbar"));
    settings_show_toolbar->setCheckable(true);
    connect(settings_show_toolbar, SIGNAL(triggered()), this, SLOT(toggleToolBar()));

    settings_show_filter = new QAction(this);
    settings_show_filter->setText(i18n("Show F&ilter"));
    settings_show_filter->setCheckable(true);
    connect(settings_show_filter, SIGNAL(triggered()), m_mainWidget, SLOT(toggleFilter()));

    settings_show_statusbar = new QAction(this);
    settings_show_statusbar->setText(i18n("Show &Statusbar"));
    settings_show_statusbar->setCheckable(true);
    connect(settings_show_statusbar, SIGNAL(triggered()), this, SLOT(toggleStatusBar()));

    settings_fullscreen = new QAction(KIcon("view-fullscreen"), i18n("&Full Screen"), this);
    settings_fullscreen->setShortcut(Qt::AltModifier+Qt::Key_Return);
    connect(settings_fullscreen, SIGNAL(triggered()), this, SLOT(viewFullScreen()));

    //This handle should be a mainwindow class member, no?
    Plugins* plugins = Plugins::self();
    settings_configure_graphics = new QAction(KIcon("video-display"), i18n("Configure &Graphics Plugin..."), this);
    connect(settings_configure_graphics, SIGNAL(triggered()), plugins, SLOT(configureGraphicsPlugin()));

    settings_configure_audio = new QAction(KIcon("audio-card"), i18n("Configure &Audio Plugin..."), this);
    connect(settings_configure_audio, SIGNAL(triggered()), plugins, SLOT(configureAudioPlugin()));

    settings_configure_input = new QAction(KIcon("input-gaming"), i18n("Configure &Input Plugin..."), this);
    connect(settings_configure_input, SIGNAL(triggered()), plugins, SLOT(configureInputPlugin()));

    settings_configure_rsp = new QAction(KIcon("cpu"), i18n("Configure &RSP Plugin..."), this);
    connect(settings_configure_rsp, SIGNAL(triggered()), plugins, SLOT(configureRspPlugin()));

    settings_configure_mupen = new QAction(KIcon("preferences-system"), i18n("Configure &Mupen64plus..."), this);
    connect(settings_configure_mupen, SIGNAL(triggered()), this, SLOT(configDialogShow()));

    //Toolbar stuff for later...
    toolbar_icon_only = new QAction(this);
    toolbar_icon_only->setText(i18n("Icons only"));
    connect(toolbar_icon_only, SIGNAL(triggered()), this, SLOT(toolbarIconOnly()));

    toolbar_text_only = new QAction(this);
    toolbar_text_only->setText(i18n("Text only"));
    connect(toolbar_text_only, SIGNAL(triggered()), this, SLOT(toolbarTextOnly()));

    toolbar_text_beside = new QAction(this);
    toolbar_text_beside->setText(i18n("Text beside icons"));
    connect(toolbar_text_beside, SIGNAL(triggered()), this, SLOT(toolbarTextBeside()));

    toolbar_text_under = new QAction(this);
    toolbar_text_under->setText(i18n("Text under icons"));
    connect(toolbar_text_under, SIGNAL(triggered()), this, SLOT(toolbarTextUnder()));
}

#include "mainwindow.moc"
