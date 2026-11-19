#include "DiagnosticsPanel.h"

#include <QLabel>
#include <QVBoxLayout>

DiagnosticsPanel::DiagnosticsPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel("DIAGNOSTICS JRTE", this);
    title->setStyleSheet("font-weight: bold; color: #65B5FF;");

    statusLabel = new QLabel("Moteur : en attente", this);
    reportLabel = new QLabel("Aucune analyse effectuée.", this);

    statusLabel->setWordWrap(true);
    reportLabel->setWordWrap(true);
    reportLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    reportLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    layout->addWidget(title);
    layout->addWidget(statusLabel);
    layout->addWidget(reportLabel);
    layout->addStretch();
}

void DiagnosticsPanel::setReport(const QString& report)
{
    reportLabel->setText(report);
}

void DiagnosticsPanel::setStatus(const QString& status)
{
    statusLabel->setText(status);
}
