#pragma once

#include <QColor>

class QWidget;
class QPainter;
class QIcon;
class QString;

namespace UiStyle {
// True when the Qlementine widget style is in use. Qlementine paints every
// control itself, so only the stylesheet rules for qimgv's own widgets are
// applied; the rest would otherwise override its corners and paddings.
bool useQlementine();
void setUseQlementine(bool enabled);

// True when one of the Material themes is installed. Parts of the UI that
// change their look with the theme (the folder tiles, which switch to Papirus'
// colourful folder) ask this instead of guessing from colours.
bool materialTheme();

// Corner radius for widgets qimgv paints itself (navigation bar, overlays).
// Follows the active style when it exposes one, so both halves agree.
int controlRadius();

// Draws the background a widget would otherwise get from the stylesheet, using
// the palette. Returns false when the stylesheet should do it instead (i.e. the
// classic style is active), so callers can fall back to PE_Widget.
// card = rounded rectangle with a border (floating panels, overlays),
// otherwise a plain fill (side panels, folder view).
bool paintBackground(QWidget *widget, QPainter &painter, bool card = false);

// Colours the Qlementine style itself would use, for the widgets qimgv has to
// paint by hand (they are plain QWidgets, which a QStyle does not draw). Taken
// from the active theme so the hand-painted parts and the styled controls agree.
// Returns false when Qlementine is not in use.
struct ChromePalette {
    QColor resting;     // resting button: theme neutralColor
    QColor hover;       // hovered button: theme neutralColorHovered
    QColor pressed;     // pressed or checked button: theme neutralColorPressed
    QColor menuHover;   // hovered list/menu row: theme neutralColor (the focused one)
    QColor surface;     // inset field (path bar): theme backgroundColorMain1
    QColor surfaceLine; // field border: theme borderColor
    QColor groupFill;   // settings group card: theme backgroundColorMain3 over Main2
    QColor groupLine;   // settings group border: theme borderColor
    QColor bar;         // folder view top bar: theme backgroundColorMain2
    QColor seam;        // 1px separator between areas: theme backgroundColorMain1
    // Text and the disabled family. The style already carries these; hand-painted
    // widgets used to hardcode a grey border or fade the whole thing to 50%, which
    // looks wrong on the dark themes.
    QColor text;           // theme secondaryColor (the body text colour)
    QColor textSecondary;  // theme secondaryAlternativeColor (captions, hints)
    QColor disabled;       // disabled surface: theme neutralColorDisabled
    QColor disabledText;   // disabled text: theme secondaryColorDisabled
    QColor disabledLine;   // disabled border: theme borderColorDisabled
    QColor accent;         // theme primaryColor
    QColor error;          // theme statusColorError (destructive hover, close button)
    qreal radius = 0;      // theme borderRadius
};
bool chromePalette(ChromePalette &palette);

// MD1 fades disabled content instead of recolouring it: 38% of the on-surface
// colour, over the surface it sits on. Used where a widget cannot recolour cheaply.
constexpr qreal kDisabledContentOpacity = 0.38;

// Maps a qimgv icon resource path (".../menuitem/back16.png") to the matching
// name in the vector icon theme ("navigation/arrow-left"). Returns an empty
// string when there is no equivalent, so callers keep the PNG.
QString themedIconName(const QString &resourcePath);

// Icon for one of those theme names. The Lucide set is preferred and the theme
// that Qlementine ships with is the fallback for names it has no equivalent for,
// so both look like one set.
QIcon vectorIcon(const QString &themeName);
}
