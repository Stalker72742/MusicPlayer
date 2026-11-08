//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QQuickImageProvider>

/// @brief Draws the app's icons for `image://icons/...`, used by AppIcon.qml.
///
/// All icons of the same look share one cached image and texture, so an icon costs an Image item and nothing else.
///
/// The id is `<name>/<aarrggbb>/<logical size>`; the requested size is the size in pixels. Icons are drawn on a
/// 24 unit grid, the stroke stays 1.5 logical pixels wide at any size. An unknown name gives a plain square.
/// A new icon is one more entry in the table in the .cpp.
class IconImageProvider : public QQuickImageProvider
{
public:
    IconImageProvider();

    /// @brief Draws the icon. Called by Qt Quick for images not in its cache.
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
