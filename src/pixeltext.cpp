#include "pixeltext.h"

#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QStringList>
#include <QTextBoundaryFinder>
#include <algorithm>
#include <cmath>

namespace {

// The lettering of the ZERO wordmark: capitals 8 pixels tall, uprights two
// pixels wide and bars one, corners cut on round letters. Z, E, R and O are
// the wordmark's own. Rows run from the capitals' top; descenders add two.
const QHash<QChar, QStringList> &glyphs() {
    static const QHash<QChar, QStringList> font{
        {'A', {".#####.", "##...##", "##...##", "#######", "##...##", "##...##", "##...##", "##...##"}},
        {'B', {"######.", "##...##", "##...##", "######.", "##...##", "##...##", "##...##", "######."}},
        {'C', {".#####.", "##...##", "##.....", "##.....", "##.....", "##.....", "##...##", ".#####."}},
        {'D', {"######.", "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", "######."}},
        {'E', {"#######", "##.....", "##.....", "######.", "##.....", "##.....", "##.....", "#######"}},
        {'F', {"#######", "##.....", "##.....", "######.", "##.....", "##.....", "##.....", "##....."}},
        {'G', {".#####.", "##...##", "##.....", "##.....", "##..###", "##...##", "##...##", ".#####."}},
        {'H', {"##...##", "##...##", "##...##", "#######", "##...##", "##...##", "##...##", "##...##"}},
        {'I', {"##", "##", "##", "##", "##", "##", "##", "##"}},
        {'J', {".....##", ".....##", ".....##", ".....##", ".....##", "##...##", "##...##", ".#####."}},
        {'K', {"##...##", "##..##.", "##.##..", "####...", "####...", "##.##..", "##..##.", "##...##"}},
        {'L', {"##.....", "##.....", "##.....", "##.....", "##.....", "##.....", "##.....", "#######"}},
        {'M', {"##....##", "###..###", "########", "##.##.##", "##....##", "##....##", "##....##", "##....##"}},
        {'N', {"##...##", "###..##", "####.##", "##.####", "##..###", "##...##", "##...##", "##...##"}},
        {'O', {".#####.", "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", ".#####."}},
        {'P', {"######.", "##...##", "##...##", "######.", "##.....", "##.....", "##.....", "##....."}},
        {'Q', {".#####.", "##...##", "##...##", "##...##", "##...##", "##.#.##", "##..##.", ".###.##"}},
        {'R', {"######.", "##...##", "##...##", "######.", "##.##..", "##..##.", "##...##", "##...##"}},
        {'S', {".#####.", "##...##", "##.....", ".#####.", ".....##", ".....##", "##...##", ".#####."}},
        {'T', {"######", "..##..", "..##..", "..##..", "..##..", "..##..", "..##..", "..##.."}},
        {'U', {"##...##", "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", ".#####."}},
        {'V', {"##...##", "##...##", "##...##", "##...##", "##...##", ".##.##.", "..###..", "...#..."}},
        {'W', {"##....##", "##....##", "##....##", "##....##", "##.##.##", "########", "###..###", "##....##"}},
        {'X', {"##...##", "##...##", ".##.##.", "..###..", "..###..", ".##.##.", "##...##", "##...##"}},
        {'Y', {"##..##", "##..##", "##..##", ".####.", "..##..", "..##..", "..##..", "..##.."}},
        {'Z', {"#######", ".....##", "....##.", "...##..", "..##...", ".##....", "##.....", "#######"}},
        // Lowercase: a body six rows tall under the capitals' top two, with
        // ascenders to the top and descenders two rows below the line.
        {'a', {"......", "......", ".####.", "....##", ".#####", "##..##", "##..##", ".#####"}},
        {'b', {"##....", "##....", "#####.", "##..##", "##..##", "##..##", "##..##", "#####."}},
        {'c', {"......", "......", ".####.", "##..##", "##....", "##....", "##..##", ".####."}},
        {'d', {"....##", "....##", ".#####", "##..##", "##..##", "##..##", "##..##", ".#####"}},
        {'e', {"......", "......", ".####.", "##..##", "######", "##....", "##..##", ".####."}},
        {'f', {".###", "##..", "####", "##..", "##..", "##..", "##..", "##.."}},
        {'g', {"......", "......", ".#####", "##..##", "##..##", "##..##", "##..##", ".#####", "....##", ".####."}},
        {'h', {"##....", "##....", "#####.", "##..##", "##..##", "##..##", "##..##", "##..##"}},
        {'i', {"##", "..", "##", "##", "##", "##", "##", "##"}},
        {'j', {"..##", "....", "..##", "..##", "..##", "..##", "..##", "..##", "..##", "###."}},
        {'k', {"##....", "##....", "##..##", "##.##.", "####..", "####..", "##.##.", "##..##"}},
        {'l', {"##.", "##.", "##.", "##.", "##.", "##.", "##.", ".##"}},
        {'m', {"........", "........", "#######.", "##.##.##", "##.##.##", "##.##.##", "##.##.##", "##.##.##"}},
        {'n', {"......", "......", "#####.", "##..##", "##..##", "##..##", "##..##", "##..##"}},
        {'o', {"......", "......", ".####.", "##..##", "##..##", "##..##", "##..##", ".####."}},
        {'p', {"......", "......", "#####.", "##..##", "##..##", "##..##", "##..##", "#####.", "##....", "##...."}},
        {'q', {"......", "......", ".#####", "##..##", "##..##", "##..##", "##..##", ".#####", "....##", "....##"}},
        {'r', {"......", "......", "##.###", "####..", "##....", "##....", "##....", "##...."}},
        {'s', {"......", "......", ".#####", "##....", ".####.", "....##", "....##", "#####."}},
        {'t', {".##..", ".##..", "#####", ".##..", ".##..", ".##..", ".##..", "..###"}},
        {'u', {"......", "......", "##..##", "##..##", "##..##", "##..##", "##..##", ".#####"}},
        {'v', {"......", "......", "##..##", "##..##", "##..##", "##..##", ".####.", "..##.."}},
        {'w', {"........", "........", "##....##", "##....##", "##.##.##", "##.##.##", "##.##.##", ".######."}},
        {'x', {"......", "......", "##..##", ".####.", "..##..", "..##..", ".####.", "##..##"}},
        {'y', {"......", "......", "##..##", "##..##", "##..##", "##..##", "##..##", ".#####", "....##", ".####."}},
        {'z', {"......", "......", "######", "...##.", "..##..", ".##...", "##....", "######"}},
        {'0', {".####.", "##..##", "##..##", "##..##", "##..##", "##..##", "##..##", ".####."}},
        {'1', {".##", "###", ".##", ".##", ".##", ".##", ".##", ".##"}},
        {'2', {".#####.", "##...##", ".....##", "....##.", "...##..", "..##...", ".##....", "#######"}},
        {'3', {".#####.", "##...##", ".....##", "..####.", ".....##", ".....##", "##...##", ".#####."}},
        {'4', {"....##.", "...###.", "..####.", ".##.##.", "##..##.", "#######", "....##.", "....##."}},
        {'5', {"#######", "##.....", "##.....", "######.", ".....##", ".....##", "##...##", ".#####."}},
        {'6', {".#####.", "##...##", "##.....", "######.", "##...##", "##...##", "##...##", ".#####."}},
        {'7', {"#######", ".....##", "....##.", "...##..", "..##...", "..##...", "..##...", "..##..."}},
        {'8', {".#####.", "##...##", "##...##", ".#####.", "##...##", "##...##", "##...##", ".#####."}},
        {'9', {".#####.", "##...##", "##...##", "##...##", ".######", ".....##", "##...##", ".#####."}},
        {' ', {"...", "...", "...", "...", "...", "...", "...", "..."}},
        {'.', {"..", "..", "..", "..", "..", "..", "##", "##"}},
        {',', {"..", "..", "..", "..", "..", "##", "##", "#."}},
        {':', {"..", "##", "##", "..", "..", "##", "##", ".."}},
        {';', {"..", "##", "##", "..", "..", "##", "##", "#."}},
        {'!', {"##", "##", "##", "##", "##", "..", "##", "##"}},
        {'?', {".#####.", "##...##", ".....##", "...###.", "..##...", "..##...", ".......", "..##..."}},
        {'\'', {"##", "##", "..", "..", "..", "..", "..", ".."}},
        {'"', {"##.##", "##.##", ".....", ".....", ".....", ".....", ".....", "....."}},
        {'-', {".....", ".....", ".....", ".....", "#####", ".....", ".....", "....."}},
        {'_', {"......", "......", "......", "......", "......", "......", "......", "######"}},
        {'+', {"......", "......", "..##..", "..##..", "######", "..##..", "..##..", "......"}},
        {'=', {"......", "......", "......", "######", "......", "######", "......", "......"}},
        {'/', {"....##", "....##", "...##.", "...##.", "..##..", ".##...", ".##...", "##...."}},
        {'(', {".##", "##.", "##.", "##.", "##.", "##.", "##.", ".##"}},
        {')', {"##.", ".##", ".##", ".##", ".##", ".##", ".##", "##."}},
        {'&', {".###...", "##.##..", "##.##..", ".###...", "##.##.#", "##..###", "##...##", ".####.#"}},
        {'#', {".##..##.", ".##..##.", "########", ".##..##.", ".##..##.", "########", ".##..##.", ".##..##."}},
        {QChar(0x2026), {"........", "........", "........", "........", "........", "........", "##.##.##", "##.##.##"}},
    };
    return font;
}

// Accents: two rows of mark and one of air above the letter, in room kept
// above the line when it's needed. Flat, without depth, which would smear
// them at this size. Others are left off.
const QHash<QChar, QStringList> &marks() {
    static const QHash<QChar, QStringList> font{
        {QChar(0x0301), {"..##", ".##."}},     // acute
        {QChar(0x0300), {"##..", ".##."}},     // grave
        {QChar(0x0302), {".##.", "##.##"}},    // circumflex
        {QChar(0x0303), {".###..#", "#..###."}}, // tilde
        {QChar(0x0308), {"##.##", "....."}},   // diaeresis
        {QChar(0x030A), {".##.", "#..#"}},     // ring
    };
    return font;
}
constexpr int markRows = 3;

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

void addRows(QList<QPoint> &points, const QStringList &rows, int x, int y) {
    for (int r = 0; r < rows.size(); ++r)
        for (int c = 0; c < rows[r].size(); ++c)
            if (rows[r][c] == '#')
                points.append({x + c, y + r});
}

} // namespace

