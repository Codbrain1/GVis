#include "ColorMapEditorDialog.h"
#include "ui_ColorMapEditorDialog.h"
#include <QColorDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QUuid>
#include <qfiledialog.h>
#include <qpushbutton.h>
#include <qtablewidget.h>

namespace QSpace::UI {

ColorMapEditorDialog::ColorMapEditorDialog(const Visualize::ColorMap& baseMap,
                                           Mode                              mode,
                                           QWidget*                          parent)
    : QDialog(parent), ui(new Ui::ColorMapEditorDialog), m_baseMap(baseMap), m_mode(mode) {
    ui->setupUi(this);

    // Связываем стандартные кнопки Ok и Cancel с закрытием диалога
    // QDialog::accept вернет QDialog::Accepted при вызове exec()
    // QDialog::reject вернет QDialog::Rejected
    connect(ui->pushButtonOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->pushButtonCancel, &QPushButton::clicked, this, &QDialog::reject);

    // Разрешаем редактировать таблицу только по двойному клику
    ui->tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked);

    // Обработка двойного клика для изменения цвета
    connect(ui->tableWidget,
            &QTableWidget::cellDoubleClicked,
            this,
            &ColorMapEditorDialog::on_colorTable_cellDoubleClicked);

    connect(ui->pushButtonAdd, &QPushButton::clicked, this, &ColorMapEditorDialog::on_addButton_clicked);
    connect(ui->pushButtonDelete,
            &QPushButton::clicked,
            this,
            &ColorMapEditorDialog::on_removeButton_clicked);
    connect(ui->pushButtonLoad, &QPushButton::clicked, this, &ColorMapEditorDialog::on_loadButton_clicked);
    connect(ui->pushButtonSave, &QPushButton::clicked, this, &ColorMapEditorDialog::on_saveButton_clicked);
    connect(ui->checkBoxIsInvert, &QCheckBox::clicked, this, &ColorMapEditorDialog::on_invertButton_clicked);
    connect(ui->spinboxLevels, &QSpinBox::valueChanged, this, &ColorMapEditorDialog::on_spinBoxChanged);

    populateTable();
}

ColorMapEditorDialog::~ColorMapEditorDialog() {
    delete ui;
}

