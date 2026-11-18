#include "DiagnosticsPanel.h"

#include <QLabel>
#include <QVBoxLayout>

DiagnosticsPanel::DiagnosticsPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel("DIAGNOSTICS JRTE", this);
    title->setStyleSheet("font-weight: bold; color: #65B5FF;");

    statusLabel = new QLabel("Moteur : en attente de connexion", this);

    layout->addWidget(title);
    layout->addWidget(statusLabel);
    layout->addStretch();
}