PixelText::Layout PixelText::layoutArt(const QString &name) {
    Layout layout;
    QStringList rows = lockup();
    if (name == "check")
        for (auto &row : rows)
            row = row.left(12);
    else if (name != "lockup")
        return layout;
    addRows(layout.face, rows, 0, 0);
    layout.width = rows.first().size() + 2;
    layout.height = rows.size() + 2;
    return layout;
}

namespace {

// The text a character the lettering lacks is drawn in instead: the given
// font, bold, sized so its capitals are as tall as the pixel ones.
QFont fallbackFont(QFont font, int scale) {
    font.setBold(true);
    font.setPixelSize(100);
    const qreal cap = QFontMetricsF(font).capHeight();
    font.setPixelSize(std::max(1, qRound(PixelText::rows * scale * 100 / (cap > 0 ? cap : 70))));
    return font;
}

// User-perceived characters, so an emoji or a letter and its accents stay whole.
QStringList graphemes(const QString &text) {
    QStringList out;
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    qsizetype start = 0;
    while (finder.toNextBoundary() != -1) {
        out << text.mid(start, finder.position() - start);
        start = finder.position();
    }
    return out;
}

int firstRow(const QStringList &glyph) {
    for (int r = 0; r < glyph.size(); ++r)
        if (glyph[r].contains('#'))
            return r;
    return 0;
}

} // namespace

