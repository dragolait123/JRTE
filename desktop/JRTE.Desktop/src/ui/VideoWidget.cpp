#include "VideoWidget.h"

#include <QPainter>
#include <QPaintEvent>

VideoWidget::VideoWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(640, 360);
    setAutoFillBackground(false);
}

void VideoWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#080C13"));

    painter.setPen(QColor("#65758B"));
    painter.drawText(rect(), Qt::AlignCenter,
        "JRTE VIDEO ENGINE\n\nZone de rendu en attente du moteur natif");
}
