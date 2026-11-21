//
// Created by Stalker7274 on 07.11.2026.
//

#include <QDebug>
#include <QDesktopServices>
#include <QKeySequence>
#include <QQuickWindow>
#include <QShortcut>
#include <QUrl>

#include "AppInstance.h"
#include "AppModule.h"
#include "MemoryDump.h"
#include "TrayIcon.h"

namespace
{
    class MemoryDumpModule final : public AppModule
    {
    public:
        void Start(AppInstance& app) override
        {
            TrayIcon* tray = app.getTrayIcon();
            if (tray)
                tray->AddAction(QStringLiteral("Dump memory"), [tray] { Dump(tray); });

            if (QQuickWindow* window = app.getMainWindow())
            {
                auto* shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+M")), window);
                QObject::connect(shortcut, &QShortcut::activated, window, [tray] { Dump(tray); });
            }
        }

    private:
        static void Dump(TrayIcon* tray)
        {
            const QString path = MemoryDump::Write();
            qInfo() << "[MemoryDump] Written:" << path;

            if (!tray)
                return;
            if (path.isEmpty())
                tray->ShowMessage(QStringLiteral("Memory dump"),
                    QStringLiteral("Could not write to %1").arg(MemoryDump::GetDirectory()));
            else
                tray->ShowMessage(QStringLiteral("Memory dump written"), path,
                    [path] { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); });
        }
    };
}

SOUNDLINK_REGISTER_MODULE(MemoryDumpModule)
