#pragma once

#include <QString>

class QWindow;

/// @brief One dialog to pick a local build for a manual update: a package zip or a deployed folder.
///
/// On Windows this is the system Open dialog filtered to `*.zip` and `package.json` with an extra
/// "Use this folder" button: it takes the selected folder, or the open one when nothing is selected.
/// Picking `package.json` means its folder. Elsewhere it falls back to a Qt file dialog.
namespace LocalBuildPicker
{
    /// @brief Shows the dialog modally.
    /// @param owner Window the dialog belongs to; may be nullptr.
    /// @param appName Shown in the title and the filter.
    /// @return The chosen zip or folder, empty when cancelled.
    QString Pick(QWindow* owner, const QString& appName);
}
