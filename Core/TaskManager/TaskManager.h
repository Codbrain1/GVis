#pragma once
#include "Common/Enums/CoreEnums.h"
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QtConcurrent/qtconcurrentmap.h>
#include <qfuture.h>
#include <qobject.h>
#include <qthreadpool.h>
#include <qtmetamacros.h>
#include <type_traits>
#include <utility>

namespace GVis::Core {
class TaskManager : public QObject {
    Q_OBJECT
  public:
    explicit TaskManager(QObject* parent = nullptr);
    ~TaskManager();

    template <typename Function, typename... Args>
    auto runCompute(Function&& func, Args&&... args)
        -> QFuture<std::invoke_result_t<Function, Args...>> {
        return QtConcurrent::run(&m_compute_pool,
                                 std::forward<Function>(func),
                                 std::forward<Args>(args)...);
    }

    template <typename Function, typename... Args>
    auto runIO(TaskPriority priority, Function&& func, Args&&... args)
        -> QFuture<std::invoke_result_t<Function, Args...>> {
        if (priority == TaskPriority::Urgent) {
            return QtConcurrent::run(&m_io_pool_urgent,
                                     std::forward<Function>(func),
                                     std::forward<Args>(args)...);
        } else {
            return QtConcurrent::run(&m_io_pool_batch,
                                     std::forward<Function>(func),
                                     std::forward<Args>(args)...);
        }
    }

    template <typename T, typename MapFunction>
    auto mapIO(const QList<T>& sequence, MapFunction&& func) {
        return QtConcurrent::mapped(&m_io_pool_batch, sequence, std::forward<MapFunction>(func));
    }

    void cancelAll();

  private:
    QThreadPool m_compute_pool;
    QThreadPool m_io_pool_batch;
    QThreadPool m_io_pool_urgent;
};
} // namespace GVis::Core