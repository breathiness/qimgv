#include "uitheme.h"
#include "themestore.h"

#ifdef USE_QLEMENTINE
#include <QApplication>
#include <QFontDatabase>
#include <oclero/qlementine/style/QlementineStyle.hpp>

#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QStyleHints>
#endif

#ifdef USE_QLEMENTINE
namespace {
// The theme reader accepts "#rrggbb" and "#rrggbbaa". QColor::name(HexArgb)
// writes the alpha first, so the string is assembled here instead.
QString hex(const QColor &color) {
    if(color.alpha() >= 255)
        return color.name();
    return QStringLiteral("#%1%2%3%4")
        .arg(color.red(),   2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(),  2, 16, QLatin1Char('0'))
        .arg(color.alpha(), 2, 16, QLatin1Char('0'));
}

// t = 0 gives `a`, t = 1 gives `b`. Keeps the alpha of `a`, which is what the
// theme wants for its "disabled" colours: same opacity, mixed towards the
// surface they sit on.
QColor mix(const QColor &a, const QColor &b, qreal t) {
    return QColor::fromRgbF(a.redF()   * (1.0 - t) + b.redF()   * t,
                            a.greenF() * (1.0 - t) + b.greenF() * t,
                            a.blueF()  * (1.0 - t) + b.blueF()  * t,
                            a.alphaF());
}

// Black or white, whichever stays readable on `background`.
QColor foregroundFor(const QColor &background) {
    return background.lightnessF() > 0.55 ? QColor(0x1a, 0x1a, 0x1a) : QColor(0xff, 0xff, 0xff);
}

bool isMaterialScheme(int tid) {
    return tid == COLORS_MATERIAL_LIGHT || tid == COLORS_MATERIAL_DARK;
}

// Theme file for a scheme that is not following the desktop. The Material
// themes are qimgv's own (res/themes); the rest are the ones Qlementine ships.
QString themeResourcePath(int tid) {
    switch(tid) {
        case COLORS_LIGHT:
            return QStringLiteral(":/qlementine/resources/themes/light.json");
        case COLORS_MATERIAL_LIGHT:
            return QStringLiteral(":/res/themes/material-light.json");
        case COLORS_MATERIAL_DARK:
            return QStringLiteral(":/res/themes/material-dark.json");
        default:
            return QStringLiteral(":/qlementine/resources/themes/dark.json");
    }
}

QJsonObject loadThemeFile(const QString &path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

// Replaces the accent family of a stock theme with the colour the user picked.
// Only the fields that depend on the accent change; surfaces, borders, text,
// shadows and status colours keep the values the theme author chose for light
// or dark.
void retintAccent(QJsonObject &theme, const QColor &accent, const QColor &surface) {
    const QColor hovered  = accent.lighter(115);
    const QColor pressed  = accent.lighter(130);
    const QColor disabled = mix(accent, surface, 0.55);
    const QColor foreground = foregroundFor(accent);
    const QColor alternative = accent.darker(115);

    theme["primaryColor"]                   = hex(accent);
    theme["primaryColorHovered"]            = hex(hovered);
    theme["primaryColorPressed"]            = hex(pressed);
    theme["primaryColorDisabled"]           = hex(disabled);
    theme["primaryColorForeground"]         = hex(foreground);
    theme["primaryColorForegroundHovered"]  = hex(foreground);
    theme["primaryColorForegroundPressed"]  = hex(foreground);
    theme["primaryColorForegroundDisabled"] = hex(mix(foreground, surface, 0.45));

    theme["primaryAlternativeColor"]         = hex(alternative);
    theme["primaryAlternativeColorHovered"]  = hex(alternative.lighter(110));
    theme["primaryAlternativeColorPressed"]  = hex(alternative.lighter(125));
    theme["primaryAlternativeColorDisabled"] = hex(mix(alternative, surface, 0.55));

    // The focus ring is the accent at 40% opacity in both stock themes.
    QColor focus = accent;
    focus.setAlpha(0x66);
    theme["focusColor"] = hex(focus);
}
}
#endif // USE_QLEMENTINE

#ifdef USE_QLEMENTINE
void UiTheme::apply(ColorScheme &scheme, bool followSystem, bool tintAccent) {
    auto *style = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style());
    if(!style)
        return;

    // "Follow the desktop" only chooses between the light and the dark half of
    // the family the user is in: following the system while on a Material theme
    // must not drop back to the stock Qlementine ones.
    const bool systemLight = UiTheme::systemPrefersLight();
    const bool material = isMaterialScheme(scheme.tid);
    QString path = themeResourcePath(scheme.tid);
    if(followSystem) {
        if(material)
            path = systemLight ? themeResourcePath(COLORS_MATERIAL_LIGHT)
                               : themeResourcePath(COLORS_MATERIAL_DARK);
        else
            path = systemLight ? themeResourcePath(COLORS_LIGHT)
                               : themeResourcePath(COLORS_DARK);
    }
    QJsonObject json = loadThemeFile(path);
    if(json.isEmpty()) {
        qWarning() << "UiTheme: cannot read the Qlementine theme resource" << path;
        return;
    }
    // The Material themes ship the palette they are named after, and the accent
    // picker is hidden while one of them is active, so recolouring here would
    // only fight the theme it is meant to reproduce.
    if(tintAccent && !followSystem && !material) {
        const QColor stock(json["primaryColor"].toString());
        // Leave the file alone when the accent is the one it was drawn with: its
        // hover/pressed/focus shades are hand-tuned, and beating them with
        // derived ones would change the stock look for no reason.
        if(scheme.accent != stock) {
            const QColor surface(json["backgroundColorMain2"].toString());
            retintAccent(json, scheme.accent, surface);
        }
    }

