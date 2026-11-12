#include "DataTreeModel.h"
#include "Common/Structures/ObjectRegistryStructures.h"
#include "Core/LayerManager/LayerManager.h"
#include "Core/ObjectRegistry/ObjectRegistry.h"
#include "Enums/LayerEnums.h"
#include "Enums/VisualizeBaseEnums.h"

#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QIcon>
#include <cstddef>
#include <memory>
#include <qelapsedtimer.h>
#include <qlogging.h>
#include <qloggingcategory.h>
#include <qnamespace.h>
#include <qtypes.h>
#include <quuid.h>
#include <qvariant.h>

namespace GVis::Models {

// ==========================================
//    РЕАЛИЗАЦИЯ ВСПОМОГАТЕЛЬНОГО DataTreeItem
// ==========================================
DataTreeItem::DataTreeItem(Type type, const QUuid& id, DataTreeItem* parent, Visualize::EntityType subType)
    : m_id(id), m_parent(parent), m_type(type), m_subType(subType) {
}

DataTreeItem* DataTreeItem::parent() const {
    return m_parent;
}

DataTreeItem::Type DataTreeItem::type() const {
    return m_type;
}

QUuid DataTreeItem::id() const {
    return m_id;
}

Visualize::EntityType DataTreeItem::subType() const {
    return m_subType;
} // Геттер для типа компоненты

void DataTreeItem::appendChild(std::unique_ptr<DataTreeItem>&& child) {
    m_children.emplace_back(std::move(child));
}

void DataTreeItem::removeChild(int row) {
    if (row < 0 || row >= static_cast<int>(m_children.size())) {
        return;
    }

    // Итератор на удаляемый элемент
    auto it = m_children.begin() + row;

    // erase удаляет std::unique_ptr из вектора.
    // Деструктор unique_ptr автоматически вызовется и освободит память (delete),
    // выделенную под этот DataTreeItem, что рекурсивно уничтожит и его детей, если они были.
    m_children.erase(it);
}

DataTreeItem* DataTreeItem::child(int row) const {
    return row >= 0 && row < childCount() ? m_children.at(row).get() : nullptr;
}

int DataTreeItem::childCount() const {
    return static_cast<int>(m_children.size());
}

int DataTreeItem::row() const {
    if (!m_parent)
        return 0;
    const auto& siblings = m_parent->m_children;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings.at(i).get() == this) {
            return i;
        }
    }
    return -1;
}

// ==========================================
//    РЕАЛИЗАЦИЯ ОСНОВНОЙ DataTreeModel
// ==========================================

DataTreeModel::DataTreeModel(Core::ObjectRegistry* registry, Core::LayerManager* layerManager, QObject* parent)
    : QAbstractItemModel(parent), m_registry(registry), m_layerManager(layerManager) {
    m_rootItem = std::make_unique<DataTreeItem>(DataTreeItem::Root, QUuid());

    connect(m_registry, &Core::ObjectRegistry::experimentAdded, this, &DataTreeModel::handleExperimentAdded);
    connect(m_registry, &Core::ObjectRegistry::snapshotAdded, this, &DataTreeModel::handleSnapshotAdded);
    connect(m_registry, &Core::ObjectRegistry::nodeAdded, this, &DataTreeModel::handleNodeAdded);

    connect(m_registry, &Core::ObjectRegistry::objectRemoved, this, &DataTreeModel::handleObjectRemoved);
    connect(m_registry, &Core::ObjectRegistry::cleared, this, &DataTreeModel::rebuildTree);

    // Подключаем перерисовку статуса ОЗУ
    connect(m_registry, &Core::ObjectRegistry::nodeDataUpdated, this, &DataTreeModel::refreshNode);
    connect(m_layerManager, &Core::LayerManager::layerCreated, this, &DataTreeModel::handleLayerAdded);
    connect(m_layerManager, &Core::LayerManager::layerRemoved, this, &DataTreeModel::handleLayerRemoved);

    // rebuildTree();
}
void DataTreeModel::handleExperimentAdded(std::shared_ptr<Core::Experiment> exp) {
    if (!exp)
        return;

    // 1. Узнаем, сколько сейчас элементов в корне (это будет индекс новой строки)
    int newRow = m_rootItem->childCount();

    // 2. Говорим QTreeView: "Я сейчас добавлю 1 строку в корень (QModelIndex())"
    beginInsertRows(QModelIndex(), newRow, newRow);

    // 3. Добавляем данные во внутреннюю структуру
    auto expItem = std::make_unique<DataTreeItem>(DataTreeItem::Experiment, exp->id, m_rootItem.get());
    m_itemMap.insert(exp->id, expItem.get());
    m_rootItem->appendChild(std::move(expItem));

    // 4. Говорим QTreeView: "Я закончил, отрисуй новую строчку"
    endInsertRows();
}
void DataTreeModel::handleSnapshotAdded(std::shared_ptr<Core::Snapshot> snapshot, const QUuid& parentExpId) {
    if (!snapshot)
        return;

    // ВАЖНО: Если включен плоский вид, снапшоты как узлы дерева не существуют. Игнорируем.
    if (m_treeMode == TreeMode::ComponentView) {
        return;
    }

    if (!m_itemMap.contains(parentExpId))
        return;

    DataTreeItem* parentItem = m_itemMap.value(parentExpId);
    int           newRow     = parentItem->childCount();

    QModelIndex parentIndex = createIndex(parentItem->row(), 0, parentItem);

    beginInsertRows(parentIndex, newRow, newRow);

    auto snapshotItem = std::make_unique<DataTreeItem>(DataTreeItem::Snapshot, snapshot->id, parentItem);
    m_itemMap.insert(snapshot->id, snapshotItem.get());
    parentItem->appendChild(std::move(snapshotItem));

    endInsertRows();
}

