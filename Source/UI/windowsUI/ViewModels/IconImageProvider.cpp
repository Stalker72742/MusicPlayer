//
// Created by Stalker7274 on 05.11.2026.
//

#include "IconImageProvider.h"

#include <QColor>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStringList>
#include <QtMath>

#include <cmath>
#include <initializer_list>
#include <numbers>

namespace
{
    constexpr qreal Grid = 24.0;
    constexpr qreal StrokeWidth = 1.5;
    constexpr qreal Pi = std::numbers::pi;

    class Canvas
    {
    public:
        Canvas(QPainter& painter, const QColor& color, qreal strokeWidth)
            : painter(painter)
            , color(color)
            , pen(color, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
        {
        }

        void Stroke(const QPainterPath& path)
        {
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }

        void Fill(const QPainterPath& path)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawPath(path);
        }

        void Line(std::initializer_list<QPointF> points, bool bClosed = false)
        {
            Stroke(Polyline(points, bClosed));
        }

        void Shape(std::initializer_list<QPointF> points)
        {
            Fill(Polyline(points, true));
        }

        void Rect(qreal x, qreal y, qreal width, qreal height)
        {
            QPainterPath path;
            path.addRect(x, y, width, height);
            Stroke(path);
        }

        void Block(qreal x, qreal y, qreal width, qreal height)
        {
            QPainterPath path;
            path.addRect(x, y, width, height);
            Fill(path);
        }

        void Circle(qreal x, qreal y, qreal radius)
        {
            QPainterPath path;
            path.addEllipse(QPointF(x, y), radius, radius);
            Stroke(path);
        }

        void Dot(qreal x, qreal y, qreal radius)
        {
            QPainterPath path;
            path.addEllipse(QPointF(x, y), radius, radius);
            Fill(path);
        }

        void Arc(qreal x, qreal y, qreal radius, qreal from, qreal to)
        {
            QPainterPath path;
            path.arcMoveTo(QRectF(x - radius, y - radius, radius * 2, radius * 2), -from * 180.0 / Pi);
            AddArc(path, x, y, radius, from, to);
            Stroke(path);
        }

        static void AddArc(QPainterPath& path, qreal x, qreal y, qreal radius, qreal from, qreal to)
        {
            path.arcTo(QRectF(x - radius, y - radius, radius * 2, radius * 2), -from * 180.0 / Pi, -(to - from) * 180.0 / Pi);
        }

    private:
        static QPainterPath Polyline(std::initializer_list<QPointF> points, bool bClosed)
        {
            QPainterPath path;
            bool bFirst = true;
            for (const QPointF& point : points)
            {
                if (bFirst)
                    path.moveTo(point);
                else
                    path.lineTo(point);
                bFirst = false;
            }
            if (bClosed)
                path.closeSubpath();
            return path;
        }

        QPainter& painter;
        QColor color;
        QPen pen;
    };

    QPainterPath HeartPath()
    {
        QPainterPath path;
        path.moveTo(12, 20);
        path.cubicTo(2, 13.5, 2, 7, 7, 5);
        path.cubicTo(9.5, 4, 11, 5.5, 12, 7.5);
        path.cubicTo(13, 5.5, 14.5, 4, 17, 5);
        path.cubicTo(22, 7, 22, 13.5, 12, 20);
        path.closeSubpath();
        return path;
    }

    void DrawSpeaker(Canvas& c)
    {
        c.Shape({{4, 9}, {8, 9}, {12, 5}, {12, 19}, {8, 15}, {4, 15}});
    }

    using DrawIcon = void (*)(Canvas&);

