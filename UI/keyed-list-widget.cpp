#include "keyed-list-widget.hpp"

#include <Qt>
#include <QDialog>
#include <QMessageBox>

void KeyedListWidget::AddNewItem(const QString &alias, int key) {
    QListWidgetItem* service = new QListWidgetItem(alias, this);
    service->setData(Qt::UserRole, key);
    addItem(service);
    setCurrentItem(service);
}

void KeyedListWidget::UpdateItemName(const QString &alias) {
    currentItem()->setText(alias);
}

void KeyedListWidget::RemoveItem() {
    
    if (currentRow() != -1) {
        // remove service
        QListWidgetItem* service = currentItem();
        int removedItemKey = service->data(Qt::UserRole).toInt();
        takeItem(row(service));
        
        int newItemKey = -1;
        
        if (count() != 0)
            newItemKey = currentItem()->data(Qt::UserRole).toInt();

        emit RemovedKey(removedItemKey, newItemKey);
    }
    else {
        // show failure
        QString message = count() == 0 ? "There are no saved streaming services." : "No services selected.";

        QMessageBox* failureNotice = new QMessageBox(this);
        failureNotice->setIcon(QMessageBox::Warning);
        failureNotice->setWindowModality(Qt::WindowModal);
        failureNotice->setWindowTitle("Notice");
        failureNotice->setText(message);
        failureNotice->exec();
    }
}

void KeyedListWidget::ScrollUp() {
    if (count() == 0)
        return;

    int currentIndex = currentRow();
    
    if (currentIndex > 0) {
        setCurrentRow(currentIndex - 1);
        emit SelectedServiceKey(currentItem()->data(Qt::UserRole).toInt());
    }
}

void KeyedListWidget::ScrollDown() {
    if (count() == 0)
        return;

    int currentIndex = currentRow();
      
    if (currentIndex < count() - 1) {
        setCurrentRow(currentIndex + 1);
        emit SelectedServiceKey(currentItem()->data(Qt::UserRole).toInt());
    }
}

void KeyedListWidget::SelectionChanged(QListWidgetItem* current) {
    emit SelectedServiceKey(current->data(Qt::UserRole).toInt());
}