void ColorMapEditorDialog::on_addButton_clicked() {
    // 1. Определяем индекс вставки.
    // Если ничего не выбрано, вставляем в конец. Если выбрано - на место текущей строки.
    int currentRow = ui->tableWidget->currentRow();
    int targetRow  = (currentRow >= 0) ? currentRow + 1 : ui->tableWidget->rowCount();

    // 2. Вставляем пустую строку
    ui->tableWidget->insertRow(targetRow);

    // 3. Подготавливаем значения по умолчанию
    double value = 0.0;
    QColor color = Qt::white;

    // Пытаемся взять данные из "соседней" строки, чтобы сдела`ть интерполяцию или дубликат
    // Берем строку выше (targetRow - 1), если она существует
    if (targetRow > 0) {
        QTableWidgetItem* prevValueItem = ui->tableWidget->item(targetRow - 1, 0);
        QTableWidgetItem* prevColorItem = ui->tableWidget->item(targetRow - 1, 1);

        if (prevValueItem) {
            value = prevValueItem->text().toDouble() + 0.1;
        }
        if (prevColorItem) {
            color = prevColorItem->background().color();
        }
        // Если мы вставили строку не в самый конец, берем значение из строки НИЖЕ
        // (она теперь на индексе insertIndex + 1) и считаем среднее арифметическое.
        if (targetRow < ui->tableWidget->rowCount() - 1) {
            QTableWidgetItem* nextValueItem = ui->tableWidget->item(targetRow + 1, 0);
            if (nextValueItem) {
                double nextValue = nextValueItem->text().toDouble();
                double prevValue = prevValueItem->text().toDouble();
                value            = (prevValue + nextValue) / 2.0;
            }
        }
    }

    // 4. Создаем и устанавливаем новые элементы (items)
    // Колонка 0: Значение
    auto* valueItem = new QTableWidgetItem(QString::number(value, 'f', 6));
    ui->tableWidget->setItem(targetRow, 0, valueItem);

    // Колонка 1: Цвет
    auto* colorItem = new QTableWidgetItem();
    colorItem->setBackground(color);
    // Делаем ячейку нередактируемой текстово, но оставляем возможность выбора
    colorItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->tableWidget->setItem(targetRow, 1, colorItem);

    // 5. Улучшаем UX: выделяем новую строку и прокручиваем к ней
    ui->tableWidget->setCurrentCell(targetRow, 0);

    ui->spinboxLevels->blockSignals(true);
    ui->spinboxLevels->setValue(ui->tableWidget->rowCount());
    ui->spinboxLevels->blockSignals(false);
}
void ColorMapEditorDialog::on_loadButton_clicked() {
    QString colorMapPath = QFileDialog::getOpenFileName(this, "Please variable colormap file", "", "*.json");
    if (colorMapPath.isEmpty()) {
        return;
    }
    emit loadPaletteRequested(colorMapPath);
}
void ColorMapEditorDialog::onPaletteLoaded(QSpace::Visualize::ColorMap colorMap) {
    m_baseMap = colorMap;
    populateTable();
}
void ColorMapEditorDialog::on_saveButton_clicked() {
    Visualize::ColorMap editedMap = getEditedMap();

    QString savePath = QFileDialog::getSaveFileName(this, "Save colormap file", "", "*.json");
    if (savePath.isEmpty()) {
        return;
    }
    emit savePaletteRequested(editedMap, savePath);
}
void ColorMapEditorDialog::on_removeButton_clicked() {
    auto selectedItems = ui->tableWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Удаление", "Пожалуйста, выберите строку для удаления.");
        return;
    }
    QSet<int> rowsToRemove;
    for (auto* item : selectedItems) {
        rowsToRemove.insert(item->row());
    }
    QList<int> sortedRows = rowsToRemove.values();
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());

    for (int row : sortedRows) {
        ui->tableWidget->removeRow(row);
    }
    ui->spinboxLevels->blockSignals(true);
    ui->spinboxLevels->setValue(ui->tableWidget->rowCount());
    ui->spinboxLevels->blockSignals(false);
}
void ColorMapEditorDialog::on_colorTable_cellDoubleClicked(int row, int column) {
    if (column == 1) { // Кликнули по колонке с цветом
        QColor currentColor = ui->tableWidget->item(row, column)->background().color();
        QColor newColor     = QColorDialog::getColor(currentColor, this, "Выберите цвет");

        if (newColor.isValid()) {
            ui->tableWidget->item(row, column)->setBackground(newColor);
        }
    }
}
void ColorMapEditorDialog::populateTable() {
    ui->tableWidget->setRowCount(0);

    for (const auto& pt : m_baseMap.points) {
        int row = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row);

        // Уровень (число)
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(pt.x, 'f', 4)));

        // Цвет (фон ячейки)
        auto* colorItem = new QTableWidgetItem();
        colorItem->setBackground(QColor::fromRgbF(pt.r, pt.g, pt.b));
        // Запрещаем ввод текста в ячейку с цветом
        colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(row, 1, colorItem);
    }
}

