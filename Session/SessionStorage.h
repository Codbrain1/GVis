#pragma once
#include <QByteArray>
#include <QString>
#include <qstringview.h>
#include <optional>

namespace GVis::Session {
class SessionStorage {
  public:
    static bool                      save(const QString& filePath, const QByteArray& data);
    static std::optional<QByteArray> load(const QString& filePath);
};
} // namespace GVis::Session