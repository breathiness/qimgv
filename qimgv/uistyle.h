#pragma once

#include <QColor>

class QWidget;
class QPainter;
class QString;

namespace UiStyle {
// True when the Qlementine widget style is in use. Qlementine paints every
// control itself, so only the stylesheet rules for qimgv's own widgets are
// applied; the rest would otherwise override its corners and paddings.
bool useQlementine();
void setUseQlementine(bool enabled);

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
    qreal radius = 0;   // theme borderRadius
};
bool chromePalette(ChromePalette &palette);

// Maps a qimgv icon resource path (".../menuitem/back16.png") to the matching
// name in the vector icon theme ("navigation/arrow-left"). Returns an empty
// string when there is no equivalent, so callers keep the PNG.
QString themedIconName(const QString &resourcePath);
}