void DataTreeModel::handleNodeAdded(std::shared_ptr<Core::DataNode> node, const QUuid& parentSnapshotId) {
    if (!node)
        return;

    DataTreeItem* parentItem = nullptr;

    // ==========================================================
    // ЛОГИКА ДЛЯ РЕЖИМА "ВРЕМЕННОЙ ВИД" (По Снапшотам)
    // ==========================================================
    if (m_treeMode == TreeMode::SnapShotView) {
        // Ищем родительский снапшот
        parentItem = m_itemMap.value(parentSnapshotId, nullptr);
    }
    // ==========================================================
    // ЛОГИКА ДЛЯ РЕЖИМА "КОМПОНЕНТНЫЙ ВИД" (По типам материи)
    // ==========================================================
    else if (m_treeMode == TreeMode::ComponentView) {
        // 1. Узнаем ID эксперимента. Так как нода знает только свой снапшот,
        // спросим у реестра (или найдем перебором, что тоже очень быстро)
        QUuid targetExpId;
        for (const auto& exp : m_registry->getAllExperiments()) {
            for (const auto& snap : exp->snapshots) {
                if (snap && snap->id == parentSnapshotId) {
                    targetExpId = exp->id;
                    break;
                }
            }
            if (!targetExpId.isNull())
                break;
        }

        if (targetExpId.isNull() || !m_itemMap.contains(targetExpId))
            return;

        // 2. Генерируем детерминированный UUID для группы (Точно так же, как в buildComponentBranch)
        QUuid componentGroupId = QUuid::createUuidV5(targetExpId, QString::number(static_cast<int>(node->type)));

        // 3. Если такой группы (например, "Газ") еще нет в дереве эксперимента — создаем её!
        if (!m_itemMap.contains(componentGroupId)) {
            DataTreeItem* expItem  = m_itemMap.value(targetExpId);
            int           groupRow = expItem->childCount();

            beginInsertRows(createIndex(expItem->row(), 0, expItem), groupRow, groupRow);

            auto groupItem = std::make_unique<DataTreeItem>(DataTreeItem::ComponentGroup, componentGroupId, expItem, node->type);
            m_itemMap.insert(componentGroupId, groupItem.get());
            expItem->appendChild(std::move(groupItem));

            endInsertRows();
        }

        // 4. Родитель найден — это виртуальная группа
        parentItem = m_itemMap.value(componentGroupId);
    }

    // Если родитель так и не найден, выходим
    if (!parentItem)
        return;

    // ==========================================================
    // ОБЩИЙ КОД ВСТАВКИ УЗЛА
    // ==========================================================
    int         newRow      = parentItem->childCount();
    QModelIndex parentIndex = createIndex(parentItem->row(), 0, parentItem);

    beginInsertRows(parentIndex, newRow, newRow);

    // ВАЖНО: передаем node->type в качестве subType
    auto nodeItem = std::make_unique<DataTreeItem>(DataTreeItem::DataNode, node->id, parentItem, node->type);
    m_itemMap.insert(node->id, nodeItem.get());
    parentItem->appendChild(std::move(nodeItem));

    endInsertRows();
}
void DataTreeModel::handleLayerAdded(const QUuid& layerId) {
    auto layer = m_layerManager->getLayer(layerId);
    // Проверяем существование слоя и наличие его родительской ноды в визуальном дереве
    if (!layer || !m_itemMap.contains(layer->dataNodeId())) {
        return;
    }

    // 2. Находим родительский узел данных
    DataTreeItem* parentItem = m_itemMap.value(layer->dataNodeId());

    // Защита: слой всегда должен добавляться только внутрь узла типа DataNode
    if (parentItem->type() != DataTreeItem::DataNode) {
        return;
    }

    // 3. Вычисляем позицию для новой строки (в конец списка детей)
    int newRow = parentItem->childCount();

    // 4. Генерируем QModelIndex для родительского узла
    QModelIndex parentIndex = createIndex(parentItem->row(), 0, parentItem);

    // 5. Оповещаем QTreeView о начале точечной вставки
    beginInsertRows(parentIndex, newRow, newRow);

    // 6. Создаем элемент дерева для слоя и привязываем его
    auto layerItem = std::make_unique<DataTreeItem>(DataTreeItem::LayerItem, layer->layerId(), parentItem);

    // Сохраняем в кэш для быстрого доступа
    m_itemMap.insert(layer->layerId(), layerItem.get());

    // Передаем владение во внутреннюю иерархию (m_children родителя)
    parentItem->appendChild(std::move(layerItem));

    // 7. Завершаем вставку, UI автоматически перерисует только эту ветку
    endInsertRows();
}
void DataTreeModel::handleObjectRemoved(const QUuid& id) {
    // 1. Проверяем, есть ли этот элемент в нашем визуальном дереве.
    // Если его нет (например, это снапшот, а мы в "Плоском виде"), просто игнорируем.
    if (!m_itemMap.contains(id)) {
        return;
    }

    // 2. Получаем удаляемый элемент и его родителя
    DataTreeItem* itemToRemove = m_itemMap.value(id);
    DataTreeItem* parentItem   = itemToRemove->parent();

    if (!parentItem) {
        return; // Защита: m_rootItem удалять нельзя
    }

    // 3. Вычисляем строку (row) удаляемого элемента и QModelIndex его родителя
    int row = itemToRemove->row();

    // Если родитель - это корень дерева, его индекс должен быть пустым QModelIndex()
    QModelIndex parentIndex = (parentItem == m_rootItem.get()) ? QModelIndex() : createIndex(parentItem->row(), 0, parentItem);

    // 4. Оповещаем QTreeView: "Внимание, сейчас исчезнет строка row"
    beginRemoveRows(parentIndex, row, row);

    // 5. ВАЖНО: Рекурсивно вычищаем удаляемый элемент и ВСЕХ его детей из m_itemMap
    // Это нужно сделать ДО физического удаления из памяти
    cleanItemMapRecursively(itemToRemove);

    // 6. Физически уничтожаем объект и убираем его из дерева
    // (метод removeChild мы писали ранее, он вызывает .erase() у std::vector)
    parentItem->removeChild(row);

    // 7. Оповещаем UI, что удаление завершено, можно перерисовывать
    endRemoveRows();
}

