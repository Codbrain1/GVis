#pragma once

namespace GVis::Core {
class LogManager {
  public:
    static void setup();
    static void setVerbose(bool enabled);
};
} // namespace GVis::Core