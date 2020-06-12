#include "service-settings.hpp"

#include <QWidget>
#include <QString>
#include <QStackedWidget>
#include <QMap>
#include <QList>
#include <QLabel>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QFormLayout>

void ServiceSettingsPage::buildSettings() {

    QWidget* page = new QWidget(this);
    QVBoxLayout* pageLayout = new QVBoxLayout;
    QFormLayout* defaultSettingsLayout = new QFormLayout;
    QLabel* nameInputLabel = new 

    QFormLayout* basicInfoForm = new QFormLayout();
    QLabel* name = new QLabel(properties.value("name"));
    
    //QLabel* id = new QLabel(properties.value("id"));
    
    layout->addWidget(name);
    layout->addWidget(id);

    box->setStyleSheet("background-color:white;");

    box->setMinimumWidth(400);
    box->setMinimumHeight(400);

    box->setLayout(layout);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ServiceSettingsStackWidget::addService() {
    
   QMap<QString, QString>* props = new QMap<QString, QString>();
   props->insert("name", "Service A");
   props->insert("id", "1"); 
   
   ServiceSettingsPage* testService = new ServiceSettingsPage(*props);
   
   QString name("name");
   QString id("id");

   name = props->value(name);

   addWidget(testService);
   setCurrentWidget(testService);
   emit serviceModified(&name, props->value(id).toInt());
}

void ServiceSettingsStackWidget::removeService() {
    // implement
    ;
}

void ServiceSettingsStackWidget::scrollToSettings(int id) {
    // implement
    ;
}