// Вспомогательный метод для защиты от "висячих" указателей (dangling pointers)
void DataTreeModel::cleanItemMapRecursively(DataTreeItem* item) {
    if (!item)
        return;

    // Сначала спускаемся на самое дно ветки (чистим детей)
    for (int i = 0; i < item->childCount(); ++i) {
        cleanItemMapRecursively(item->child(i));
    }

    // Затем удаляем текущий элемент из хэш-таблицы
    m_itemMap.remove(item->id());
}

void DataTreeModel::handleLayerRemoved(const QUuid& layerId) {
    if (!m_itemMap.contains(layerId)) {
        return;
    }

    DataTreeItem* layerItem  = m_itemMap.value(layerId);
    DataTreeItem* parentItem = layerItem->parent();

    if (!parentItem) {
        return;
    }

    int         row         = layerItem->row();
    QModelIndex parentIndex = createIndex(parentItem->row(), 0, parentItem);

    // Оповещаем UI об удалении конкретной строки
    beginRemoveRows(parentIndex, row, row);

    // Удаляем из QMap
    m_itemMap.remove(layerId);

    parentItem->removeChild(row);

    endRemoveRows();
}
void DataTreeModel::setTreeMode(TreeMode mode) {
    if (m_treeMode == mode) {
        return;
    }

    m_treeMode = mode;
    rebuildTree(); // Полностью пересобирает иерархию DataTreeItem'ов
    emit treeModeChanged(m_treeMode);
}

