#pragma once

#include <QMainWindow>
#include "../engine/NativeVideoEngine.h"

class VideoWidget;
class DiagnosticsPanel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void openVideo();

    VideoWidget* videoWidget;
    DiagnosticsPanel* diagnosticsPanel;

    NativeVideoEngine engine;
};