Visualize::ColorMap ColorMapEditorDialog::getEditedMap() const {
    Visualize::ColorMap newMap = m_baseMap;
    if (m_mode == Mode::Create) {
        newMap.id   = QUuid::createUuid();
        newMap.name = "Custom_" + newMap.id.toString().mid(1, 4);
    }
    newMap.isPreset = false;
    // Собираем данные из таблицы
    newMap.points.clear();
    for (int i = 0; i < ui->tableWidget->rowCount(); ++i) {
        double x = ui->tableWidget->item(i, 0)->text().toDouble();
        QColor c = ui->tableWidget->item(i, 1)->background().color();
        newMap.points.push_back({x, c.redF(), c.greenF(), c.blueF()});
    }

    // Сортируем по 'x', так как для рендера точки должны идти по возрастанию
    std::sort(newMap.points.begin(), newMap.points.end(), [](const auto& a, const auto& b) {
        return a.x < b.x;
    });

    return newMap;
}
void ColorMapEditorDialog::on_invertButton_clicked() {
    int rowCount = ui->tableWidget->rowCount();
    if (rowCount <= 1)
        return;

    // 1. Собираем все цвета по порядку
    QList<QColor> colors;
    for (int i = 0; i < rowCount; ++i) {
        QTableWidgetItem* colorItem = ui->tableWidget->item(i, 1);
        if (colorItem) {
            colors.append(colorItem->background().color());
        } else {
            colors.append(Qt::white); // fallback
        }
    }

    // 2. Переворачиваем список цветов
    std::reverse(colors.begin(), colors.end());

    // 3. Записываем цвета обратно в таблицу
    for (int i = 0; i < rowCount; ++i) {
        QTableWidgetItem* colorItem = ui->tableWidget->item(i, 1);
        if (colorItem) {
            colorItem->setBackground(colors[i]);
        }
    }
}
void ColorMapEditorDialog::on_spinBoxChanged(double value) {
    int newLevelCount   = static_cast<int>(value);
    int currentRowCount = ui->tableWidget->rowCount();

    // Блокируем перерисовку таблицы на время массовых изменений, чтобы интерфейс не мерцал
    ui->tableWidget->setUpdatesEnabled(false);

    // Добавляем или удаляем строки
    if (newLevelCount > currentRowCount) {
        int rowsToAdd = newLevelCount - currentRowCount;
        for (int i = 0; i < rowsToAdd; ++i) {
            int insertIndex = ui->tableWidget->rowCount();
            ui->tableWidget->insertRow(insertIndex);

            // Копируем цвет из последней строки (чтобы не было резкого перехода в белый)
            QColor color = Qt::white;
            if (insertIndex > 0) {
                QTableWidgetItem* prevColorItem = ui->tableWidget->item(insertIndex - 1, 1);
                if (prevColorItem)
                    color = prevColorItem->background().color();
            }

            // Значение (Value) пока ставим 0, мы его перезапишем шагом ниже
            auto* valueItem = new QTableWidgetItem("0.0");
            ui->tableWidget->setItem(insertIndex, 0, valueItem);

            auto* colorItem = new QTableWidgetItem();
            colorItem->setBackground(color);
            colorItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            ui->tableWidget->setItem(insertIndex, 1, colorItem);
        }
    } else if (newLevelCount < currentRowCount) {
        int rowsToRemove = currentRowCount - newLevelCount;
        for (int i = 0; i < rowsToRemove; ++i) {
            ui->tableWidget->removeRow(ui->tableWidget->rowCount() - 1);
        }
    }

    // Автоматическое распределение значений от 0.0 до 1.0
    int finalRowCount = ui->tableWidget->rowCount();
    if (finalRowCount > 1) {
        for (int i = 0; i < finalRowCount; ++i) {
            // Считаем долю: для 3 строк это будет 0.0, 0.5, 1.0
            double distributedValue = static_cast<double>(i) / (finalRowCount - 1);

            QTableWidgetItem* item = ui->tableWidget->item(i, 0);
            if (item) {
                item->setText(QString::number(distributedValue, 'f', 6));
            }
        }
    } else if (finalRowCount == 1) {
        // Если осталась всего одна строка, логично сделать её нулем
        QTableWidgetItem* item = ui->tableWidget->item(0, 0);
        if (item)
            item->setText(QString::number(0.0, 'f', 6));
    }

    // Включаем перерисовку обратно
    ui->tableWidget->setUpdatesEnabled(true);
}

} // namespace QSpace::UI