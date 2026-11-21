import QtQuick
import SoundLink 1.0

// Drawn by IconImageProvider; icons of the same look share one image. Needs an explicit width and height.
// Do not animate color: every color in between would be a new image.
Image {
    property string name: ""
    property color color: Theme.palette.iconDefault

    source: name !== "" && width > 0
            ? "image://icons/" + name + "/" + String(color).slice(1) + "/" + width
            : ""
    sourceSize: Qt.size(Math.ceil(width * Theme.pixelRatio), Math.ceil(height * Theme.pixelRatio))
}
