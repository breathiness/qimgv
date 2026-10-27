#pragma once

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

// Maps a qimgv icon resource path (".../menuitem/back16.png") to the matching
// name in the vector icon theme ("navigation/arrow-left"). Returns an empty
// string when there is no equivalent, so callers keep the PNG.
QString themedIconName(const QString &resourcePath);
}
