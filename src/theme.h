#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    // Hyprland's corner rounding, in pixels; a real, as QML radii are.
    Q_PROPERTY(double radius READ radius NOTIFY changed)
    // The rest of the Omarchy theme's palette (colors.toml), so the whole
    // window follows the theme, not just its accent.
    Q_PROPERTY(bool light READ light NOTIFY changed)
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor darkBackground READ darkBackground NOTIFY changed)
    Q_PROPERTY(QColor darkerBackground READ darkerBackground NOTIFY changed)
    Q_PROPERTY(QColor lighterBackground READ lighterBackground NOTIFY changed)
    Q_PROPERTY(QColor foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QColor dimForeground READ dimForeground NOTIFY changed)
    Q_PROPERTY(QColor softForeground READ softForeground NOTIFY changed)
    Q_PROPERTY(QColor muted READ muted NOTIFY changed)
    Q_PROPERTY(QColor selection READ selection NOTIFY changed)
    Q_PROPERTY(QColor red READ red NOTIFY changed)
    Q_PROPERTY(QColor orange READ orange NOTIFY changed)
    Q_PROPERTY(QColor blue READ blue NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    double radius() const { return m_radius; }

    bool light() const { return m_palette.value("mode") == "light"; }
    QColor background() const { return color("background"); }
    QColor darkBackground() const { return color("dark_background"); }
    QColor darkerBackground() const { return color("darker_background"); }
    QColor lighterBackground() const { return color("lighter_background"); }
    QColor foreground() const { return color("foreground"); }
    QColor dimForeground() const { return color("dark_foreground"); }
    QColor softForeground() const { return color("light_foreground"); }
    QColor muted() const { return color("muted"); }
    QColor selection() const { return color("selection"); }
    QColor red() const { return color("red"); }
    QColor orange() const { return color("orange"); }
    QColor blue() const { return color("blue"); }

    // The foreground at an opacity, for hover and selection washes that work
    // on light themes as well as dark ones.
    Q_INVOKABLE QColor tint(qreal alpha) const;
    // Black or white, whichever reads on the given color.
    Q_INVOKABLE QColor textOn(const QColor &color) const;
    // a over b at the given amount, for surfaces like the error banner.
    Q_INVOKABLE QColor mix(const QColor &a, const QColor &b, qreal amount) const;

    static QString readAccent(const QString &path);
    // Every `name = "#rrggbb"` (and mode) in colors.toml.
    static QHash<QString, QString> readPalette(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QColor color(const QString &name) const;

    QString m_accent = "#FFD60A";
    QHash<QString, QString> m_palette;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