void DataTreeModel::rebuildTree() {
    beginResetModel();
    m_itemMap.clear();
    m_rootItem = std::make_unique<DataTreeItem>(DataTreeItem::Root, QUuid());

    auto experiments = m_registry->getAllExperiments();

    // проходим по всем экспериментам
    for (const auto& exp : experiments) {
        if (!exp)
            continue;

        // создаем элемент эксперимента
        auto  expItem    = std::make_unique<DataTreeItem>(DataTreeItem::Experiment, exp->id, m_rootItem.get());
        auto* expItemPtr = expItem.get();
        m_itemMap.insert(exp->id, expItemPtr); // доабвляем указатель в дерево
        m_rootItem->appendChild(std::move(expItem));

        // ----- Два вида иерархии -----
        // 1) Временной: Эксперимент -> Снапшот -> Тип компоненты -> Слой
        if (m_treeMode == TreeMode::SnapShotView) {
            for (const auto& snap : exp->snapshots) {
                buildSnapshotBranch(snap, expItemPtr);
            }
        } // 2) Компонентный: Эксперимент -> Тип компоненты (Газ/Звезды/Темная материя) -> Нода -> Слой
        else if (m_treeMode == TreeMode::ComponentView) {
            // 1. Группируем ноды со всех снапшотов по их физическому типу (EntityType)
            QMap<Visualize::EntityType, QList<std::shared_ptr<Core::DataNode>>> groupedNodesByEntityType;
            for (const auto& snap : exp->snapshots) {
                if (!snap)
                    continue;
                for (const auto& node : snap->components) {
                    if (node) {
                        groupedNodesByEntityType[node->type].append(node);
                    }
                }
            }

            // 2. Создаем ветку для каждой уникальной группы
            for (const auto& [entityType, nodes] : groupedNodesByEntityType.asKeyValueRange()) {
                buildComponentBranch(entityType, nodes, expItemPtr);
            }
        }
    }
    endResetModel();
    emit treeRebuilt();
}

// ====== Создает временную структуру ======
void DataTreeModel::buildSnapshotBranch(std::shared_ptr<Core::Snapshot> snapshot, DataTreeItem* parentItem) {
    if (!snapshot)
        return;

    // 1. Создаем и добавляем элемент контейнера (Снапшота)
    auto  snapshotItem    = std::make_unique<DataTreeItem>(DataTreeItem::Snapshot, snapshot->id, parentItem);
    auto* snapshotItemPtr = snapshotItem.get();
    m_itemMap.insert(snapshot->id, snapshotItemPtr);
    parentItem->appendChild(std::move(snapshotItem));

    // 2. Добавляем ноды (физические компоненты: газ, звезды...)
    for (const auto& node : snapshot->components) {
        if (!node)
            continue;

        buildNodeBranch(node, snapshotItemPtr);
    }
}

// ====== Создает компонентную структуру ======
void DataTreeModel::buildComponentBranch(const Visualize::EntityType&                  entityType,
                                         const QList<std::shared_ptr<Core::DataNode>>& nodes,
                                         DataTreeItem*                                 parentItem) {
    // Генерируем виртуальный UUID для группы, чтобы TreeView мог её идентифицировать
    QUuid componentId      = QUuid::createUuidV5(parentItem->id(), QString::number(static_cast<int>(entityType)));
    auto  componentItem    = std::make_unique<DataTreeItem>(DataTreeItem::ComponentGroup, componentId, parentItem, entityType);
    auto* componentItemPtr = componentItem.get();
    m_itemMap.insert(componentId, componentItemPtr);
    parentItem->appendChild(std::move(componentItem));

    // Добавляем ноды в эту группу
    for (const auto& node : nodes) {
        buildNodeBranch(node, componentItemPtr);
    }
}

