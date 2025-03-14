/*=====================================================================

QGroundControl Open Source Ground Control Station

(c) 2009 - 2013 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>

This file is part of the QGROUNDCONTROL project

    QGROUNDCONTROL is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    QGROUNDCONTROL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with QGROUNDCONTROL. If not, see <http://www.gnu.org/licenses/>.

======================================================================*/

/**
 * @file
 *   @brief Implementation of class MainWindow
 *   @author Lorenz Meier <mail@qgroundcontrol.org>
 */

#include <QSettings>
#include <QNetworkInterface>
#include <QDebug>
#include <QTimer>
#include <QHostInfo>
#include <QSplashScreen>
#include <QQuickView>
#include <QDesktopWidget>
#include <QScreen>
#include <QDesktopServices>
#include <QDockWidget>

#include "QGC.h"
#include "MAVLinkProtocol.h"
#include "MainWindow.h"
#include "GAudioOutput.h"
#include "QGCMAVLinkLogPlayer.h"
#include "SettingsDialog.h"
#include "MAVLinkDecoder.h"
#include "QGCDataPlot2D.h"
#include "Linecharts.h"
#include "FlightDisplayView.h"
#include "SetupView.h"
#include "QGCApplication.h"
#include "QGCFileDialog.h"
#include "QGCMessageBox.h"
#include "MultiVehicleManager.h"
#include "HomePositionManager.h"
#include "MissionEditor.h"
#include "LogCompressor.h"
#include "UAS.h"

#ifndef __mobile__
#include "QGCUASFileViewMulti.h"
#include "UASQuickView.h"
#include "QGCTabbedInfoView.h"
#include "UASRawStatusView.h"
#include "CustomCommandWidget.h"
#include "QGCDockWidget.h"
#include "FlightDisplayWidget.h"
#include "UASInfoWidget.h"
#include "HILDockWidget.h"
#endif

#ifndef __ios__
#include "SerialLink.h"
#endif

#ifdef UNITTEST_BUILD
#include "QmlControls/QmlTestWidget.h"
#endif

#ifdef QGC_OSG_ENABLED
#include "Q3DWidgetFactory.h"
#endif


/// The key under which the Main Window settings are saved
const char* MAIN_SETTINGS_GROUP = "QGC_MAINWINDOW";

#ifndef __mobile__
const char* MainWindow::_mavlinkDockWidgetName = "MAVLINK_INSPECTOR_DOCKWIDGET";
const char* MainWindow::_customCommandWidgetName = "CUSTOM_COMMAND_DOCKWIDGET";
const char* MainWindow::_filesDockWidgetName = "FILE_VIEW_DOCKWIDGET";
const char* MainWindow::_uasStatusDetailsDockWidgetName = "UAS_STATUS_DETAILS_DOCKWIDGET";
const char* MainWindow::_mapViewDockWidgetName = "MAP_VIEW_DOCKWIDGET";
const char* MainWindow::_pfdDockWidgetName = "PRIMARY_FLIGHT_DISPLAY_DOCKWIDGET";
const char* MainWindow::_uasInfoViewDockWidgetName = "UAS_INFO_INFOVIEW_DOCKWIDGET";
const char* MainWindow::_hilDockWidgetName = "HIL_DOCKWIDGET";
#endif

static MainWindow* _instance = NULL;   ///< @brief MainWindow singleton

MainWindow* MainWindow::_create(QSplashScreen* splashScreen)
{
    Q_ASSERT(_instance == NULL);
    new MainWindow(splashScreen);
    // _instance is set in constructor
    Q_ASSERT(_instance);
    return _instance;
}

MainWindow* MainWindow::instance(void)
{
    return _instance;
}

void MainWindow::deleteInstance(void)
{
    delete this;
}