PixelText::Layout PixelText::layoutText(const QString &text, int scale, const QFont &font) {
    scale = std::max(1, scale);
    Layout layout;
    layout.shown = text.simplified();
    layout.font = fallbackFont(font, scale);
    const QFontMetricsF metrics(layout.font);

    // Each character is a glyph with any accents, or plain text; plain text
    // next to plain text is one run, so emoji sequences stay joined.
    struct Piece { QStringList glyph; QList<QChar> marks; QString plain; };
    QList<Piece> pieces;
    for (const QString &cluster : graphemes(layout.shown)) {
        // Á is A plus an acute: draw the letter, and the accent if known.
        const QString parts = cluster.normalized(QString::NormalizationForm_D);
        Piece piece;
        piece.glyph = parts.at(0).isSpace() ? glyphs().value(' ') : glyphs().value(parts.at(0));
        for (int i = 1; i < parts.size() && !piece.glyph.isEmpty(); ++i) {
            if (marks().contains(parts.at(i)))
                piece.marks.append(parts.at(i));
            else if (parts.at(i).category() != QChar::Mark_NonSpacing)
                piece.glyph.clear();
        }
        if (piece.glyph.isEmpty()) {
            layout.supported = false;
            if (!pieces.isEmpty() && !pieces.last().plain.isEmpty())
                pieces.last().plain += cluster;
            else
                pieces.append({{}, {}, cluster});
            continue;
        }
        // An accent takes the place of i's and j's dot.
        if (!piece.marks.isEmpty() && (parts.at(0) == 'i' || parts.at(0) == 'j'))
            piece.glyph[0] = QString(piece.glyph[0].size(), '.');
        pieces.append(piece);
    }

    // Room above the capitals for accents and the plain font's ascent, and
    // below the line for descenders and its descent.
    int above = 0, below = rows;
    for (const auto &piece : pieces) {
        if (!piece.plain.isEmpty()) {
            above = std::max(above, int(std::ceil((metrics.ascent() - rows * scale) / scale)));
            below = std::max(below, rows + int(std::ceil(metrics.descent() / scale)));
        } else {
            if (!piece.marks.isEmpty())
                above = std::max(above, markRows - firstRow(piece.glyph));
            below = std::max(below, int(piece.glyph.size()));
        }
    }
    layout.faceTop = above;

    int x = 0, left = 0;
    for (const auto &piece : pieces) {
        int width;
        if (!piece.plain.isEmpty()) {
            width = int(std::ceil(metrics.horizontalAdvance(piece.plain) / scale));
            layout.runs.append({piece.plain, x});
        } else {
            width = piece.glyph.first().size();
            addRows(layout.face, piece.glyph, x, layout.faceTop);
            const int markTop = layout.faceTop + firstRow(piece.glyph) - markRows;
            for (const QChar m : piece.marks) {
                const auto mark = marks().value(m);
                const int at = x + (width - int(mark.first().size())) / 2;
                left = std::min(left, at);
                addRows(layout.marks, mark, at, markTop);
            }
        }
        x += width + gap;
    }
    // A mark wider than its letter, on the first one, can start left of 0.
    if (left < 0) {
        for (auto &p : layout.face)
            p.rx() -= left;
        for (auto &p : layout.marks)
            p.rx() -= left;
        for (auto &run : layout.runs)
            run.x -= left;
    }
    layout.width = pieces.isEmpty() ? 0 : x - gap + 2 - left;
    layout.height = pieces.isEmpty() ? 0 : layout.faceTop + below + 2;
    return layout;
}

