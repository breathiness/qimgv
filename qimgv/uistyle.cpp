#include "uistyle.h"

#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QPalette>
#include <QHash>
#include <QString>
#include <QIcon>
#include <QFile>

#ifdef USE_QLEMENTINE
#include <oclero/qlementine/style/QlementineStyle.hpp>
#endif

namespace {
bool g_useQlementine = false;
}

namespace UiStyle {
bool useQlementine() {
    return g_useQlementine;
}

void setUseQlementine(bool enabled) {
    g_useQlementine = enabled;
}

bool materialTheme() {
#ifdef USE_QLEMENTINE
    if(!g_useQlementine)
        return false;
    if(auto *style = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style()))
        return style->theme().meta.name.startsWith(QStringLiteral("Material"));
#endif
    return false;
}

int controlRadius() {
#ifdef USE_QLEMENTINE
    if(auto *style = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style()))
        return qRound(style->theme().borderRadius);
#endif
    return 7;
}

bool paintBackground(QWidget *widget, QPainter &painter, bool card) {
    if(!g_useQlementine || !widget)
        return false;
    const QPalette pal = widget->palette();
    painter.setRenderHint(QPainter::Antialiasing, true);
    if(card) {
        const qreal radius = controlRadius();
        const QRectF rect = QRectF(widget->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(pal.color(QPalette::Mid), 1.0));
        painter.setBrush(pal.color(QPalette::Window));
        painter.drawRoundedRect(rect, radius, radius);
    } else {
        painter.fillRect(widget->rect(), pal.color(QPalette::Window));
    }
    return true;
}

bool chromePalette(ChromePalette &palette) {
#ifdef USE_QLEMENTINE
    if(!g_useQlementine)
        return false;
    auto *style = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style());
    if(!style)
        return false;
    const auto &theme = style->theme();
    // The values Qlementine itself uses for these states, see
    // QlementineStyle::buttonBackgroundColor() and ::listItemBackgroundColor().
    palette.resting = theme.neutralColor;
    palette.hover = theme.neutralColorHovered;
    palette.pressed = theme.neutralColorPressed;
    palette.menuHover = theme.neutralColor;
    palette.surface = theme.backgroundColorMain1;
    palette.surfaceLine = theme.borderColor;
    // QElementineStyle::groupBoxBackgroundColor() / groupBoxBorderColor()
    palette.groupFill = QColor::fromRgbF(
        theme.backgroundColorMain2.redF() * 0.25f + theme.backgroundColorMain3.redF() * 0.75f,
        theme.backgroundColorMain2.greenF() * 0.25f + theme.backgroundColorMain3.greenF() * 0.75f,
        theme.backgroundColorMain2.blueF() * 0.25f + theme.backgroundColorMain3.blueF() * 0.75f);
    palette.groupLine = theme.borderColor;
    palette.bar = theme.backgroundColorMain2;
    palette.seam = theme.backgroundColorMain1;
    palette.text = theme.secondaryColor;
    palette.textSecondary = theme.secondaryAlternativeColor;
    palette.disabled = theme.neutralColorDisabled;
    palette.disabledText = theme.secondaryColorDisabled;
    palette.disabledLine = theme.borderColorDisabled;
    palette.accent = theme.primaryColor;
    palette.radius = theme.borderRadius;
    return true;
#else
    Q_UNUSED(palette)
    return false;
#endif
}

QString themedIconName(const QString &resourcePath) {
#ifdef USE_QLEMENTINE
    if(!g_useQlementine || resourcePath.isEmpty())
        return QString();
    // matched on the tail of the resource path, so the theme folder does not matter
    static const QHash<QString, QString> map = {
        {"menuitem/back16.png",       "navigation/arrow-left"},
        {"menuitem/next16.png",       "navigation/arrow-right"},
        {"menuitem/folder16.png",     "file/folder"},
        {"menuitem/folderview16.png", "file/folder"},
        {"menuitem/document-view16.png", "document/open"},
        {"menuitem/open16.png",       "document/open"},
        {"menuitem/settings16.png",   "navigation/settings"},
        {"menuitem/trash16.png",      "action/trash"},
        {"menuitem/copy16.png",       "action/copy"},
        {"menuitem/move16.png",       "action/cut"},
        {"menuitem/run16.png",        "media/play"},
        {"menuitem/print16.png",      "action/print"},
        {"menuitem/image-crop16.png", "action/crop"},
        {"menuitem/resize16.png",     "action/resize-bigger"},
        {"menuitem/rotate-left16.png", "action/rotate-anticlockwise"},
        {"menuitem/rotate-right16.png", "action/rotate-clockwise"},
        {"menuitem/flip-h16.png",     "action/flip-horizontal"},
        {"menuitem/flip-v16.png",     "action/flip-vertical"},
        {"buttons/panel/up16.png",    "navigation/arrow-up"},
        {"buttons/contextmenu/zoom-in18.png", "action/zoom-in"},
        {"buttons/contextmenu/zoom-out18.png", "action/zoom-out"},
        {"buttons/contextmenu/zoom-original18.png", "action/zoom-original"},
        {"buttons/contextmenu/fit-window18.png", "action/zoom-fit"},
        {"buttons/contextmenu/fit-width18.png", "page/zoom-fit-page"},
        {"buttons/contextmenu/fit-height-stretch18.png", "page/zoom-fit-screen"},
        {"overlay/info16.png",        "misc/info"},
        {"overlay/copy16.png",        "action/copy"},
        {"overlay/move16.png",        "action/cut"},
        {"overlay/close-dim16.png",   "action/close"},
        {"settings/general32.png",    "misc/dialog"},
        {"settings/view32.png",       "misc/grid"},
        {"settings/appearance32.png", "misc/color-swatch"},
        {"settings/shortcuts32.png",  "hardware/keyboard"},
        {"settings/navbar32.png",     "navigation/ui-panel-top"},
        {"settings/terminal32.png",   "software/command-line"},
        {"settings/advanced32.png",   "misc/debug"},
        {"settings/about32.png",      "misc/faq"},
        {"buttons/panel/close16.png", "action/close"},
        {"buttons/panel/folder16.png", "file/folder"},
        {"buttons/panel/folderview20.png", "file/folder"},
        {"buttons/panel/settings20.png", "navigation/settings"},
        {"buttons/panel-small/remove12.png", "action/close-small"},
        {"notifications/success16.png", "misc/success"},
        {"overlay/arrow_left_50.png", "navigation/arrow-left"},
        {"overlay/arrow_right_50.png", "navigation/arrow-right"},
    };
    for(auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if(resourcePath.endsWith(it.key()))
            return it.value();
    }
    return QString();
#else
    Q_UNUSED(resourcePath)
    return QString();
#endif
}