/// @brief Private constructor for MainWindow. MainWindow singleton is only ever created
///         by MainWindow::_create method. Hence no other code should have access to
///         constructor.
MainWindow::MainWindow(QSplashScreen* splashScreen)
    : _autoReconnect(false)
    , _lowPowerMode(false)
    , _showStatusBar(false)
    , _centerStackActionGroup(new QActionGroup(this))
    , _centralLayout(NULL)
    , _currentViewWidget(NULL)
    , _splashScreen(splashScreen)
    , _currentView(VIEW_SETUP)
{
    Q_ASSERT(_instance == NULL);
    _instance = this;

    if (splashScreen) {
        connect(this, &MainWindow::initStatusChanged, splashScreen, &QSplashScreen::showMessage);
    }

    // Qt 4/5 on Ubuntu does place the native menubar correctly so on Linux we revert back to in-window menu bar.
#ifdef Q_OS_LINUX
    menuBar()->setNativeMenuBar(false);
#endif
    // Setup user interface
    loadSettings();
    emit initStatusChanged(tr("Setting up user interface"), Qt::AlignLeft | Qt::AlignBottom, QColor(62, 93, 141));
    _ui.setupUi(this);
    // Make sure tool bar elements all fit before changing minimum width
    setMinimumWidth(1008);
    configureWindowName();

    // Setup central widget with a layout to hold the views
    _centralLayout = new QVBoxLayout();
    _centralLayout->setContentsMargins(0,0,0,0);
    centralWidget()->setLayout(_centralLayout);
    // Set dock options
    setDockOptions(AnimatedDocks | AllowTabbedDocks | AllowNestedDocks);
    // Setup corners
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

    // On Mobile devices, we don't want any main menus at all.
#ifdef __mobile__
    menuBar()->setNativeMenuBar(false);
#endif

#ifndef __mobile__
#ifdef UNITTEST_BUILD
    QAction* qmlTestAction = new QAction("Test QML palette and controls", NULL);
    connect(qmlTestAction, &QAction::triggered, this, &MainWindow::_showQmlTestWidget);
    _ui.menuWidgets->addAction(qmlTestAction);
#endif
#endif

    // Load QML Toolbar
    QDockWidget* widget = new QDockWidget(this);
    widget->setObjectName("ToolBarDockWidget");
    qmlRegisterType<MainToolBar>("QGroundControl.MainToolBar", 1, 0, "MainToolBar");
    _mainToolBar = new MainToolBar(widget);
    widget->setWidget(_mainToolBar);
    widget->setFeatures(QDockWidget::NoDockWidgetFeatures);
    widget->setTitleBarWidget(new QWidget(this)); // Disables the title bar
    addDockWidget(Qt::TopDockWidgetArea, widget);

    // Setup UI state machines
    _centerStackActionGroup->setExclusive(true);
    // Status Bar
    setStatusBar(new QStatusBar(this));
    statusBar()->setSizeGripEnabled(true);

#ifndef __mobile__
    emit initStatusChanged(tr("Building common widgets."), Qt::AlignLeft | Qt::AlignBottom, QColor(62, 93, 141));
    _buildCommonWidgets();
    emit initStatusChanged(tr("Building common actions"), Qt::AlignLeft | Qt::AlignBottom, QColor(62, 93, 141));
#endif
    
    // Create actions
    connectCommonActions();
    // Connect user interface devices
#ifdef QGC_MOUSE_ENABLED_WIN
    emit initStatusChanged(tr("Initializing 3D mouse interface"), Qt::AlignLeft | Qt::AlignBottom, QColor(62, 93, 141));
    mouseInput = new Mouse3DInput(this);
    mouse = new Mouse6dofInput(mouseInput);
#endif //QGC_MOUSE_ENABLED_WIN

#if QGC_MOUSE_ENABLED_LINUX
    emit initStatusChanged(tr("Initializing 3D mouse interface"), Qt::AlignLeft | Qt::AlignBottom, QColor(62, 93, 141));

    mouse = new Mouse6dofInput(this);
    connect(this, SIGNAL(x11EventOccured(XEvent*)), mouse, SLOT(handleX11Event(XEvent*)));
#endif //QGC_MOUSE_ENABLED_LINUX

    // These also cause the screen to redraw so we need to update any OpenGL canvases in QML controls
    connect(LinkManager::instance(), &LinkManager::linkConnected,    this, &MainWindow::_linkStateChange);
    connect(LinkManager::instance(), &LinkManager::linkDisconnected, this, &MainWindow::_linkStateChange);

    // Connect link
    if (_autoReconnect)
    {
        restoreLastUsedConnection();
    }

    // Set low power mode
    enableLowPowerMode(_lowPowerMode);
    emit initStatusChanged(tr("Restoring last view state"), Qt::AlignLeft | Qt::AlignBottom, QColor(62, 93, 141));
    // Restore the window setup
    _loadCurrentViewState();
#ifndef __mobile__

    // Restore the window position and size
    emit initStatusChanged(tr("Restoring last window size"), Qt::AlignLeft | Qt::AlignBottom, QColor(62, 93, 141));
    if (settings.contains(_getWindowGeometryKey()))
    {
        restoreGeometry(settings.value(_getWindowGeometryKey()).toByteArray());
    }
    else
    {
        // Adjust the size
        QScreen* scr = QApplication::primaryScreen();
        QSize scrSize = scr->availableSize();
        if (scrSize.width() <= 1280)
        {
            resize(scrSize.width(), scrSize.height());
        }
        else
        {
            int w = scrSize.width()  > 1600 ? 1600 : scrSize.width();
            int h = scrSize.height() >  800 ?  800 : scrSize.height();
            resize(w, h);
            move((scrSize.width() - w) / 2, (scrSize.height() - h) / 2);
        }
    }

    // Make sure the proper fullscreen/normal menu item is checked properly.
    if (isFullScreen())
    {
        _ui.actionFullscreen->setChecked(true);
        _ui.actionNormal->setChecked(false);
    }
    else
    {
        _ui.actionFullscreen->setChecked(false);
        _ui.actionNormal->setChecked(true);
    }

    // And that they will stay checked properly after user input
    connect(_ui.actionFullscreen, &QAction::triggered, this, &MainWindow::fullScreenActionItemCallback);
    connect(_ui.actionNormal,     &QAction::triggered, this, &MainWindow::normalActionItemCallback);
#endif

    connect(_ui.actionStatusBar,  &QAction::triggered, this, &MainWindow::showStatusBarCallback);

    // Set OS dependent keyboard shortcuts for the main window, non OS dependent shortcuts are set in MainWindow.ui
#ifdef Q_OS_MACX
    _ui.actionSetup->setShortcut(QApplication::translate("MainWindow", "Meta+1", 0));
    _ui.actionPlan->setShortcut(QApplication::translate("MainWindow", "Meta+2", 0));
    _ui.actionFlight->setShortcut(QApplication::translate("MainWindow", "Meta+3", 0));
    _ui.actionAnalyze->setShortcut(QApplication::translate("MainWindow", "Meta+4", 0));
    _ui.actionFullscreen->setShortcut(QApplication::translate("MainWindow", "Meta+Return", 0));
#else
    _ui.actionSetup->setShortcut(QApplication::translate("MainWindow", "Ctrl+1", 0));
    _ui.actionPlan->setShortcut(QApplication::translate("MainWindow", "Ctrl+2", 0));
    _ui.actionFlight->setShortcut(QApplication::translate("MainWindow", "Ctrl+3", 0));
    _ui.actionAnalyze->setShortcut(QApplication::translate("MainWindow", "Ctrl+4", 0));
    _ui.actionFullscreen->setShortcut(QApplication::translate("MainWindow", "Ctrl+Return", 0));
#endif

    connect(&windowNameUpdateTimer, SIGNAL(timeout()), this, SLOT(configureWindowName()));
    windowNameUpdateTimer.start(15000);
    emit initStatusChanged(tr("Done"), Qt::AlignLeft | Qt::AlignBottom, QColor(62, 93, 141));

    if (!qgcApp()->runningUnitTests()) {
        _ui.actionStatusBar->setChecked(_showStatusBar);
        showStatusBarCallback(_showStatusBar);
#ifdef __mobile__
        menuBar()->hide();
#endif
        show();
#ifdef Q_OS_MAC
        // TODO HACK
        // This is a really ugly hack. For whatever reason, by having a QQuickWidget inside a
        // QDockWidget (MainToolBar above), the main menu is not shown when the app first
        // starts. I looked everywhere and I could not find a solution. What I did notice was
        // that if any other window gets focus, the menu comes up when you come back to QGC.
        // That is, if you were to click on another window and then back to QGC, the menus
        // would appear. This hack below creates a 0x0 dialog and immediately closes it.
        // That works around the issue and it will do until I find the root of the problem.
        QDialog qd(this);
        qd.show();
        qd.raise();
        qd.activateWindow();
        qd.close();
#endif
    }
}

