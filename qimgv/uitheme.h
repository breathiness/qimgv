#pragma once

class ColorScheme;

// The Qlementine widget style paints buttons, lists, menus and scroll bars from a
// theme: a JSON document holding ~70 colours. qimgv paints the navigation bar,
// overlays, folder view and thumbnail strip itself and takes those colours from
// the same theme (see UiStyle::chromePalette()), which is what keeps the two
// halves of the window consistent.
//
// main.cpp used to install a fixed theme (dark.json) before Settings existed,
// and Settings::setColorScheme() then copied the theme's colours over whatever
// the user had picked. That made the whole colour section of the settings
// cosmetic. This module is the missing link: it installs the theme that matches
// the chosen window theme and, if the user picked one, recolours its accent.
namespace UiTheme {
// Installs the theme that goes with `scheme` on the active Qlementine style and
// reads the result back into `scheme`, so qimgv's own painting uses exactly the
// same colours. Does nothing outside Qlementine mode, or in builds without it.
//
// followSystem: use the desktop's light/dark preference instead of the scheme's
//               preset. The theme then keeps its own accent, as the colour
//               options are disabled while following the system.
// tintAccent:   use scheme.accent for the theme's accent instead of the accent
//               the theme file ships with.
void apply(ColorScheme &scheme, bool followSystem, bool tintAccent);

// True when the desktop asks for a light UI. Platforms that do not report the
// preference count as dark, which is what the Qlementine UI always was.
bool systemPrefersLight();
}
