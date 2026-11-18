#include "MainWindow.h"
#include "VideoWidget.h"
#include "DiagnosticsPanel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QWidget>
#include <QFileDialog>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      videoWidget(nullptr),
      diagnosticsPanel(nullptr)
{
    setWindowTitle("JRTE Desktop 1.0");
    resize(1200, 760);

    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);

    auto* header = new QLabel("JRTE  |  NATIVE VIDEO PLAYER", central);
    header->setStyleSheet(
        "font-size: 20px; font-weight: bold; color: #65B5FF;"
        "padding: 12px;"
    );

    auto* content = new QHBoxLayout();

    videoWidget = new VideoWidget(central);
    diagnosticsPanel = new DiagnosticsPanel(central);
    diagnosticsPanel->setMinimumWidth(230);
    diagnosticsPanel->setMaximumWidth(300);

    content->addWidget(videoWidget, 1);
    content->addWidget(diagnosticsPanel);

    auto* controls = new QHBoxLayout();

    auto* openButton = new QPushButton("Ouvrir une vidéo", central);
    auto* playButton = new QPushButton("Lecture", central);
    auto* stopButton = new QPushButton("Arrêt", central);

    controls->addWidget(openButton);
    controls->addWidget(playButton);
    controls->addWidget(stopButton);
    controls->addStretch();

    mainLayout->addWidget(header);
    mainLayout->addLayout(content, 1);
    mainLayout->addLayout(controls);

    setCentralWidget(central);

    connect(openButton, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            this,
            "Ouvrir une vidéo",
            QString(),
            "Vidéos (*.mp4 *.h264 *.264);;Tous les fichiers (*.*)"
        );

        if (!file.isEmpty()) {
            statusBar()->showMessage(
                "Fichier sélectionné : " + file
            );
        }
    });

    connect(playButton, &QPushButton::clicked, this, [this]() {
        statusBar()->showMessage(
            "Le moteur natif doit encore être connecté."
        );
    });

    connect(stopButton, &QPushButton::clicked, this, [this]() {
        statusBar()->showMessage("Arrêt demandé.");
    });

    setStyleSheet(
        "QMainWindow, QWidget { background: #111722; color: #E7EDF5; }"
        "QPushButton { background: #26364B; padding: 10px 18px;"
        "border: 1px solid #40536B; border-radius: 5px; }"
        "QPushButton:hover { background: #365579; }"
    );

    statusBar()->showMessage("JRTE Desktop initialisé.");
}