MainWindow::~MainWindow()
{
    // Delete all UAS objects
    for (int i=0;i<_commsWidgetList.size();i++)
    {
        _commsWidgetList[i]->deleteLater();
    }
    _instance = NULL;
}

void MainWindow::resizeEvent(QResizeEvent * event)
{
    QMainWindow::resizeEvent(event);
}

QString MainWindow::_getWindowStateKey()
{
	return QString::number(_currentView)+"_windowstate_";
}

QString MainWindow::_getWindowGeometryKey()
{
    return "_geometry";
}

#ifndef __mobile__
void MainWindow::_createDockWidget(const QString& title, const QString& name, Qt::DockWidgetArea area, QWidget* innerWidget)
{
    Q_ASSERT(!_mapName2DockWidget.contains(name));
	
    // Add to menu
    QAction* action = new QAction(title, NULL);
    action->setCheckable(true);
    action->setData(name);
    connect(action, &QAction::triggered, this, &MainWindow::_showDockWidgetAction);
    _ui.menuWidgets->addAction(action);
	
	// Create widget
	QGCDockWidget* dockWidget = new QGCDockWidget(title, action, this);
	Q_CHECK_PTR(dockWidget);
	dockWidget->setObjectName(name);
	dockWidget->setVisible (false);
	if (innerWidget) {
		// Put inner widget inside QDockWidget
		innerWidget->setParent(dockWidget);
		dockWidget->setWidget(innerWidget);
		innerWidget->setVisible(true);
	}
	
    _mapName2DockWidget[name] = dockWidget;
    _mapDockWidget2Action[dockWidget] = action;
    addDockWidget(area, dockWidget);
}