// ====== Отображение отдельного узла данных и его представлений (слоев)
void DataTreeModel::buildNodeBranch(std::shared_ptr<Core::DataNode> node, DataTreeItem* parentItem) {
    auto  nodeItem    = std::make_unique<DataTreeItem>(DataTreeItem::DataNode, node->id, parentItem, node->type);
    auto* nodeItemPtr = nodeItem.get();
    m_itemMap.insert(node->id, nodeItemPtr);
    parentItem->appendChild(std::move(nodeItem));

    // 3. Добавляем слои визуализации, которые относятся к этой конкретной ноде
    auto nodeLayers = m_layerManager->getLayersForNode(node->id);
    for (const auto& layer : nodeLayers) {
        if (!layer)
            continue;

        // Используем ID слоя, чтобы потом в методе data() вытащить настройки конкретного окна
        auto  layerItem    = std::make_unique<DataTreeItem>(DataTreeItem::LayerItem, layer->layerId(), nodeItemPtr);
        auto* layerItemPtr = layerItem.get();
        nodeItemPtr->appendChild(std::move(layerItem));
        m_itemMap.insert(layer->layerId(), layerItemPtr);
    }
}

void DataTreeModel::refreshNode(const QUuid& nodeId) {
    if (!m_itemMap.contains(nodeId))
        return;

    DataTreeItem* item        = m_itemMap.value(nodeId);
    QModelIndex   topLeft     = createIndex(item->row(), 0, item);
    QModelIndex   bottomRight = createIndex(item->row(), ColumnCount - 1, item);

    emit dataChanged(topLeft, bottomRight);
}

// --- Навигационные методы дерева Qt ---
QModelIndex DataTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    DataTreeItem* parentItem = parent.isValid() ? static_cast<DataTreeItem*>(parent.internalPointer()) : m_rootItem.get();

    DataTreeItem* childItem = parentItem->child(row);
    if (childItem) {
        return createIndex(row, column, childItem);
    }
    return QModelIndex();
}

QModelIndex DataTreeModel::parent(const QModelIndex& child) const {
    if (!child.isValid())
        return QModelIndex();

    auto*         childItem  = static_cast<DataTreeItem*>(child.internalPointer());
    DataTreeItem* parentItem = childItem->parent();

    if (parentItem == m_rootItem.get() || !parentItem) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

int DataTreeModel::rowCount(const QModelIndex& parent) const {
    if (parent.column() > 0)
        return 0;

    DataTreeItem* parentItem = parent.isValid() ? static_cast<DataTreeItem*>(parent.internalPointer()) : m_rootItem.get();

    return parentItem->childCount();
}

int DataTreeModel::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return ColumnCount;
}

