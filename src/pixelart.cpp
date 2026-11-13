#include "pixelart.h"

#include <QPainter>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace {

// The logo's faces, from the brand art: the check, four pixels of air, ZERO.
const QStringList &lockup() {
    static const QStringList art{
        "........##......#######...#######...######.....#####.",
        ".......###...........##...##........##...##...##...##",
        "......###...........##....##........##...##...##...##",
        "##...###...........##.....######....######....##...##",
        "###.###...........##......##........##.##.....##...##",
        ".#####...........##.......##........##..##....##...##",
        "..###...........##........##........##...##...##...##",
        "...#............#######...#######...##...##....#####.",
    };
    return art;
}

} // namespace

PixelArt::Layout PixelArt::layout(const QString &art) {
    Layout layout;
    QStringList rows = lockup();
    if (art == "check")
        for (auto &row : rows)
            row = row.left(12);
    else if (art != "lockup")
        return layout;
    for (int r = 0; r < rows.size(); ++r)
        for (int c = 0; c < rows[r].size(); ++c)
            if (rows[r][c] == '#')
                layout.face.append({c, r});
    layout.width = rows.first().size() + 2;
    layout.height = rows.size() + 2;
    return layout;
}

PixelArt::PixelArt(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(false);
    // Nearest-neighbor if the texture is ever scaled, so pixels stay square.
    setSmooth(false);
    setFillColor(Qt::transparent);
}

void PixelArt::relayout() {
    m_layout = layout(m_art);
    setImplicitSize(m_layout.width * m_scale, m_layout.height * m_scale);
    update();
}

void PixelArt::paint(QPainter *painter) {
    painter->setRenderHint(QPainter::Antialiasing, false);
    const int s = m_scale;
    // Deepest first, so each nearer layer covers the one behind it.
    const std::pair<int, QColor> layers[] = {{2, m_deep}, {1, m_mid}, {0, m_face}};
    for (const auto &[depth, color] : layers)
        for (const QPoint &p : std::as_const(m_layout.face))
            painter->fillRect((p.x() + depth) * s, (p.y() + depth) * s, s, s, color);
}

void PixelArt::setArt(const QString &art) {
    if (art == m_art) return;
    m_art = art;
    emit artChanged();
    relayout();
}

void PixelArt::setPixelScale(double value) {
    const int scale = std::max(1, int(std::lround(value)));
    if (scale == m_scale) return;
    m_scale = scale;
    emit artChanged();
    relayout();
}

void PixelArt::setFaceColor(const QColor &c) {
    if (c == m_face) return;
    m_face = c;
    emit colorsChanged();
    update();
}

void PixelArt::setMidColor(const QColor &c) {
    if (c == m_mid) return;
    m_mid = c;
    emit colorsChanged();
    update();
}

void PixelArt::setDeepColor(const QColor &c) {
    if (c == m_deep) return;
    m_deep = c;
    emit colorsChanged();
    update();
}
