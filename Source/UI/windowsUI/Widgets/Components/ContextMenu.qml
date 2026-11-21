pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import SoundLink 1.0

// A right-click menu built when it opens: openAt(scenePoint, entries). An entry is { text, icon, action } or
// { separator: true }; null entries are skipped, so a list can use `condition ? {...} : null`.
Popup {
    id: root

    property var entries: []

    function openAt(point, items) {
        entries = items.filter((item) => item !== null);

        let contentHeight = 2 * padding;
        for (const item of entries)
            contentHeight += item.separator ? 9 : 32;

        x = Math.max(8, Math.min(point.x, parent.width - width - 8));
        y = Math.max(8, Math.min(point.y, parent.height - contentHeight - 8));
        open();
    }

    parent: Overlay.overlay
    width: 240
    padding: 4
    // Modal without a dimmer: a click outside only closes the menu and does not also press what is under it.
    modal: true
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: Theme.palette.fieldBg
        radius: Theme.metrics.radiusLg
        border.width: 1
        border.color: Theme.palette.borderStrong
    }

    contentItem: Column {
        Repeater {
            model: root.entries

            Column {
                id: line

                required property var modelData

                MenuEntry {
                    visible: !line.modelData.separator
                    iconName: line.modelData.icon || ""
                    text: line.modelData.text || ""
                    onTriggered: {
                        const action = line.modelData.action;
                        root.close();
                        action();
                    }
                }
                MenuDivider { visible: line.modelData.separator === true }
            }
        }
    }
}
