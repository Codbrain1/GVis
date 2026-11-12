#include "TaskManager.h"
#include <qminmax.h>
#include <qobject.h>
#include <qthread.h>

namespace GVis::Core {
TaskManager::TaskManager(QObject* parent) : QObject(parent) {
    m_compute_pool.setMaxThreadCount(qMax(2, QThread::idealThreadCount() - 1));
    m_io_pool_batch.setMaxThreadCount(4); // TODO: на слабопроцессорных ПК может работать плохо
    m_io_pool_urgent.setMaxThreadCount(2);
}

TaskManager::~TaskManager() {
    cancelAll();
}

void TaskManager::cancelAll() {
    m_compute_pool.clear();
    m_io_pool_batch.clear();
    m_io_pool_urgent.clear();
    m_compute_pool.waitForDone();
    m_io_pool_batch.waitForDone();
    m_io_pool_urgent.waitForDone();
}
} // namespace GVis::Core