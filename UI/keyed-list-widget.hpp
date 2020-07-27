#pragma once

#include <QString>
#include <QWidget>
#include <QList>
#include <QListWidget>
#include <QListWidgetItem>
#include <QAbstractItemView>

#define NONE_SELECTED -1

class KeyedListWidget : public QListWidget {
       Q_OBJECT

public:
        KeyedListWidget(QWidget *parent = nullptr) : QListWidget(parent) {
            QObject::connect(this, SIGNAL(itemClicked(QListWidgetItem*)),
                this, SLOT(SelectionChanged(QListWidgetItem*)));
        }
        virtual ~KeyedListWidget() {}
public slots:
        void AddNewItem(const QString &alias, int key);
        void UpdateItemName(const QString &alias);
        void RemoveItem();
        void ScrollUp();
        void ScrollDown();

private slots:
        void SelectionChanged(QListWidgetItem* current);

signals:
        void RemovedKey(int removedKey, int currentKey);
        void ItemClicked(int key);
};
