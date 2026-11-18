#pragma once

#include <QWidget>

class VideoWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};
