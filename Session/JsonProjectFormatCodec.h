#pragma once

#include "Common/Interfaces/IProjectFormatCodec.h"

namespace GVis::Session {

class JsonProjectFormatCodec final : public IProjectFormatCodec {
  public:
    QByteArray                 encode(const QVariantMap& data) const override;
    std::optional<QVariantMap> decode(const QByteArray& data) const override;
};

} // namespace GVis::Session
