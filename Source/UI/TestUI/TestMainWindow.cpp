//
// TestUI - Simple Hello World window implementation
//

#include "TestMainWindow.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QFont>

TestMainWindow::TestMainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Set window properties
    setWindowTitle("Hello World - Test UI");
    resize(800, 600);

    // Create central widget with white background
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setStyleSheet("background-color: white;");
    setCentralWidget(centralWidget);

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setAlignment(Qt::AlignCenter);

    // Create "Hello World" label
    helloLabel = new QLabel("Hello World!", centralWidget);

    // Style the label
    QFont font;
    font.setPointSize(48);
    font.setBold(true);
    helloLabel->setFont(font);
    helloLabel->setStyleSheet("color: #2196F3;");
    helloLabel->setAlignment(Qt::AlignCenter);

    // Add to layout
    layout->addWidget(helloLabel);

    // Add info label
    QLabel* infoLabel = new QLabel("✨ UI Plugin System Working! ✨", centralWidget);
    QFont infoFont;
    infoFont.setPointSize(16);
    infoLabel->setFont(infoFont);
    infoLabel->setStyleSheet("color: #666666;");
    infoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(infoLabel);

    // Add version info
    QLabel* versionLabel = new QLabel("Zero-Dependency Architecture", centralWidget);
    QFont versionFont;
    versionFont.setPointSize(12);
    versionLabel->setFont(versionFont);
    versionLabel->setStyleSheet("color: #999999;");
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);
}