void MainWindow::_buildCommonWidgets(void)
{
    // Add generic MAVLink decoder
    // TODO: This is never deleted
    mavlinkDecoder = new MAVLinkDecoder(MAVLinkProtocol::instance(), this);
    connect(mavlinkDecoder, SIGNAL(valueChanged(int,QString,QString,QVariant,quint64)),
                      this, SIGNAL(valueChanged(int,QString,QString,QVariant,quint64)));

    // Log player
    // TODO: Make this optional with a preferences setting or under a "View" menu
    logPlayer = new QGCMAVLinkLogPlayer(statusBar());
    statusBar()->addPermanentWidget(logPlayer);

    // In order for Qt to save and restore state of widgets all widgets must be created ahead of time. We only create the QDockWidget
    // holders. We do not create the actual inner widget until it is needed. This saves memory and cpu from running widgets that are
    // never shown.

    struct DockWidgetInfo {
        const char* name;
        const char* title;
        Qt::DockWidgetArea area;
    };

    static const struct DockWidgetInfo rgDockWidgetInfo[] = {
        { _mavlinkDockWidgetName,           "MAVLink Inspector",        Qt::RightDockWidgetArea },
        { _customCommandWidgetName,         "Custom Command",			Qt::RightDockWidgetArea },
        { _filesDockWidgetName,             "Onboard Files",            Qt::RightDockWidgetArea },
        { _uasStatusDetailsDockWidgetName,  "Status Details",           Qt::RightDockWidgetArea },
        { _mapViewDockWidgetName,           "Map view",                 Qt::RightDockWidgetArea },
        { _pfdDockWidgetName,               "Primary Flight Display",   Qt::RightDockWidgetArea },
        { _uasInfoViewDockWidgetName,       "Info View",                Qt::LeftDockWidgetArea },
        { _hilDockWidgetName,               "HIL Config",               Qt::LeftDockWidgetArea },
    };
    static const size_t cDockWidgetInfo = sizeof(rgDockWidgetInfo) / sizeof(rgDockWidgetInfo[0]);

    for (size_t i=0; i<cDockWidgetInfo; i++) {
        const struct DockWidgetInfo* pDockInfo = &rgDockWidgetInfo[i];
        _createDockWidget(pDockInfo->title, pDockInfo->name, pDockInfo->area, NULL /* no inner widget yet */);
    }
}

/// Shows or hides the specified dock widget, creating if necessary
void MainWindow::_showDockWidget(const QString& name, bool show)
{
    if (!_mapName2DockWidget.contains(name)) {
        // Don't show any sort of warning here. Dock Widgets which have been remove could still be in settings.
        // Which would cause us to end up here.
        return;
    }
    
    // Create the inner widget if we need to
    if (!_mapName2DockWidget[name]->widget()) {
        _createInnerDockWidget(name);
    }
    
    Q_ASSERT(_mapName2DockWidget.contains(name));
    QDockWidget* dockWidget = _mapName2DockWidget[name];
    Q_ASSERT(dockWidget);
    
    dockWidget->setVisible(show);
    
    Q_ASSERT(_mapDockWidget2Action.contains(dockWidget));
    _mapDockWidget2Action[dockWidget]->setChecked(show);
}

