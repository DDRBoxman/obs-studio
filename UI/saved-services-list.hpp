#pragma once

#include <QString>
#include <QWidget>
#include <QList>
#include <QListWidget>
#include <QListWidgetItem>
#include <QAbstractItemView>

// class ServiceListItem : public QListWidgetItem {
    
//     //friend class SavedServicesList;

//     public:
//         // ServiceListItem(QWidget* parent = nullptr) : QListWidgetItem(parent) {;}
//         ServiceListItem(int id, QString alias, QListWidget* parent = nullptr) : QListWidgetItem(alias, parent) { myId = id; }
//         virtual ~ServiceListItem() { ; }
//         int getId() const { return myId; }
//         QString getAlias() const { return myAlias; }
//         void setAlias(QString name) { myAlias = name; }
//     private:
//         int myId;
//         QString myAlias;
// };

class SavedServicesList : public QListWidget {
    Q_OBJECT

    public:
        SavedServicesList(QWidget *parent = nullptr) : QListWidget(parent) { ; }
        virtual ~SavedServicesList() { ;}
    public slots:
        void AddNewService(QString* alias, int id);
        void RemoveService();
        void ScrollUp();
        void ScrollDown();

    signals:
        void removedID(int itemID);
};
