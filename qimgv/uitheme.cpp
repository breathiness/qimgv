#include "uitheme.h"
#include "themestore.h"

#ifdef USE_QLEMENTINE
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

QJsonObject loadThemeFile(bool light) {
    QFile file(light ? ":/qlementine/resources/themes/light.json"
                     : ":/qlementine/resources/themes/dark.json");
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

    const bool light = followSystem ? UiTheme::systemPrefersLight()
                                    : (scheme.tid == COLORS_LIGHT);
    QJsonObject json = loadThemeFile(light);
    if(json.isEmpty()) {
        qWarning() << "UiTheme: cannot read the Qlementine theme resource";
        return;
    }
    if(tintAccent && !followSystem) {
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
    style->setTheme(theme.value());

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
