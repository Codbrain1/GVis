#pragma once
#include <QMap>
#include <QObject>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QUuid>
#include "colormapmanager_export.h"

namespace GVis::Visualize::Layers {

// Кэш 1D-текстур градиентов, привязанный к share-группе GL-контекстов.
// При AA_ShareOpenGLContexts=true все QOpenGLWidget-ы приложения используют
// один и тот же shareContext() — значит один кэш на всё приложение корректен,
// но привязка к конкретному QOpenGLContext* защищает от случая, когда
// sharing по каким-то причинам не установлен для конкретного контекста.
class COLORMAPMANAGER_EXPORT ColorMapTexture {
  public:
    static GLuint getOrCreate(QOpenGLFunctions_3_3_Core* gl, const QUuid& colorMapId);
    static void   invalidate(const QUuid& colorMapId); // сбросить во всех контекстах сразу
    static void   releaseForContext(QOpenGLContext* ctx);

  private:
    struct ContextCache {
        QMap<QUuid, GLuint> textures;
    };

    // ключ — либо сам контекст, либо его shareContext(), если он есть —
    // так все окна с общим sharing попадают в один и тот же кэш
    static QOpenGLContext* cacheKeyFor(QOpenGLContext* ctx);

    static QMap<QOpenGLContext*, ContextCache> s_caches;
};

} // namespace GVis::Visualize::Layers