// --- Чтение и вывод данных в UI ---
QVariant DataTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return QVariant();

    auto*                  item   = static_cast<DataTreeItem*>(index.internalPointer());
    DataTreeModel::Columns column = static_cast<DataTreeModel::Columns>(index.column());
    // =========================================================================
    // 0. КАСТОМНЫЕ РОЛИ (Доступ к метаданным для виджетов и сортировки)
    // =========================================================================
    if (role == CustomRoles::IdRole) {
        return item->id(); // Возвращаем UUID элемента напрямую из DataTreeItem
    }

    if (role == CustomRoles::DefaultOrderRole) {
        return item->row(); // Исходный порядок равен индексу строки в родителе
    }

    if (role == CustomRoles::TimestampRole) {
        // Логика получения времени зависит от типа элемента
        if (item->type() == DataTreeItem::Snapshot) {
            auto snapshot = m_registry->getSnapshot(item->id());
            if (snapshot) {
                return snapshot->timestamp;
            }
        }
        return 0; // Для не-снапшотов (или если времени нет) возвращаем 0
    }
    // Добавляем возврат типа:
    if (role == CustomRoles::TypeRole) {
        return static_cast<int>(item->type()); // возвращаем как int
    }
    // =========================================================================
    // 1. РОЛЬ: Состояние чекбокса видимости (Только для первой колонки NameColumn)
    // =========================================================================
    if (role == Qt::CheckStateRole && column == Columns::NameColumn) {
        switch (item->type()) {
            case DataTreeItem::Type::LayerItem: {
                auto layer = m_layerManager->getLayer(item->id());
                return (layer && layer->getSettings()->isVisible()) ? Qt::Checked : Qt::Unchecked;
            }
            case DataTreeItem::Type::DataNode: {
                auto node      = m_registry->getNode(item->id());
                bool isVisible = m_layerManager->isNodeVisible(node->id);
                return (isVisible) ? Qt::Checked : Qt::Unchecked;
            }
            case DataTreeItem::Type::ComponentGroup: {
                return calculateComponentGroupCheckState(item);
            }
            case DataTreeItem::Type::Snapshot: {
                auto snapshot = m_registry->getSnapshot(item->id());
                return calculateSnapshotheckState(snapshot);
            }
            case DataTreeItem::Type::Experiment: {
                auto experiment = m_registry->getExperiment(item->id());
                return calculateExperimentCheckState(experiment);
            }
            default:
                return Qt::Unchecked;
        }
    }
    // =========================================================================
    // 2. РОЛЬ: Отображение текста в ячейках таблицы
    // =========================================================================
    if (role == Qt::DisplayRole) {
        switch (item->type()) {
            case DataTreeItem::Type::LayerItem: {
                auto layer = m_layerManager->getLayer(item->id());
                if (!layer)
                    return QVariant();

                switch (column) {
                    case NameColumn:
                        return QString("Layer: %1").arg(layer->name());
                    // case TypeColumn:
                    //     return "Visualization Layer";
                    default:
                        return "";
                }
            }
            case DataTreeItem::Type::DataNode: {
                auto node = m_registry->getNode(item->id());
                if (!node)
                    return QVariant();
                switch (column) {
                    case DataTreeModel::Columns::NameColumn: {
                        if (m_treeMode == TreeMode::SnapShotView)
                            return entityTypeToString(node->type);
                        else if (m_treeMode == TreeMode::ComponentView)
                            return node->label;
                        else
                            return "ERROR";
                    }
                    // case DataTreeModel::Columns::TypeColumn:
                    //     return entityTypeToString(node->type);
                    case DataTreeModel::Columns::MemoryColumn:
                        return (node->data != nullptr) ? "In RAM" : "Unloaded";
                    case DataTreeModel::Columns::SizeColumn:
                        return QString::number(node->stats.pointCount);
                    default:
                        return "";
                }
            }
            case DataTreeItem::Type::ComponentGroup: {
                switch (column) {
                    case DataTreeModel::Columns::NameColumn:
                        return entityTypeToString(item->subType());
                    // case DataTreeModel::Columns::TypeColumn:
                    //     return "Component collection";
                    case DataTreeModel::SizeColumn:
                        return QString("[%1 nodes]").arg(item->childCount());
                    default:
                        return "";
                }
            }
            case DataTreeItem::Type::Snapshot: {
                auto snapshot = m_registry->getSnapshot(item->id());
                if (!snapshot)
                    return QVariant();
                switch (column) {
                    case DataTreeModel::Columns::NameColumn:
                        return snapshot->name;
                    case DataTreeModel::Columns::SizeColumn:
                        return QString("[%1 components]").arg(snapshot->components.size());
                    default:
                        return "";
                }
            }
            case DataTreeItem::Type::Experiment: {
                auto experiment = m_registry->getExperiment(item->id());
                if (!experiment)
                    return QVariant();
                switch (column) {
                    case DataTreeModel::Columns::NameColumn:
                        return experiment->name;
                    default:
                        return "";
                }
            }
            default:
                return QVariant();
        }
    }

    // =========================================================================
    // 3. РОЛЬ: Стилизация интерфейса (Кастомизация отображения)
    // =========================================================================
    if (role == Qt::ForegroundRole && item->type() == DataTreeItem::DataNode) {
        auto node = m_registry->getNode(item->id());
        // Если данные выгружены LRU-кэшем — приглушаем текст серым цветом
        if (node && !node->isLoaded()) {
            return QColor(Qt::gray);
        }
    } else if (role == Qt::ForegroundRole && item->type() == DataTreeItem::Snapshot) {
        auto snapshot = m_registry->getSnapshot(item->id());
        if (snapshot && !snapshot->isLoaded()) {
            return QColor(Qt::gray);
        }
    }

    // Для всех необработанных ролей возвращаем пустой QVariant (Qt применит дефолты)
    return QVariant();
} // namespace GVis::Models

