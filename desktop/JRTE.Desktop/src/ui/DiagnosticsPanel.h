#pragma once

#include <QWidget>

class QLabel;

class DiagnosticsPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit DiagnosticsPanel(QWidget* parent = nullptr);

    void setReport(const QString& report);
    void setStatus(const QString& status);

private:
    QLabel* statusLabel;
    QLabel* reportLabel;
};