PixelText::Layout PixelText::fit(const QString &text, qreal maxWidth, int maxScale, int minScale, int *scale,
                                 const QFont &font) {
    minScale = std::max(1, minScale);
    maxScale = std::max(minScale, maxScale);
    for (int s = maxScale; s >= minScale; --s) {
        Layout layout = layoutText(text, s, font);
        if (maxWidth < 0 || layout.width * s <= maxWidth) {
            *scale = s;
            return layout;
        }
    }
    *scale = minScale;
    // Too long even small: shorten it a character at a time, never leaving a
    // space before the ellipsis.
    const Layout whole = layoutText(text, minScale, font);
    QStringList kept = graphemes(whole.shown);
    while (!kept.isEmpty()) {
        kept.removeLast();
        const QString head = kept.join(QString()).trimmed();
        Layout cut = layoutText(head + QChar(0x2026), minScale, font);
        if (cut.width * minScale <= maxWidth || head.isEmpty())
            return cut;
    }
    return whole;
}

PixelText::PixelText(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(false);
    // Nearest-neighbor if the texture is ever scaled, so pixels stay square.
    setSmooth(false);
    setFillColor(Qt::transparent);
}

void PixelText::relayout() {
    int scale = std::max(1, m_maxScale);
    m_layout = m_art.isEmpty() ? fit(m_text, m_maxWidth, m_maxScale, m_minScale, &scale, m_font) : layoutArt(m_art);
    m_scale = scale;
    setImplicitSize(m_layout.width * m_scale, m_layout.height * m_scale);
    emit laidOut();
    update();
}