/// Creates the specified inner dock widget and adds to the QDockWidget
void MainWindow::_createInnerDockWidget(const QString& widgetName)
{
    Q_ASSERT(_mapName2DockWidget.contains(widgetName)); // QDockWidget should already exist
    Q_ASSERT(!_mapName2DockWidget[widgetName]->widget());     // Inner widget should not
    
    QWidget* widget = NULL;
    
    if (widgetName == _mavlinkDockWidgetName) {
        widget = new QGCMAVLinkInspector(MAVLinkProtocol::instance(),this);
    } else if (widgetName == _customCommandWidgetName) {
        widget = new CustomCommandWidget(this);
    } else if (widgetName == _filesDockWidgetName) {
        widget = new QGCUASFileViewMulti(this);
    } else if (widgetName == _uasStatusDetailsDockWidgetName) {
        widget = new UASInfoWidget(this);
    } else if (widgetName == _pfdDockWidgetName) {
        widget = new FlightDisplayWidget(this);
#ifndef __mobile__
    } else if (widgetName == _hilDockWidgetName) {
        widget = new HILDockWidget(this);
#endif
    } else if (widgetName == _uasInfoViewDockWidgetName) {
        QGCTabbedInfoView* pInfoView = new QGCTabbedInfoView(this);
        pInfoView->addSource(mavlinkDecoder);
        widget = pInfoView;
    } else {
        qWarning() << "Attempt to create unknown Inner Dock Widget" << widgetName;
    }
    
    if (widget) {
        QDockWidget* dockWidget = _mapName2DockWidget[widgetName];
        Q_CHECK_PTR(dockWidget);
        widget->setParent(dockWidget);
        dockWidget->setWidget(widget);
    }
}

void MainWindow::_hideAllDockWidgets(void)
{
    foreach(QDockWidget* dockWidget, _mapName2DockWidget) {
        dockWidget->setVisible(false);
    }
}

void MainWindow::_showDockWidgetAction(bool show)
{
    QAction* action = dynamic_cast<QAction*>(QObject::sender());
    Q_ASSERT(action);
    _showDockWidget(action->data().toString(), show);
}
#endif

void MainWindow::_buildMissionEditorView(void)
{
    if (!_missionEditorView) {
        _missionEditorView = new MissionEditor(this);
        _missionEditorView->setVisible(false);
    }
}

void MainWindow::_buildFlightView(void)
{
    if (!_flightView) {
        _flightView = new FlightDisplayView(this);
        _flightView->setVisible(false);
    }
}

void MainWindow::_buildSetupView(void)
{
    if (!_setupView) {
        _setupView = new SetupView(this);
        _setupView->setVisible(false);
    }
}

void MainWindow::_buildAnalyzeView(void)
{
    if (!_analyzeView) {
        _analyzeView = new QGCDataPlot2D(this);
        _analyzeView->setVisible(false);
    }
}

void MainWindow::fullScreenActionItemCallback(bool)
{
    _ui.actionNormal->setChecked(false);
}

void MainWindow::normalActionItemCallback(bool)
{
    _ui.actionFullscreen->setChecked(false);
}

void MainWindow::showStatusBarCallback(bool checked)
{
    _showStatusBar = checked;
    checked ? statusBar()->show() : statusBar()->hide();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Disallow window close if there are active connections
    if (LinkManager::instance()->anyConnectedLinks()) {
        QGCMessageBox::StandardButton button =
            QGCMessageBox::warning(
                tr("QGroundControl close"),
                tr("There are still active connections to vehicles. Do you want to disconnect these before closing?"),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel);
		if (button == QMessageBox::Yes) {
			LinkManager::instance()->disconnectAll();
			// The above disconnect causes a flurry of activity as the vehicle components are removed. This in turn
			// causes the Windows Version of Qt to crash if you allow the close event to be accepted. In order to prevent
			// the crash, we ignore the close event and setup a delayed timer to close the window after things settle down.
			QTimer::singleShot(1500, this, &MainWindow::_closeWindow);
		}

        event->ignore();
        return;
    }

    // This will process any remaining flight log save dialogs
    qgcApp()->processEvents(QEventLoop::ExcludeUserInputEvents);
    
    // Should not be any active connections
    Q_ASSERT(!LinkManager::instance()->anyConnectedLinks());
    
    _storeCurrentViewState();
    storeSettings();
    event->accept();
}

