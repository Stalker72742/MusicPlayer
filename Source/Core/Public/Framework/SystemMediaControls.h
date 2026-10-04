//
// Created by Stalker7274 on 03.10.2026.
//

#pragma once

#include <QObject>

#include <memory>

class QWindow;

/// @brief Connects PlayerSubsystem to the OS media controls: media keys and the system media overlay.
///
/// On Windows this is System Media Transport Controls: media keys are not grabbed, Windows routes them
/// to whichever app played last. The session is enabled only once something has been played here.
/// Does nothing on other platforms for now.
class SystemMediaControls : public QObject
{
    Q_OBJECT

public:
    /// @param window The main window; Windows ties the media session to its HWND.
    /// @param parent QObject parent.
    explicit SystemMediaControls(QWindow* window, QObject* parent = nullptr);
    ~SystemMediaControls() override;

    /// @brief Whether the OS media session was created.
    bool IsAvailable() const;

private:
    void OnButtonPressed(int button);
    void UpdatePlaybackStatus();
    void UpdateDisplay();

    struct Impl;
    std::unique_ptr<Impl> impl;
};