void PixelText::paint(QPainter *painter) {
    painter->setRenderHint(QPainter::Antialiasing, false);
    const int s = m_scale;
    // Deepest first, so each nearer layer covers the one behind it.
    const std::pair<int, QColor> layers[] = {{2, m_deep}, {1, m_mid}, {0, m_face}};
    for (const auto &[depth, color] : layers)
        for (const QPoint &p : std::as_const(m_layout.face))
            painter->fillRect((p.x() + depth) * s, (p.y() + depth) * s, s, s, color);
    for (const QPoint &p : std::as_const(m_layout.marks))
        painter->fillRect(p.x() * s, p.y() * s, s, s, m_face);
    // Characters the lettering lacks, in the plain font on the same line.
    if (!m_layout.runs.isEmpty()) {
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        painter->setFont(m_layout.font);
        painter->setPen(m_face);
        for (const auto &run : std::as_const(m_layout.runs))
            painter->drawText(QPointF(run.x * s, (m_layout.faceTop + rows) * s), run.text);
    }
}

void PixelText::setText(const QString &text) {
    if (text == m_text) return;
    m_text = text;
    emit textChanged();
    relayout();
}

void PixelText::setArt(const QString &art) {
    if (art == m_art) return;
    m_art = art;
    emit textChanged();
    relayout();
}

void PixelText::setFaceColor(const QColor &c) {
    if (c == m_face) return;
    m_face = c;
    emit colorsChanged();
    update();
}

void PixelText::setMidColor(const QColor &c) {
    if (c == m_mid) return;
    m_mid = c;
    emit colorsChanged();
    update();
}

void PixelText::setDeepColor(const QColor &c) {
    if (c == m_deep) return;
    m_deep = c;
    emit colorsChanged();
    update();
}

void PixelText::setMaxScale(int s) {
    if (s == m_maxScale) return;
    m_maxScale = s;
    emit textChanged();
    relayout();
}

void PixelText::setMinScale(int s) {
    if (s == m_minScale) return;
    m_minScale = s;
    emit textChanged();
    relayout();
}

void PixelText::setFont(const QFont &font) {
    if (font == m_font) return;
    m_font = font;
    emit textChanged();
    relayout();
}

void PixelText::setMaxWidth(qreal w) {
    if (w == m_maxWidth) return;
    m_maxWidth = w;
    emit textChanged();
    relayout();
}