void MainWindow::loadSettings()
{
    // Why the screaming?
    QSettings settings;
    settings.beginGroup(MAIN_SETTINGS_GROUP);
    _autoReconnect  = settings.value("AUTO_RECONNECT",      _autoReconnect).toBool();
    _lowPowerMode   = settings.value("LOW_POWER_MODE",      _lowPowerMode).toBool();
    _showStatusBar  = settings.value("SHOW_STATUSBAR",      _showStatusBar).toBool();
    settings.endGroup();
}

void MainWindow::storeSettings()
{
    QSettings settings;
    settings.beginGroup(MAIN_SETTINGS_GROUP);
    settings.setValue("AUTO_RECONNECT",     _autoReconnect);
    settings.setValue("LOW_POWER_MODE",     _lowPowerMode);
    settings.setValue("SHOW_STATUSBAR",     _showStatusBar);
    settings.endGroup();
    settings.setValue(_getWindowGeometryKey(), saveGeometry());
	
    // Save the last current view in any case
    settings.setValue("CURRENT_VIEW", _currentView);
    settings.setValue(_getWindowStateKey(), saveState());
}

void MainWindow::configureWindowName()
{
    QList<QHostAddress> hostAddresses = QNetworkInterface::allAddresses();
    QString windowname = qApp->applicationName() + " " + qApp->applicationVersion();
    bool prevAddr = false;
    windowname.append(" (" + QHostInfo::localHostName() + ": ");
    for (int i = 0; i < hostAddresses.size(); i++)
    {
        // Exclude loopback IPv4 and all IPv6 addresses
        if (hostAddresses.at(i) != QHostAddress("127.0.0.1") && !hostAddresses.at(i).toString().contains(":"))
        {
            if(prevAddr) windowname.append("/");
            windowname.append(hostAddresses.at(i).toString());
            prevAddr = true;
        }
    }
    windowname.append(")");
    setWindowTitle(windowname);
}

void MainWindow::enableAutoReconnect(bool enabled)
{
    _autoReconnect = enabled;
}

/**
* @brief Create all actions associated to the main window
*
**/
void MainWindow::connectCommonActions()
{
    // Bind together the perspective actions
    QActionGroup* perspectives = new QActionGroup(_ui.menuPerspectives);
    perspectives->addAction(_ui.actionAnalyze);
    perspectives->addAction(_ui.actionFlight);
    perspectives->addAction(_ui.actionPlan);
    perspectives->addAction(_ui.actionSetup);
    perspectives->setExclusive(true);

    // Mark the right one as selected
    if (_currentView == VIEW_ANALYZE)
    {
        _ui.actionAnalyze->setChecked(true);
        _ui.actionAnalyze->activate(QAction::Trigger);
    }
    if (_currentView == VIEW_FLIGHT)
    {
        _ui.actionFlight->setChecked(true);
        _ui.actionFlight->activate(QAction::Trigger);
    }
    if (_currentView == VIEW_MISSIONEDITOR)
    {
        _ui.actionPlan->setChecked(true);
        _ui.actionPlan->activate(QAction::Trigger);
    }
    if (_currentView == VIEW_SETUP)
    {
        _ui.actionSetup->setChecked(true);
        _ui.actionSetup->activate(QAction::Trigger);
    }

    // Connect actions from ui
    connect(_ui.actionAdd_Link, SIGNAL(triggered()), this, SLOT(manageLinks()));

    // Connect internal actions
    connect(MultiVehicleManager::instance(), &MultiVehicleManager::vehicleAdded, this, &MainWindow::_vehicleAdded);

    // Views actions
    connect(_ui.actionFlight,           SIGNAL(triggered()), this, SLOT(loadFlightView()));
    connect(_ui.actionAnalyze,          SIGNAL(triggered()), this, SLOT(loadAnalyzeView()));
    connect(_ui.actionPlan,             SIGNAL(triggered()), this, SLOT(loadPlanView()));
    
    // Help Actions
    connect(_ui.actionOnline_Documentation, SIGNAL(triggered()), this, SLOT(showHelp()));
    connect(_ui.actionDeveloper_Credits, SIGNAL(triggered()), this, SLOT(showCredits()));
    connect(_ui.actionProject_Roadmap, SIGNAL(triggered()), this, SLOT(showRoadMap()));

    // Audio output
    _ui.actionMuteAudioOutput->setChecked(GAudioOutput::instance()->isMuted());
    connect(GAudioOutput::instance(), SIGNAL(mutedChanged(bool)), _ui.actionMuteAudioOutput, SLOT(setChecked(bool)));
    connect(_ui.actionMuteAudioOutput, SIGNAL(triggered(bool)), GAudioOutput::instance(), SLOT(mute(bool)));

    // Application Settings
    connect(_ui.actionSettings, SIGNAL(triggered()), this, SLOT(showSettings()));

    // Update Tool Bar
    _mainToolBar->setCurrentView(_currentView);
}