// The vector set the application prefers. Lucide is ISC licensed; the files live
// in qimgv/res/icons/lucide and are recoloured like the other mask icons.
QIcon vectorIcon(const QString &themeName) {
    static const QHash<QString, QString> lucide = {
        {"action/close",              "close"},
        {"action/close-small",        "close"},
        {"action/copy",               "content_copy"},
        {"action/crop",               "crop"},
        {"action/cut",                "content_cut"},
        {"action/flip-horizontal",    "flip"},
        {"action/flip-vertical",      "flip_vertical"},
        {"action/print",              "print"},
        {"action/resize-bigger",      "open_in_full"},
        {"action/resize-vertical",    "height"},
        {"action/rotate-anticlockwise", "rotate_left"},
        {"action/rotate-clockwise",   "rotate_right"},
        {"action/swap",               "compare_arrows"},
        {"action/trash",              "delete"},
        {"action/zoom-fit",           "fullscreen"},
        {"action/zoom-in",            "zoom_in"},
        {"action/zoom-original",      "crop_free"},
        {"action/zoom-out",           "zoom_out"},
        {"document/open",             "draft"},
        {"file/folder",               "folder"},
        {"file/folder-open",          "folder_open"},
        {"hardware/keyboard",         "keyboard"},
        {"media/play",                "play_arrow"},
        {"misc/color-swatch",         "palette"},
        {"misc/debug",                "bug_report"},
        {"misc/dialog",               "web_asset"},
        {"misc/faq",                  "help"},
        {"misc/grid",                 "grid_view"},
        {"misc/info",                 "info"},
        {"misc/success",              "check_circle"},
        {"navigation/arrow-left",     "arrow_back"},
        {"navigation/arrow-right",    "arrow_forward"},
        {"navigation/arrow-up",       "arrow_upward"},
        {"navigation/settings",       "settings"},
        {"navigation/ui-panel-top",   "view_agenda"},
        {"page/zoom-fit-page",        "fit_screen"},
        {"page/zoom-fit-screen",      "zoom_out_map"},
        {"software/command-line",     "terminal"},
        {"fit/width",                 "width_full"},
        {"fit/stretch",               "aspect_ratio"},
        {"fit/original",              "image"},
        {"action/refresh",            "refresh"},
        {"action/bookmark-add",       "bookmark_add"},
        {"action/ungroup",            "dashboard"},
        {"action/paste",              "content_paste"},
        {"buttons/panel-small/add-new12.png", "add"},
        {"buttons/panel-small/home12.png", "home"},
        {"buttons/panel-small/root12.png", "hard_drive"},
        {"buttons/panel/document-view20.png", "draft"},
        {"buttons/panel/open20.png",  "folder_open"},
        {"buttons/panel/pin-panel20.png", "push_pin"},
        {"buttons/panel/settings16.png", "settings"},
        {"buttons/panel/toggle-panel20.png", "view_sidebar"},
        {"buttons/videocontrols/mute-off24.png", "volume_off"},
        {"buttons/videocontrols/mute-on24.png", "volume_up"},
        {"buttons/videocontrols/pause24.png", "pause"},
        {"buttons/videocontrols/play24.png", "play_arrow"},
        {"buttons/videocontrols/skip-backwards24.png", "skip_previous"},
        {"buttons/videocontrols/skip-forward24.png", "skip_next"},
        {"notifications/dir_start20.png", "first_page"},
        {"notifications/dir_end20.png", "last_page"},
        {"notifications/error16.png", "error"},
        {"other/dropDownArrow.png",   "expand_more"},
        {"other/sorting-mode16.png",  "swap_vert"},
        {"overlay/edit16.png",        "edit"},
        {"other/image-crop48.png",    "crop"},
    };
    if(themeName.isEmpty())
        return QIcon();
    const QString lucideName = lucide.value(themeName);
    if(!lucideName.isEmpty()) {
        const QString path = QStringLiteral(":/res/icons/material/%1.svg").arg(lucideName);
        if(QFile::exists(path))
            return QIcon(path);
    }
    // no Lucide equivalent: keep the set that ships with the style
    return QIcon(QStringLiteral(":/qlementine/icons/16/%1.svg").arg(themeName));
}
}
