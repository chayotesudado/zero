#pragma once

#include <QColor>
#include <QList>
#include <QPoint>
#include <QQuickPaintedItem>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Zero's pixel art, drawn rather than loaded, so it takes the theme's colors:
// the logo (the check and ZERO) or the check alone. Each is a bright face
// plus two pixels of darker depth down and to the right, as in the brand art.
//
// Pixels are a whole multiple of the item's pixels, so they stay sharp.
class PixelArt : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    // "lockup" (the check and ZERO) or "check".
    Q_PROPERTY(QString art READ art WRITE setArt NOTIFY artChanged)
    // A whole number; anything else is rounded.
    Q_PROPERTY(double pixelScale READ pixelScale WRITE setPixelScale NOTIFY artChanged)
    Q_PROPERTY(QColor faceColor READ faceColor WRITE setFaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor midColor READ midColor WRITE setMidColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor deepColor READ deepColor WRITE setDeepColor NOTIFY colorsChanged)

public:
    // Pixel coordinates of the faces; the depth is implied.
    struct Layout {
        QList<QPoint> face;
        int width = 0;  // including the depth
        int height = 0; // including the depth
    };
    static Layout layout(const QString &art);

    explicit PixelArt(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

    QString art() const { return m_art; }
    void setArt(const QString &art);
    double pixelScale() const { return m_scale; }
    void setPixelScale(double scale);
    QColor faceColor() const { return m_face; }
    void setFaceColor(const QColor &c);
    QColor midColor() const { return m_mid; }
    void setMidColor(const QColor &c);
    QColor deepColor() const { return m_deep; }
    void setDeepColor(const QColor &c);

signals:
    void artChanged();
    void colorsChanged();

private:
    void relayout();

    QString m_art;
    int m_scale = 3;
    QColor m_face{"#ebdbb2"}, m_mid{"#a89984"}, m_deep{"#665c54"};
    Layout m_layout;
};