    const QHash<QString, DrawIcon>& Icons()
    {
        static const QHash<QString, DrawIcon> icons{
            {QStringLiteral("menu"), [](Canvas& c) {
                c.Line({{4, 7}, {20, 7}});
                c.Line({{4, 12}, {20, 12}});
                c.Line({{4, 17}, {20, 17}});
            }},
            {QStringLiteral("chevron-left"), [](Canvas& c) { c.Line({{15, 6}, {9, 12}, {15, 18}}); }},
            {QStringLiteral("chevron-right"), [](Canvas& c) { c.Line({{9, 6}, {15, 12}, {9, 18}}); }},
            {QStringLiteral("chevron-down"), [](Canvas& c) { c.Line({{6, 9}, {12, 15}, {18, 9}}); }},
            {QStringLiteral("chevron-up"), [](Canvas& c) { c.Line({{6, 15}, {12, 9}, {18, 15}}); }},
            {QStringLiteral("search"), [](Canvas& c) {
                c.Circle(11, 11, 6);
                c.Line({{15.5, 15.5}, {20, 20}});
            }},
            {QStringLiteral("library"), [](Canvas& c) {
                c.Circle(12, 12, 9);
                c.Shape({{10, 8}, {16, 12}, {10, 16}});
            }},
            {QStringLiteral("home"), [](Canvas& c) {
                c.Line({{4, 11}, {12, 4}, {20, 11}});
                c.Line({{6, 9.5}, {6, 20}, {10, 20}, {10, 14}, {14, 14}, {14, 20}, {18, 20}, {18, 9.5}});
            }},
            {QStringLiteral("globe"), [](Canvas& c) {
                c.Circle(12, 12, 9);
                QPainterPath ellipse;
                ellipse.addEllipse(8, 3, 8, 18);
                c.Stroke(ellipse);
                c.Line({{3, 12}, {21, 12}});
            }},
            {QStringLiteral("plus"), [](Canvas& c) {
                c.Line({{12, 5}, {12, 19}});
                c.Line({{5, 12}, {19, 12}});
            }},
            {QStringLiteral("check"), [](Canvas& c) { c.Line({{5, 12.5}, {10, 17.5}, {19, 7}}); }},
            {QStringLiteral("pencil"), [](Canvas& c) {
                c.Line({{4, 20}, {4.5, 15.5}, {15.5, 4.5}, {19.5, 8.5}, {8.5, 19.5}}, true);
                c.Line({{13, 7}, {17, 11}});
            }},
            {QStringLiteral("select"), [](Canvas& c) {
                c.Rect(4, 4, 16, 16);
                c.Line({{8, 12.5}, {11, 15.5}, {16.5, 9}});
            }},
            {QStringLiteral("link"), [](Canvas& c) {
                c.Line({{10, 14}, {14, 10}});
                QPainterPath upper;
                upper.moveTo(11, 7);
                upper.lineTo(13.5, 4.5);
                Canvas::AddArc(upper, 16.5, 7.5, 4.2, -Pi * 0.75, Pi * 0.25);
                upper.lineTo(17, 13);
                c.Stroke(upper);
                QPainterPath lower;
                lower.moveTo(13, 17);
                lower.lineTo(10.5, 19.5);
                Canvas::AddArc(lower, 7.5, 16.5, 4.2, Pi * 0.25, Pi * 1.25);
                lower.lineTo(7, 11);
                c.Stroke(lower);
            }},
            {QStringLiteral("external"), [](Canvas& c) {
                c.Line({{11, 5}, {5, 5}, {5, 19}, {19, 19}, {19, 13}});
                c.Line({{12, 12}, {20, 4}});
                c.Line({{14, 4}, {20, 4}, {20, 10}});
            }},
            {QStringLiteral("trash"), [](Canvas& c) {
                c.Line({{4, 7}, {20, 7}});
                c.Line({{9, 7}, {9, 4}, {15, 4}, {15, 7}});
                c.Line({{6, 7}, {7, 20}, {17, 20}, {18, 7}});
            }},
            {QStringLiteral("folder"), [](Canvas& c) { c.Line({{3, 6}, {9, 6}, {11, 8}, {21, 8}, {21, 19}, {3, 19}}, true); }},
            {QStringLiteral("funnel"), [](Canvas& c) { c.Line({{4, 5}, {20, 5}, {14, 12.5}, {14, 19}, {10, 17}, {10, 12.5}}, true); }},
            {QStringLiteral("tag"), [](Canvas& c) {
                c.Line({{4, 4}, {12, 4}, {20.5, 12.5}, {12.5, 20.5}, {4, 12}}, true);
                c.Dot(8.5, 8.5, 1.4);
            }},
            {QStringLiteral("grip"), [](Canvas& c) {
                for (int row = 0; row < 3; ++row)
                {
                    for (int column = 0; column < 2; ++column)
                        c.Dot(9 + column * 6, 6 + row * 6, 1.5);
                }
            }},
            {QStringLiteral("list"), [](Canvas& c) {
                c.Rect(4, 5, 16, 14);
                c.Line({{8, 9}, {17, 9}});
                c.Line({{8, 12}, {17, 12}});
                c.Line({{8, 15}, {13, 15}});
            }},
            {QStringLiteral("heart"), [](Canvas& c) { c.Stroke(HeartPath()); }},
            {QStringLiteral("heart-filled"), [](Canvas& c) { c.Fill(HeartPath()); }},
            {QStringLiteral("clock"), [](Canvas& c) {
                c.Circle(12, 12, 8);
                c.Line({{12, 12}, {12, 7}});
                c.Line({{12, 12}, {16, 12}});
            }},
            {QStringLiteral("settings"), [](Canvas& c) {
                c.Circle(12, 12, 3.5);
                for (int i = 0; i < 8; ++i)
                {
                    const qreal angle = i * Pi / 4;
                    c.Line({{12 + std::cos(angle) * 6, 12 + std::sin(angle) * 6}, {12 + std::cos(angle) * 8.5, 12 + std::sin(angle) * 8.5}});
                }
            }},
            {QStringLiteral("audio"), [](Canvas& c) {
                DrawSpeaker(c);
                c.Arc(13, 12, 3, -Pi / 3, Pi / 3);
                c.Arc(13, 12, 6, -Pi / 3, Pi / 3);
            }},
            {QStringLiteral("info"), [](Canvas& c) {
                c.Circle(12, 12, 9);
                c.Line({{12, 9}, {12, 16}});
                c.Dot(12, 6, 0.5);
            }},
            {QStringLiteral("download"), [](Canvas& c) {
                c.Line({{12, 4}, {12, 15}});
                c.Line({{7, 11}, {12, 16}, {17, 11}});
                c.Line({{5, 20}, {19, 20}});
            }},
            {QStringLiteral("devices"), [](Canvas& c) {
                c.Line({{12, 15}, {3, 15}, {3, 5}, {17, 5}, {17, 8}});
                c.Line({{6, 19}, {12, 19}});
                c.Line({{9, 15}, {9, 19}});
                c.Rect(14, 10, 7, 10);
                c.Dot(17.5, 17.5, 0.9);
            }},
            {QStringLiteral("sync"), [](Canvas& c) {
                c.Arc(12, 12, 6.75, -Pi * 0.892, -Pi * 0.25);
                c.Line({{16.77, 7.23}, {20.25, 10.5}});
                c.Line({{20.25, 6}, {20.25, 10.5}, {15.75, 10.5}});
                c.Arc(12, 12, 6.75, Pi * 0.75, Pi * 0.108);
                c.Line({{3.75, 13.5}, {7.23, 16.77}});
                c.Line({{3.75, 18}, {3.75, 13.5}, {8.25, 13.5}});
            }},
            {QStringLiteral("play"), [](Canvas& c) { c.Shape({{7, 5}, {19, 12}, {7, 19}}); }},
            {QStringLiteral("pause"), [](Canvas& c) {
                c.Block(7, 5, 3, 14);
                c.Block(14, 5, 3, 14);
            }},
            {QStringLiteral("next"), [](Canvas& c) {
                c.Shape({{5, 5}, {15, 12}, {5, 19}});
                c.Block(16, 5, 2.5, 14);
            }},
            {QStringLiteral("prev"), [](Canvas& c) {
                c.Block(5.5, 5, 2.5, 14);
                c.Shape({{19, 5}, {9, 12}, {19, 19}});
            }},
            {QStringLiteral("shuffle"), [](Canvas& c) {
                c.Line({{3, 7}, {7, 7}, {17, 17}, {21, 17}});
                c.Line({{3, 17}, {7, 17}, {11, 13}});
                c.Line({{13, 11}, {17, 7}, {21, 7}});
                c.Line({{18, 4}, {21, 7}, {18, 10}});
                c.Line({{18, 14}, {21, 17}, {18, 20}});
            }},
            {QStringLiteral("repeat"), [](Canvas& c) {
                c.Line({{7, 8}, {17, 8}, {17, 11}});
                c.Line({{20, 8}, {17, 5}});
                c.Line({{20, 8}, {17, 11}});
                c.Line({{17, 16}, {7, 16}, {7, 13}});
                c.Line({{4, 16}, {7, 13}});
                c.Line({{4, 16}, {7, 19}});
            }},
            {QStringLiteral("queue"), [](Canvas& c) {
                c.Line({{4, 7}, {16, 7}});
                c.Line({{4, 12}, {16, 12}});
                c.Line({{4, 17}, {11, 17}});
                c.Block(18, 13, 3, 7);
                c.Block(15, 15, 2, 5);
            }},
            {QStringLiteral("volume"), [](Canvas& c) {
                DrawSpeaker(c);
                c.Arc(13, 12, 3.5, -Pi / 3, Pi / 3);
            }},
            {QStringLiteral("more"), [](Canvas& c) {
                c.Dot(6, 12, 1.3);
                c.Dot(12, 12, 1.3);
                c.Dot(18, 12, 1.3);
            }},
            {QStringLiteral("playing"), [](Canvas& c) {
                c.Block(5, 8, 2, 8);
                c.Block(11, 5, 2, 14);
                c.Block(17, 10, 2, 6);
            }},
            {QStringLiteral("minimize"), [](Canvas& c) { c.Line({{5, 12}, {19, 12}}); }},
            {QStringLiteral("maximize"), [](Canvas& c) { c.Rect(5, 5, 14, 14); }},
            {QStringLiteral("restore"), [](Canvas& c) {
                c.Rect(5, 7, 12, 12);
                c.Line({{8, 7}, {8, 4}, {20, 4}, {20, 16}, {17, 16}});
            }},
            {QStringLiteral("close"), [](Canvas& c) {
                c.Line({{6, 6}, {18, 18}});
                c.Line({{18, 6}, {6, 18}});
            }},
        };
        return icons;
    }
}

IconImageProvider::IconImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage IconImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    const QStringList parts = id.split(QLatin1Char('/'));
    if (parts.size() != 3)
        return {};

    const QColor color(QLatin1Char('#') + parts[1]);
    const qreal logicalSize = parts[2].toDouble();
    if (logicalSize <= 0)
        return {};

    const QSize pixels = requestedSize.isEmpty() ? QSize(qCeil(logicalSize), qCeil(logicalSize)) : requestedSize;
    if (size)
        *size = pixels;

    QImage image(pixels, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(pixels.width() / Grid, pixels.height() / Grid);

    Canvas canvas(painter, color, StrokeWidth * Grid / logicalSize);
    if (const DrawIcon draw = Icons().value(parts[0]))
        draw(canvas);
    else
        canvas.Rect(4, 4, 16, 16);

    return image;
}
