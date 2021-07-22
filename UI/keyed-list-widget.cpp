#include "keyed-list-widget.hpp"

#include <Qt>
#include <QMessageBox>

void KeyedListWidget::AddNewItem(const QString &alias, int key)
{
	auto prev = findItems(alias, Qt::MatchExactly);
	for (auto &it : prev) {
		// Duplicate item present.
		if (it->data(Qt::UserRole) == key)
			return;
	}

	QString name = alias;
	QListWidgetItem *item = new QListWidgetItem(name, this);
	item->setData(Qt::UserRole, key);
	addItem(item);
	setCurrentItem(item);
}

void KeyedListWidget::UpdateItemName(const QString &alias)
{
	currentItem()->setText(alias);
}

void KeyedListWidget::RemoveItem()
{
	if (currentRow() == NONE_SELECTED) {
		QString message = count() == 0 ? "There are no saved items."
					       : "No items selected.";

		QMessageBox *failureNotice = new QMessageBox(this);
		failureNotice->setIcon(QMessageBox::Warning);
		failureNotice->setWindowModality(Qt::WindowModal);
		failureNotice->setWindowTitle("Notice");
		failureNotice->setText(message);
		failureNotice->exec();
	}

	// remove item
	QListWidgetItem *item = currentItem();
	int removedItemKey = item->data(Qt::UserRole).toInt();
	takeItem(row(item));

	int newItemKey = -1;

	if (count() != 0)
		newItemKey = currentItem()->data(Qt::UserRole).toInt();

	delete item;
	emit RemovedKey(removedItemKey, newItemKey);
}

void KeyedListWidget::ScrollUp()
{
	if (count() == 0)
		return;

	int currentIndex = currentRow();

	if (currentIndex > 0) {
		setCurrentRow(currentIndex - 1);
		emit ItemClicked(currentItem()->data(Qt::UserRole).toInt());
	}
}

void KeyedListWidget::ScrollDown()
{
	if (count() == 0)
		return;

	int currentIndex = currentRow();

	if (currentIndex < count() - 1) {
		setCurrentRow(currentIndex + 1);
		emit ItemClicked(currentItem()->data(Qt::UserRole).toInt());
	}
}

void KeyedListWidget::SelectionChanged(QListWidgetItem *current)
{
	emit ItemClicked(current->data(Qt::UserRole).toInt());
}