// --- Обработка клика по чекбоксу (Изменение видимости) ---
bool DataTreeModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.column() != NameColumn)
        return false;

    // =========================================================================
    // ХЕНДЛЕР 1: ОБРАБОТКА ПЕРЕИМЕНОВАНИЯ (Qt::EditRole)
    // =========================================================================

    auto* item = static_cast<DataTreeItem*>(index.internalPointer());

    if (role == Qt::EditRole) {
        QString newName = value.toString().trimmed();
        if (newName.isEmpty())
            return false;

        switch (item->type()) {
            case DataTreeItem::Experiment: {
                if (auto exp = m_registry->getExperiment(item->id())) {
                    exp->name = newName;
                }
                break;
            }
            case DataTreeItem::LayerItem: {
                if (auto layer = m_layerManager->getLayer(item->id())) {
                    layer->setName(newName);
                }
                break;
            }
            default:
                return false;
        }
        // 2. Уведомляем View, что текст ячейки изменился
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});

        return true;
    }

    // =========================================================================
    // ХЕНДЛЕР 2: ОБРАБОТКА КЛИКА ПО ЧЕКБОКСУ (Qt::CheckStateRole)
    // =========================================================================
    if (role != Qt::CheckStateRole)
        return false; // Защита от всех остальных ролей
    const bool visible = (value.toInt() == Qt::Checked);

    // --- 1. Бизнес-логика и ленивое уведомление Слоев ---
    switch (item->type()) {
        case DataTreeItem::Experiment: {
            if (auto exp = m_registry->getExperiment(item->id())) {
                for (const auto& snap : exp->snapshots) {
                    m_layerManager->setSnapshotVisibility(snap, visible);
                }
            }
            break;
        }
        case DataTreeItem::ComponentGroup: {
            const int totalChildren = item->childCount();
            for (int i = 0; i < totalChildren; ++i) {
                DataTreeItem* child = item->child(i);
                if (child->type() == DataTreeItem::DataNode) {
                    if (auto node = m_registry->getNode(child->id())) {
                        m_layerManager->setNodeVisibility(node->id, visible);
                    }
                }
            }
            break;
        }
        case DataTreeItem::Snapshot: {
            if (auto container = m_registry->getSnapshot(item->id())) {
                m_layerManager->setSnapshotVisibility(container, visible);
            }
            break;
        }
        case DataTreeItem::DataNode: {
            if (auto node = m_registry->getNode(item->id())) {
                m_layerManager->setNodeVisibility(node->id, visible);
            }
            break;
        }
        case DataTreeItem::LayerItem: {
            if (auto layer = m_layerManager->getLayer(item->id())) {
                layer->setVisible(visible);
            }
            break;
        }
        default:
            return false;
    }

    // --- 2. Уведомление самого нажатого узла ---
    emit dataChanged(index, index, {Qt::CheckStateRole});

    // --- 3. ВОЛНА ВНИЗ: Уведомляем всех потомков (детей, внуков...) ---
    auto notifyDescendants = [&](auto& self, const QModelIndex& parentIdx) -> void {
        const int rows = this->rowCount(parentIdx);
        if (rows == 0)
            return;

        QModelIndex topLeft     = this->index(0, NameColumn, parentIdx);
        QModelIndex bottomRight = this->index(rows - 1, NameColumn, parentIdx);
        emit        dataChanged(topLeft, bottomRight, {Qt::CheckStateRole});

        for (int i = 0; i < rows; ++i) {
            QModelIndex childIdx = this->index(i, 0, parentIdx);
            self(self, childIdx);
        }
    };

    notifyDescendants(notifyDescendants, index);

    // --- 4. ВОЛНА ВВЕРХ (НОВОЕ): Уведомляем всех предков (родителя, прародителя...) ---
    QModelIndex currentParent = index.parent();
    while (currentParent.isValid()) {
        // Так как чекбоксы живут строго в NameColumn, нам нужно получить правильный индекс
        // для этой колонки на уровне родителя
        QModelIndex parentNameIdx = this->index(currentParent.row(), NameColumn, currentParent.parent());

        // Говорим QTreeView: "Перерисуй этот родительский чекбокс, его дети изменились"
        emit dataChanged(parentNameIdx, parentNameIdx, {Qt::CheckStateRole});

        // Шагаем еще выше по дереву к следующему родителю
        currentParent = currentParent.parent();
    }
    emit sceneUpdateRequested();

    return true;
}

