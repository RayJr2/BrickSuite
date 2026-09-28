#pragma once

#include <QSurfaceFormat>

inline QSurfaceFormat partViewerSurfaceFormat()
{
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setSamples(4);
    return format;
}

inline void configurePartViewerSurfaceFormat()
{
#ifdef Q_OS_MACOS
    // Cocoa's window compositor and QOpenGLWidget must use compatible profiles.
    // Set this before QApplication creates any internal OpenGL contexts.
    QSurfaceFormat::setDefaultFormat(partViewerSurfaceFormat());
#endif
}
