#pragma once

#include <QWidget>
#include <QString>
#include <QStackedWidget>
#include <QMap>
#include <QList>

class ServiceSettingsPage : public QWidget {
    Q_OBJECT

    public:
        ServiceSettingsPage(QWidget* parent = nullptr) : QWidget(parent) { ; }
        ServiceSettingsPage(QMap<QString, QString> props, QWidget* parent = nullptr) : QWidget(parent), properties(props) { buildSettings(); }
        virtual ~ServiceSettingsPage() { ; }

    signals:
        void serviceNameUpdated(QString* alias, int id);
        void serviceAdded(QString* alias, int id);
    private:
        QMap<QString, QString> properties;
        void buildSettings();
};

class ServiceSettingsStackWidget : public QStackedWidget {
    Q_OBJECT

    public:
        ServiceSettingsStackWidget(QWidget* parent = nullptr) : QStackedWidget(parent) { loadSavedServices(); }
        virtual ~ServiceSettingsStackWidget() { ; }
    public slots:
        void addService();
        void removeService();
        void scrollToSettings(int id);
    signals:
        void serviceModified(QString* alias, int id);
    private:
        QList<QMap<QString, QString>> services;
        int count = 0;
        void loadSavedServices() { ; }
};