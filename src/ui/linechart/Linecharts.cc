#include <QShowEvent>

#include "Linecharts.h"
#include "MultiVehicleManager.h"
#include "MainWindow.h"
#include "UAS.h"

Linecharts::Linecharts(QWidget *parent) :
    QStackedWidget(parent),
    plots(),
    active(true)
{
    this->setVisible(false);
    
    // Add each MAV
    foreach (Vehicle* vehicle, MultiVehicleManager::instance()->vehicles()) {
        addVehicle(vehicle);
    }
    connect(MultiVehicleManager::instance(), &MultiVehicleManager::vehicleAdded, this, &Linecharts::addVehicle);
}

void Linecharts::showEvent(QShowEvent* event)
{
    // React only to internal (pre-display)
    // events
    Q_UNUSED(event)
    QWidget* prevWidget = currentWidget();
    if (prevWidget)
    {
        LinechartWidget* chart = dynamic_cast<LinechartWidget*>(prevWidget);
        if (chart) {
            this->active = true;
            chart->setActive(true);
        }
    }
    QWidget::showEvent(event);
    emit visibilityChanged(true);
}

void Linecharts::hideEvent(QHideEvent* event)
{
    // React only to internal (pre-display)
    // events
    Q_UNUSED(event)
    QWidget* prevWidget = currentWidget();
    if (prevWidget) {
        LinechartWidget* chart = dynamic_cast<LinechartWidget*>(prevWidget);
        if (chart) {
            this->active = false;
            chart->setActive(false);
        }
    }
    QWidget::hideEvent(event);
    emit visibilityChanged(false);
}

void Linecharts::addVehicle(Vehicle* vehicle)
{
    UAS* uas = vehicle->uas();
    
    // FIXME Add removeSystem() call

    // Compatibility hack
    int uasid = 0; /*uas->getUASID()*/
    if (!plots.contains(uasid))
    {
        LinechartWidget* widget = new LinechartWidget(uasid, this);
        addWidget(widget);
        plots.insert(uasid, widget);
		
		// Connect valueChanged signals
        connect(uas, SIGNAL(valueChanged(int,QString,QString,QVariant,quint64)), widget, SLOT(appendData(int,QString,QString,QVariant,quint64)));

        connect(widget, SIGNAL(logfileWritten(QString)), this, SIGNAL(logfileWritten(QString)));
        // Set system active if this is the only system
//        if (active)
//        {
//            if (plots.size() == 1)
//            {
                // FIXME XXX HACK
                // Connect generic sources
                for (int i = 0; i < genericSources.count(); ++i)
                {
                    connect(genericSources[i], SIGNAL(valueChanged(int,QString,QString,QVariant,quint64)), plots.values().first(), SLOT(appendData(int,QString,QString,QVariant,quint64)));
                }
                // Select system
                widget->setActive(true);
                //widget->selectActiveSystem(0);
            }
//        }
//    }
}

void Linecharts::addSource(QObject* obj)
{
    genericSources.append(obj);
    // FIXME XXX HACK
    if (plots.size() > 0)
    {
        // Connect generic source
        connect(obj, SIGNAL(valueChanged(int,QString,QString,QVariant,quint64)), plots.values().first(), SLOT(appendData(int,QString,QString,QVariant,quint64)));
    }
}