void MainWindow::_openUrl(const QString& url, const QString& errorMessage)
{
    if(!QDesktopServices::openUrl(QUrl(url))) {
        QMessageBox::critical(
            this,
            tr("Could not open information in browser"),
            errorMessage);
    }
}

void MainWindow::showHelp()
{
    _openUrl(
        "http://qgroundcontrol.org/users/start",
        tr("To get to the online help, please open http://qgroundcontrol.org/user_guide in a browser."));
}

void MainWindow::showCredits()
{
    _openUrl(
        "http://qgroundcontrol.org/credits",
        tr("To get to the credits, please open http://qgroundcontrol.org/credits in a browser."));
}

void MainWindow::showRoadMap()
{
    _openUrl(
        "http://qgroundcontrol.org/dev/roadmap",
        tr("To get to the online help, please open http://qgroundcontrol.org/roadmap in a browser."));
}

void MainWindow::showSettings()
{
    SettingsDialog settings(this);
    settings.exec();
}

void MainWindow::commsWidgetDestroyed(QObject *obj)
{
    // Do not dynamic cast or de-reference QObject, since object is either in destructor or may have already
    // been destroyed.
    if (_commsWidgetList.contains(obj))
    {
        _commsWidgetList.removeOne(obj);
    }
}

void MainWindow::_vehicleAdded(Vehicle* vehicle)
{
    connect(vehicle->uas(), SIGNAL(valueChanged(int,QString,QString,QVariant,quint64)), this, SIGNAL(valueChanged(int,QString,QString,QVariant,quint64)));

    if (!linechartWidget)
    {
        linechartWidget = new Linecharts(this);
        linechartWidget->setVisible(false);
    }

    linechartWidget->addSource(mavlinkDecoder);
    if (_analyzeView != linechartWidget)
    {
        _analyzeView = linechartWidget;
    }
}

/// Stores the state of the toolbar, status bar and widgets associated with the current view
void MainWindow::_storeCurrentViewState(void)
{
#ifndef __mobile__
    // Save list of visible widgets
    bool firstWidget = true;
    QString widgetNames = "";
    foreach(QDockWidget* dockWidget, _mapName2DockWidget) {
        if (dockWidget->isVisible()) {
            if (!firstWidget) {
                widgetNames += ",";
            }
            widgetNames += dockWidget->objectName();
            firstWidget = false;
        }
    }
    settings.setValue(_getWindowStateKey() + "WIDGETS", widgetNames);
#endif
    settings.setValue(_getWindowStateKey(), saveState());
    settings.setValue(_getWindowGeometryKey(), saveGeometry());
}

