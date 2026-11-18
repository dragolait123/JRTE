#pragma once

#include <QWidget>

class QLabel;

class DiagnosticsPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit DiagnosticsPanel(QWidget* parent = nullptr);

private:
    QLabel* statusLabel;
};