    const auto theme = oclero::qlementine::Theme::fromJsonDoc(QJsonDocument(json));
    if(!theme.has_value()) {
        qWarning() << "UiTheme: malformed Qlementine theme";
        return;
    }
    static bool fontsLoaded = false;
    static QString cjkFamily;
    if(!fontsLoaded) {
        // Source Han Sans CN, subset to GB2312 plus the strings in this repo and
        // renamed to "Qimgv Han Sans" so it cannot collide with a copy installed
        // system-wide. The Regular and Bold cuts share the family and differ only
        // in style, so emphasis is a real weight match instead of a synthesised
        // emboldening.
        const int regularId = QFontDatabase::addApplicationFont(QStringLiteral(":/res/fonts/SourceHanSansCN-Regular.otf"));
        QFontDatabase::addApplicationFont(QStringLiteral(":/res/fonts/SourceHanSansCN-Bold.otf"));
        const QStringList cjkFamilies = regularId >= 0 ? QFontDatabase::applicationFontFamilies(regularId) : QStringList();
        if(!cjkFamilies.isEmpty())
            cjkFamily = cjkFamilies.first();
        fontsLoaded = true;
    }

    auto appliedTheme = theme.value();
    // Latin comes from a system family and Chinese from the bundled Source Han
    // Sans, through Qt's per-script application fallback. Naming a CJK (or an
    // application) family in the QFont itself forces a full font-database
    // population on the first metric query (~1.2 s with the ~1450 fonts installed
    // here); the script fallback is resolved lazily instead, so the window comes
    // up in ~0.6 s while Han text still renders in the bundled face.
    QStringList families{QStringLiteral("Arial")};
    if(!cjkFamily.isEmpty()) {
        for (const auto script : {QChar::Script_Han, QChar::Script_Hiragana,
                                  QChar::Script_Katakana, QChar::Script_Hangul}) {
            QFontDatabase::setApplicationFallbackFontFamilies(script, {cjkFamily});
        }
    }

    // MD1 type scale: 14sp body, 12sp caption, 16/20/24 for headings, and Medium
    // for the places MD emphasises with weight rather than size.
    const auto applyFont = [&](QFont &font, int pixelSize, QFont::Weight weight, bool bold = false) {
        font.setFamilies(families);
        font.setPixelSize(pixelSize);
        // Both families carry a real Bold: Arial Bold for Latin, Source Han
        // Sans CN Bold for the Han runs, so nothing is synthesised.
        font.setWeight(bold ? QFont::Bold : weight);
    };
    // Qlementine paints some text (menus, item rows) from this int rather than
    // from the QFont objects, so it has to move with the scale or those stay 12.
    appliedTheme.fontSize = 14;
    applyFont(appliedTheme.fontRegular, 14, QFont::Normal);
    applyFont(appliedTheme.fontBold, 14, QFont::Medium, true);
    applyFont(appliedTheme.fontCaption, 12, QFont::Normal);
    applyFont(appliedTheme.fontH1, 24, QFont::Medium, true);
    applyFont(appliedTheme.fontH2, 20, QFont::Medium, true);
    applyFont(appliedTheme.fontH3, 16, QFont::Medium, true);
    applyFont(appliedTheme.fontH4, 16, QFont::Normal);
    applyFont(appliedTheme.fontH5, 14, QFont::Medium, true);
    appliedTheme.fontMonospace.setFamilies({"Cascadia Mono", "Consolas", "Roboto Mono"});

    style->setTheme(appliedTheme);
    // Qlementine only pushes the palette; widgets that paint themselves (the
    // folder tree, the context menu) kept Qt's default 9pt font, which is why
    // they looked smaller than everything else.
    QApplication::setFont(appliedTheme.fontRegular);

    // Read the result back into the scheme. The theme may have adjusted what was
    // asked for (and while following the desktop it was never asked), but qimgv
    // paints part of the window itself and has to use the same colours.
    const auto &t = style->theme();
    const QPalette pal = style->standardPalette();
    QColor overlay = t.backgroundColorMain1;
    overlay.setAlpha(230); // overlays float above the image, slightly translucent
    BaseColorScheme base = {-1};
    base.tid                   = scheme.tid;
    base.background            = t.backgroundColorMain1;
    base.background_fullscreen = t.backgroundColorWorkspace;
    base.text                  = pal.color(QPalette::WindowText);
    base.icons                 = pal.color(QPalette::WindowText);
    base.widget                = t.neutralColor;
    base.widget_border         = t.borderColor;
    base.accent                = t.primaryColor;
    base.folderview            = t.backgroundColorMain3;
    base.folderview_topbar     = t.backgroundColorMain2;
    base.scrollbar             = t.neutralColor;
    base.overlay_text          = pal.color(QPalette::WindowText);
    base.overlay               = overlay;
    scheme.setBaseColors(base);
}
#else
void UiTheme::apply(ColorScheme &scheme, bool followSystem, bool tintAccent) {
    Q_UNUSED(scheme)
    Q_UNUSED(followSystem)
    Q_UNUSED(tintAccent)
}
#endif

bool UiTheme::systemPrefersLight() {
#if defined(USE_QLEMENTINE) && QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light;
#else
    return false;
#endif
}
