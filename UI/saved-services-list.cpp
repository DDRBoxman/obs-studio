#include "saved-services-list.hpp"

#include <Qt>
// explicit ServiceListItem::ServiceListItem(int id = 0, const QString *alias, QWidget* parent = nullptr) : QListWidgetItem(parent) {
//     myId = id;
//     myAlias = *alias;
// }

void SavedServicesList::AddNewService(QString* alias, int id) {
    QListWidgetItem* service = new QListWidgetItem(*alias, this);
    service->setData(Qt::UserRole, id);
    addItem(service);
}

void SavedServicesList::RemoveService() {
    
    QListWidgetItem* service = currentItem();
    int removedItemId = service->data(Qt::UserRole).toInt();
    takeItem(row(service));
    emit removedID(removedItemId);
}

void SavedServicesList::ScrollUp() {
    int currentIndex = currentRow();
    if (currentIndex >= 1)
        setCurrentRow(currentIndex - 1);
    else
        setCurrentRow(0);
}

void SavedServicesList::ScrollDown() {
    int currentIndex = currentRow();
    if (currentIndex < count())
        setCurrentRow(currentIndex + 1);
    else
        setCurrentRow(0);
}