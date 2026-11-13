#include "pixeltext.h"

#include <QHash>
#include <QPainter>
#include <QStringList>
#include <algorithm>

namespace {

// The lettering of the ZERO wordmark: capitals 8 pixels tall, uprights two
// pixels wide and bars one, corners cut on round letters. Z, E, R and O are
// the wordmark's own.
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

// Accents, drawn in the five rows kept above the capitals: two rows of mark,
// two of its depth, and one of air. Others are left off rather than refusing the text.
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
constexpr int markRows = 5;

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

void addRows(PixelText::Layout &layout, const QStringList &rows, int x, int y) {
    for (int r = 0; r < rows.size(); ++r)
        for (int c = 0; c < rows[r].size(); ++c)
            if (rows[r][c] == '#')
                layout.face.append({x + c, y + r});
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
    addRows(layout, rows, 0, 0);
    layout.width = rows.first().size() + 2;
    layout.height = rows.size() + 2;
    return layout;
}

PixelText::Layout PixelText::layoutText(const QString &text) {
    Layout layout;
    layout.shown = text.simplified().toUpper();

    struct Piece { QStringList glyph; QList<QChar> marks; };
    QList<Piece> pieces;
    for (const QChar c : layout.shown) {
        // Á is A plus an acute: draw the letter, and the accent if known.
        const QString parts = QString(c).normalized(QString::NormalizationForm_D);
        Piece piece;
        if (c.isSpace())
            piece.glyph = glyphs().value(' ');
        else
            piece.glyph = glyphs().value(parts.at(0));
        if (piece.glyph.isEmpty()) {
            layout.supported = false;
            continue;
        }
        for (int i = 1; i < parts.size(); ++i)
            if (marks().contains(parts.at(i)))
                piece.marks.append(parts.at(i));
        pieces.append(piece);
    }

    const bool accented = std::any_of(pieces.cbegin(), pieces.cend(), [](const Piece &p) { return !p.marks.isEmpty(); });
    layout.faceTop = accented ? markRows : 0;
    int x = 0, left = 0;
    for (const auto &piece : pieces) {
        const int width = piece.glyph.first().size();
        addRows(layout, piece.glyph, x, layout.faceTop);
        for (const QChar m : piece.marks) {
            const auto mark = marks().value(m);
            const int at = x + (width - int(mark.first().size())) / 2;
            left = std::min(left, at);
            addRows(layout, mark, at, 0);
        }
        x += width + gap;
    }
    // A mark wider than its letter, on the first one, can start left of 0.
    if (left < 0)
        for (auto &p : layout.face)
            p.rx() -= left;
    layout.width = pieces.isEmpty() ? 0 : x - gap + 2 - left;
    layout.height = pieces.isEmpty() ? 0 : layout.faceTop + rows + 2;
    return layout;
}

PixelText::Layout PixelText::fit(const QString &text, qreal maxWidth, int maxScale, int minScale, int *scale) {
    minScale = std::max(1, minScale);
    maxScale = std::max(minScale, maxScale);
    Layout layout = layoutText(text);
    for (int s = maxScale; s >= minScale; --s) {
        if (maxWidth < 0 || layout.width * s <= maxWidth) {
            *scale = s;
            return layout;
        }
    }
    *scale = minScale;
    // Too long even small: shorten it word by word where it can, letter by
    // letter where it can't, and end it with an ellipsis.
    QString shown = layout.shown;
    while (!shown.isEmpty()) {
        shown.chop(1);
        if (shown.endsWith(' '))
            continue;
        Layout cut = layoutText(shown.trimmed() + QChar(0x2026));
        if (cut.width * minScale <= maxWidth || shown.trimmed().isEmpty()) {
            cut.supported = layout.supported;
            return cut;
        }
    }
    return layout;
}

PixelText::PixelText(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(false);
    // Nearest-neighbor if the texture is ever scaled, so pixels stay square.
    setSmooth(false);
    setFillColor(Qt::transparent);
}

void PixelText::relayout() {
    int scale = std::max(1, m_maxScale);
    m_layout = m_art.isEmpty() ? fit(m_text, m_maxWidth, m_maxScale, m_minScale, &scale) : layoutArt(m_art);
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

void PixelText::setMaxWidth(qreal w) {
    if (w == m_maxWidth) return;
    m_maxWidth = w;
    emit textChanged();
    relayout();
}