/// Restores the state of the toolbar, status bar and widgets associated with the current view
void MainWindow::_loadCurrentViewState(void)
{
    QWidget* centerView = NULL;
    QString defaultWidgets;

    switch (_currentView) {
        case VIEW_SETUP:
            _buildSetupView();
            centerView = _setupView;
            break;

        case VIEW_ANALYZE:
            _buildAnalyzeView();
            centerView = _analyzeView;
            defaultWidgets = "PARAMETER_INTERFACE_DOCKWIDGET,FILE_VIEW_DOCKWIDGET";
            break;

        case VIEW_FLIGHT:
            _buildFlightView();
            centerView = _flightView;
            defaultWidgets = "COMMUNICATION_CONSOLE_DOCKWIDGET,UAS_INFO_INFOVIEW_DOCKWIDGET";
            break;

        case VIEW_MISSIONEDITOR:
            _buildMissionEditorView();
            centerView = _missionEditorView;
            break;

        default:
            Q_ASSERT(false);
            break;
    }

    // Remove old view
    if (_currentViewWidget) {
        _currentViewWidget->setVisible(false);
        Q_ASSERT(_centralLayout->count() == 1);
        QLayoutItem *child = _centralLayout->takeAt(0);
        Q_ASSERT(child);
        delete child;
    }

    // Add the new one
    Q_ASSERT(centerView);
    Q_ASSERT(_centralLayout->count() == 0);
    _currentViewWidget = centerView;
    _centralLayout->addWidget(_currentViewWidget);
    _centralLayout->setContentsMargins(0, 0, 0, 0);
    _currentViewWidget->setVisible(true);

#ifndef __mobile__
    // Hide all widgets from previous view
    _hideAllDockWidgets();

    // Restore the widgets for the new view
    QString widgetNames = settings.value(_getWindowStateKey() + "WIDGETS", defaultWidgets).toString();
    qDebug() << widgetNames;
    if (!widgetNames.isEmpty()) {
        QStringList split = widgetNames.split(",");
        foreach (QString widgetName, split) {
            Q_ASSERT(!widgetName.isEmpty());
            _showDockWidget(widgetName, true);
        }
    }
#endif

    if (settings.contains(_getWindowStateKey())) {
        restoreState(settings.value(_getWindowStateKey()).toByteArray());
    }

    // There is a bug in Qt where a Canvas element inside a QQuickWidget does not
    // receive update requests. Here we emit a signal for them to get repainted.
    emit repaintCanvas();
}

void MainWindow::loadAnalyzeView()
{
    if (_currentView != VIEW_ANALYZE)
    {
        _storeCurrentViewState();
        _currentView = VIEW_ANALYZE;
        _ui.actionAnalyze->setChecked(true);
        _loadCurrentViewState();
    }
}

void MainWindow::loadPlanView()
{
    if (_currentView != VIEW_MISSIONEDITOR)
    {
        _storeCurrentViewState();
        _currentView = VIEW_MISSIONEDITOR;
        _ui.actionPlan->setChecked(true);
        _loadCurrentViewState();
    }
}

void MainWindow::loadSetupView()
{
    if (_currentView != VIEW_SETUP)
    {
        _storeCurrentViewState();
        _currentView = VIEW_SETUP;
        _ui.actionSetup->setChecked(true);
        _loadCurrentViewState();
    }
}

void MainWindow::loadFlightView()
{
    if (_currentView != VIEW_FLIGHT)
    {
        _storeCurrentViewState();
        _currentView = VIEW_FLIGHT;
        _ui.actionFlight->setChecked(true);
        _loadCurrentViewState();
    }
}

/// @brief Hides the spash screen if it is currently being shown
void MainWindow::hideSplashScreen(void)
{
    if (_splashScreen) {
        _splashScreen->hide();
        _splashScreen = NULL;
    }
}

void MainWindow::manageLinks()
{
    SettingsDialog settings(this, SettingsDialog::ShowCommLinks);
    settings.exec();
}

/// @brief Saves the last used connection
void MainWindow::saveLastUsedConnection(const QString connection)
{
    QSettings settings;
    QString key(MAIN_SETTINGS_GROUP);
    key += "/LAST_CONNECTION";
    settings.setValue(key, connection);
}

/// @brief Restore (and connects) the last used connection (if any)
void MainWindow::restoreLastUsedConnection()
{
    // TODO This should check and see of the port/whatever is present
    // first. That is, if the last connection was to a PX4 on some serial
    // port, it should check and see if the port is present before making
    // the connection.
    QSettings settings;
    QString key(MAIN_SETTINGS_GROUP);
    key += "/LAST_CONNECTION";
    if(settings.contains(key)) {
        QString connection = settings.value(key).toString();
        // Create a link for it
        LinkManager::instance()->createConnectedLink(connection);
    }
}

void MainWindow::_linkStateChange(LinkInterface*)
{
    emit repaintCanvas();
}

#ifdef QGC_MOUSE_ENABLED_LINUX
bool MainWindow::x11Event(XEvent *event)
{
    emit x11EventOccured(event);
    return false;
}
#endif // QGC_MOUSE_ENABLED_LINUX

#ifdef UNITTEST_BUILD
void MainWindow::_showQmlTestWidget(void)
{
    new QmlTestWidget();
}
#endif
