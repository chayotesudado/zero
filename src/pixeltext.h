#pragma once

#include <QColor>
#include <QFont>
#include <QList>
#include <QPoint>
#include <QQuickPaintedItem>
#include <QString>

// Zero's pixel art, drawn rather than loaded, so it takes the theme's colors:
// the logo (the check and ZERO), the check alone, and any text in the same
// lettering, for page titles. Each piece is a bright face plus two pixels of
// darker depth down and to the right, as in the brand art.
//
// Pixels are whole multiples of the item's pixels, so they stay sharp.
class PixelText : public QQuickPaintedItem {
    Q_OBJECT
    // Drawn in the pixel lettering, as written.
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    // For characters the lettering lacks (©, emoji, other scripts): drawn
    // in this font, bold, at the pixel capitals' height.
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY textChanged)
    // Instead of text: "lockup" (the check and ZERO) or "check".
    Q_PROPERTY(QString art READ art WRITE setArt NOTIFY textChanged)
    Q_PROPERTY(QColor faceColor READ faceColor WRITE setFaceColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor midColor READ midColor WRITE setMidColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor deepColor READ deepColor WRITE setDeepColor NOTIFY colorsChanged)
    // The biggest scale that fits maxWidth wins, down to minScale; below
    // that, the text is cut short with an ellipsis. maxWidth < 0: no limit.
    Q_PROPERTY(int maxScale READ maxScale WRITE setMaxScale NOTIFY textChanged)
    Q_PROPERTY(int minScale READ minScale WRITE setMinScale NOTIFY textChanged)
    Q_PROPERTY(qreal maxWidth READ maxWidth WRITE setMaxWidth NOTIFY textChanged)
    Q_PROPERTY(int pixelScale READ pixelScale NOTIFY laidOut)
    // Room kept above the faces for any accents, in item pixels: put the
    // item this much higher to line its faces up with another's.
    Q_PROPERTY(int faceTop READ faceTop NOTIFY laidOut)
    // False when some of the text is in the plain font.
    Q_PROPERTY(bool supported READ supported NOTIFY laidOut)
    // The text as drawn, after any ellipsis.
    Q_PROPERTY(QString shownText READ shownText NOTIFY laidOut)

public:
    // Pixel coordinates of the faces; the depth is implied.
    struct Layout {
        QList<QPoint> face;
        QList<QPoint> marks; // accents, flat
        int width = 0;   // including the depth
        int height = 0;  // including the depth and any room for accents
        int faceTop = 0; // rows above the faces, for accents
        bool supported = true;
        QString shown;
        // Plain-font text, from a pixel column, on the capitals' baseline.
        struct Run { QString text; int x; };
        QList<Run> runs;
        QFont font;
    };
    // Plain-font text is sized for the scale, so layouts are per scale.
    static Layout layoutText(const QString &text, int scale = 3, const QFont &font = QFont());
    static Layout layoutArt(const QString &name);
    // Fits text into maxWidth item pixels, as described for the properties.
    static Layout fit(const QString &text, qreal maxWidth, int maxScale, int minScale, int *scale,
                      const QFont &font = QFont());

    static constexpr int rows = 8;  // a face's height
    static constexpr int gap = 3;   // between faces; two of it are depth

    explicit PixelText(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

    QString text() const { return m_text; }
    void setText(const QString &text);
    QString art() const { return m_art; }
    void setArt(const QString &art);
    QColor faceColor() const { return m_face; }
    void setFaceColor(const QColor &c);
    QColor midColor() const { return m_mid; }
    void setMidColor(const QColor &c);
    QColor deepColor() const { return m_deep; }
    void setDeepColor(const QColor &c);
    int maxScale() const { return m_maxScale; }
    void setMaxScale(int s);
    int minScale() const { return m_minScale; }
    void setMinScale(int s);
    QFont font() const { return m_font; }
    void setFont(const QFont &font);
    qreal maxWidth() const { return m_maxWidth; }
    void setMaxWidth(qreal w);
    int pixelScale() const { return m_scale; }
    int faceTop() const { return m_layout.faceTop * m_scale; }
    bool supported() const { return m_layout.supported; }
    QString shownText() const { return m_layout.shown; }

signals:
    void textChanged();
    void colorsChanged();
    void laidOut();

private:
    void relayout();

    QString m_text, m_art;
    QColor m_face{"#ebdbb2"}, m_mid{"#a89984"}, m_deep{"#665c54"};
    int m_maxScale = 3, m_minScale = 3, m_scale = 3;
    qreal m_maxWidth = -1;
    QFont m_font;
    Layout m_layout;
};