// --- Оформление шапки таблицы ---

QVariant DataTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case NameColumn:
                return "Name";
            // case TypeColumn:
            //     return "Physical Type";
            case MemoryColumn:
                return "RAM Status";
            case SizeColumn:
                return "Particles Count";
        }
    }
    return QVariant();
}

Qt::ItemFlags DataTreeModel::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    // Делаем интерактивным чекбокс только в первой колонке
    if (index.column() == NameColumn) {
        flags |= Qt::ItemIsUserCheckable;
    }
    if (index.data(CustomRoles::IdRole).isValid()) {
        auto* item = static_cast<DataTreeItem*>(index.internalPointer());
        if (item->type() == DataTreeItem::Experiment || item->type() == DataTreeItem::LayerItem)
            flags |= Qt::ItemIsEditable;
    }
    return flags;
}

// --- Внутренние утилиты калькуляции состояний ---
Qt::CheckState DataTreeModel::calculateComponentGroupCheckState(DataTreeItem* groupItem) const {
    if (!groupItem || groupItem->childCount() == 0)
        return Qt::Unchecked;

    int total        = groupItem->childCount();
    int visibleCount = 0;

    for (int i = 0; i < total; ++i) {
        DataTreeItem* child = groupItem->child(i);
        if (child->type() == DataTreeItem::DataNode) {
            auto node = m_registry->getNode(child->id());
            if (node) {
                if (m_layerManager->isNodeVisible(node->id)) {
                    visibleCount++;
                }
            }
        }
        // calculateComponentGroupCheckState(child);
    }

    if (visibleCount == 0)
        return Qt::Unchecked;
    if (visibleCount == total)
        return Qt::Checked;
    return Qt::PartiallyChecked;
}

Qt::CheckState DataTreeModel::calculateExperimentCheckState(std::shared_ptr<Core::Experiment> exp) const {
    // Этот метод универсален и работает независимо от текущего m_treeMode,
    // так как опрашивает сам реестр, а не визуальное дерево.
    if (!exp)
        return Qt::Unchecked;

    int total        = 0;
    int visibleCount = 0;

    for (const auto& snap : exp->snapshots) {
        if (!snap)
            continue;
        for (const auto& node : snap->components) {
            if (!node)
                continue;
            total++;
            if (m_layerManager->isNodeVisible(node->id)) {
                visibleCount++;
            }
        }
    }

    if (total == 0 || visibleCount == 0)
        return Qt::Unchecked;
    if (visibleCount == total)
        return Qt::Checked;
    return Qt::PartiallyChecked;
}

Qt::CheckState DataTreeModel::calculateSnapshotheckState(std::shared_ptr<Core::Snapshot> container) const {
    if (!container || container->components.isEmpty()) {
        return Qt::Unchecked;
    }

    int total        = 0;
    int visibleCount = 0;

    // Считаем только атомарные ноды (Газ, ТМ, Звезды) внутри данного снапшота
    for (const auto& node : container->components) {
        if (!node)
            continue;

        total++;
        if (m_layerManager->isNodeVisible(node->id)) {
            visibleCount++;
        }
    }

    // Финальный расчет состояния чекбокса для снапшота
    if (total == 0 || visibleCount == 0) {
        return Qt::Unchecked;
    }
    if (visibleCount == total) {
        return Qt::Checked;
    }

    return Qt::PartiallyChecked;
}

QString DataTreeModel::entityTypeToString(Visualize::EntityType type) const {
    // Маппинг вашего перечисления EntityType (Газ, Звезды, Темная Материя) в красивый текст
    switch (type) {
        case Visualize::EntityType::Gas:
            return "Gas / SPH";
        case Visualize::EntityType::Stars:
            return "Stars";
        case Visualize::EntityType::DarkMatter:
            return "Dark Matter";
        default:
            return "Unknown";
    }
}

} // namespace GVis::Models