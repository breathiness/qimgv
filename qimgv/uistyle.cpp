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
        {"menuitem/back16.png",               "navigation/arrow-left"},
        {"menuitem/next16.png",               "navigation/arrow-right"},
        {"menuitem/folder16.png",             "file/folder"},
        {"menuitem/folderview16.png",         "file/folder"},
        {"menuitem/document-view16.png",      "document/open"},
        {"menuitem/open16.png",               "document/open"},
        {"menuitem/settings16.png",           "navigation/settings"},
        {"menuitem/trash16.png",              "action/trash"},
        {"menuitem/copy16.png",               "action/copy"},
        {"menuitem/move16.png",               "action/cut"},
        {"menuitem/run16.png",                "media/play"},
        {"menuitem/print16.png",              "action/print"},
        {"menuitem/image-crop16.png",         "action/crop"},
        {"menuitem/resize16.png",             "action/resize-bigger"},
        {"menuitem/rotate-left16.png",        "action/rotate-anticlockwise"},
        {"menuitem/rotate-right16.png",       "action/rotate-clockwise"},
        {"menuitem/flip-h16.png",             "action/flip-horizontal"},
        {"menuitem/flip-v16.png",             "action/flip-vertical"},
        {"buttons/panel/up16.png",            "navigation/arrow-up"},
        {"buttons/contextmenu/zoom-in18.png", "action/zoom-in"},
        {"buttons/contextmenu/zoom-out18.png","action/zoom-out"},
        {"buttons/contextmenu/zoom-original18.png", "action/zoom-original"},
        {"buttons/contextmenu/fit-window18.png",    "action/zoom-fit"},
        {"buttons/contextmenu/fit-width18.png",     "page/zoom-fit-page"},
        {"buttons/contextmenu/fit-height-stretch18.png", "page/zoom-fit-screen"},
        {"overlay/info16.png",                "misc/info"},
        {"overlay/copy16.png",                "action/copy"},
        {"overlay/move16.png",                "action/cut"},
        {"overlay/close-dim16.png",           "action/close"},
        {"settings/general32.png",            "misc/dialog"},
        {"settings/view32.png",               "misc/grid"},
        {"settings/appearance32.png",         "misc/color-swatch"},
        {"settings/shortcuts32.png",          "hardware/keyboard"},
        {"settings/navbar32.png",             "navigation/ui-panel-top"},
        {"settings/terminal32.png",           "software/command-line"},
        {"settings/advanced32.png",           "misc/debug"},
        {"settings/about32.png",              "misc/faq"},
        {"buttons/panel/close16.png",         "action/close"},
        {"buttons/panel/folder16.png",        "file/folder"},
        {"buttons/panel/folderview20.png",    "file/folder"},
        {"buttons/panel/settings20.png",      "navigation/settings"},
        {"buttons/panel-small/remove12.png",  "action/close-small"},
        {"notifications/success16.png",       "misc/success"},
        {"overlay/arrow_left_50.png",         "navigation/arrow-left"},
        {"overlay/arrow_right_50.png",        "navigation/arrow-right"},
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
        {"action/close",                "x"},
        {"action/close-small",          "x"},
        {"action/copy",                 "copy"},
        {"action/crop",                 "crop"},
        {"action/cut",                  "scissors"},
        {"action/flip-horizontal",      "flip-horizontal"},
        {"action/flip-vertical",        "flip-vertical"},
        {"action/print",                "printer"},
        {"action/resize-bigger",        "maximize-2"},
        {"action/resize-vertical",      "move-vertical"},
        {"action/rotate-anticlockwise", "rotate-ccw"},
        {"action/rotate-clockwise",     "rotate-cw"},
        {"action/swap",                 "arrow-left-right"},
        {"action/trash",                "trash-2"},
        {"action/zoom-fit",             "maximize"},
        {"action/zoom-in",              "zoom-in"},
        {"action/zoom-original",        "scan"},
        {"action/zoom-out",             "zoom-out"},
        {"document/open",               "file"},
        {"file/folder",                 "folder"},
        {"file/folder-open",            "folder-open"},
        {"hardware/keyboard",           "keyboard"},
        {"media/play",                  "play"},
        {"misc/color-swatch",           "palette"},
        {"misc/debug",                  "bug"},
        {"misc/dialog",                 "app-window"},
        {"misc/faq",                    "circle-help"},
        {"misc/grid",                   "layout-grid"},
        {"misc/info",                   "info"},
        {"misc/success",                "circle-check"},
        {"navigation/arrow-left",       "arrow-left"},
        {"navigation/arrow-right",      "arrow-right"},
        {"navigation/arrow-up",         "arrow-up"},
        {"navigation/settings",         "settings"},
        {"navigation/ui-panel-top",     "panel-top"},
        {"page/zoom-fit-page",          "stretch-horizontal"},
        {"page/zoom-fit-screen",        "expand"},
        {"software/command-line",       "terminal"},
        {"fit/width",                   "square-code"},
        {"fit/stretch",                 "scaling"},
        {"fit/original",                "aperture"},
        {"action/refresh",               "refresh-cw"},
        {"action/bookmark-add",          "bookmark-plus"},
        {"action/ungroup",               "ungroup"},
        {"action/paste",                 "clipboard-paste"},
        {"buttons/panel-small/add-new12.png", "plus"},
        {"buttons/panel-small/home12.png", "house"},
        {"buttons/panel-small/root12.png", "hard-drive"},
        {"buttons/panel/document-view20.png", "file"},
        {"buttons/panel/open20.png",      "folder-open"},
        {"buttons/panel/pin-panel20.png", "pin"},
        {"buttons/panel/settings16.png",  "settings"},
        {"buttons/panel/toggle-panel20.png", "panel-left"},
        {"buttons/videocontrols/mute-off24.png", "volume-x"},
        {"buttons/videocontrols/mute-on24.png", "volume-2"},
        {"buttons/videocontrols/pause24.png", "pause"},
        {"buttons/videocontrols/play24.png", "play"},
        {"buttons/videocontrols/skip-backwards24.png", "skip-back"},
        {"buttons/videocontrols/skip-forward24.png", "skip-forward"},
        {"notifications/dir_start20.png", "arrow-left-to-line"},
        {"notifications/dir_end20.png",   "arrow-right-to-line"},
        {"notifications/error16.png",     "circle-alert"},
        {"other/dropDownArrow.png",       "chevron-down"},
        {"other/sorting-mode16.png",      "arrow-up-down"},
        {"overlay/edit16.png",            "pencil"},
        {"other/image-crop48.png",        "crop"},
    };
    if(themeName.isEmpty())
        return QIcon();
    const QString lucideName = lucide.value(themeName);
    if(!lucideName.isEmpty()) {
        const QString path = QStringLiteral(":/res/icons/lucide/%1.svg").arg(lucideName);
        if(QFile::exists(path))
            return QIcon(path);
    }
    // no Lucide equivalent: keep the set that ships with the style
    return QIcon(QStringLiteral(":/qlementine/icons/16/%1.svg").arg(themeName));
}
}
