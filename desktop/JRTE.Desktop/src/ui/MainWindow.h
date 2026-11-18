#pragma once

#include <QMainWindow>

class VideoWidget;
class DiagnosticsPanel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    VideoWidget* videoWidget;
    DiagnosticsPanel* diagnosticsPanel;
};
