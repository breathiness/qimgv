#include "materialstyle.h"
#include "uistyle.h"

#include <QStyle>
#include <QPainter>
#include <QStyleOptionButton>
#include <QStyleOptionMenuItem>
#include <QStyleOptionToolButton>
#include <QStyleOptionViewItem>
#include <QTabBar>
#include <QTreeView>
#include <QStyleOptionSlider>
#include <QPainterPath>
#include <QFontMetrics>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QHash>
#include <QImage>
#include <QPixmap>

namespace {
// MD1 / MDUI button metrics (com/google material design 1, "buttons"):
// 36dp tall, 88dp minimum width, 16dp horizontal padding, 2dp corner radius,
// 14sp medium uppercase label with 0.04em letter spacing.
constexpr int kButtonHeight = 36;
constexpr int kButtonMinWidth = 88;
constexpr int kButtonPadding = 16;
constexpr qreal kButtonRadius = 2.0;
constexpr int kButtonFontSize = 14;
constexpr qreal kLetterSpacing = 0.04;
// QStyle painting is clipped to the widget, so the shadow has to live inside
// the button's own rect: the body is inset by this much and the widget is grown
// by twice as much (see sizeFromContents), which keeps the visible button at
// the MD1 size.
constexpr int kShadowMargin = 3;
// MD1 list row.
constexpr int kListItemHeight = 48;
constexpr int kTreeRowHeight = 40;
// MD1 menu row / icon.
constexpr int kMenuItemHeight = 32;
constexpr int kMenuIconSize = 20;
// MD1 text field.
constexpr int kFieldHeight = 48;
constexpr int kFieldPadding = 16;
// MD1 combo chevron.
constexpr qreal kChevronWidth = 8.0;
constexpr qreal kChevronHeight = 5.0;
// MDUI draws the checkbox and the radio as an 18dp shape with a 2dp border.
constexpr qreal kSelectorRadius = 2.0;
constexpr qreal kSelectorBorder = 2.0;

// Material 3 state layer opacities. Hover, press and focus are a translucent wash
// of the on-surface colour instead of the solid neutral colour Qlementine paints,
// so one surface colour serves every state. MD3 itself uses 8/12/12; this round
// asks for a stronger focus layer, hence 16%.
// The steps are 20/31/41 of 255: on the dark surface (#121212) that is about
// #25/#2e/#38, on a light one about #eb/#e0/#d6, so all three stay visible and
// distinguishable from the opaque selection fill without a ring or outline.
constexpr qreal kHoverAlpha = 0.08;
constexpr qreal kPressedAlpha = 0.12;
constexpr qreal kFocusAlpha = 0.16;

// Material 3 elevation is defined in uistyle.h, because the surfaces qimgv paints
// by hand (dialog card, undocked navigation bar, selected thumbnail) have to draw
// exactly the same shadow as the style-painted menus.

// MD1 elevation, 2/4/8dp. QStyle painting is clipped to the widget, so the
// shadow is rendered into a pixmap the size of the button (the body is inset by
// kShadowMargin) and blurred by drawing it small and scaling it back up - a real
// blur is not reachable from inside a QStyle. Cached per size and elevation,
// because buttons repaint on every hover animation step.
QPixmap shadowFor(const QSize &size, qreal radius, int dp) {
    static QHash<QString, QPixmap> cache;
    const QString key = QStringLiteral("%1x%2|%3|%4")
                            .arg(size.width()).arg(size.height())
                            .arg(qRound(radius)).arg(dp);
    const auto cached = cache.constFind(key);
    if(cached != cache.constEnd())
        return cached.value();

    const int shrink = 4;
    const QSize smallSize(qMax(1, size.width() / shrink), qMax(1, size.height() / shrink));
    QImage small(smallSize, QImage::Format_ARGB32_Premultiplied);
    small.fill(Qt::transparent);
    {
        QPainter p(&small);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        const int alpha = dp <= 2 ? 90 : dp <= 4 ? 110 : 130;
        p.setBrush(QColor(0, 0, 0, alpha));
        // The body inside this widget, shifted down by the elevation offset.
        const qreal margin = qreal(kShadowMargin) / shrink;
        const qreal offsetY = (dp <= 2 ? 1.0 : 2.0) * (qreal(shrink - 1) / shrink);
        const QRectF body(margin, margin + offsetY,
                          smallSize.width() - 2 * margin, smallSize.height() - 2 * margin);
        const qreal r = qMax(0.0, radius / shrink);
        p.drawRoundedRect(body, r, r);
    }

    QImage blurred = small.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QPixmap pixmap = QPixmap::fromImage(blurred);
    if(cache.size() > 24)
        cache.clear();
    cache.insert(key, pixmap);
    return pixmap;
}

// The on-surface colour of the theme: white on a dark theme, black on a light
// one. Every state layer is a wash of it, which is what keeps one colour family
// across hover, focus and press.
QColor onSurfaceColor(const oclero::qlementine::Theme &theme) {
    return theme.backgroundColorMain1.lightnessF() < 0.5 ? QColor(255, 255, 255)
                                                         : QColor(0, 0, 0);
}

// Material 3 state layer: a translucent wash of the content colour over whatever
// the control painted underneath. Focus wins over hover, press wins over both, so
// a focused control keeps its layer while the mouse is over it.
void drawStateLayer(QPainter *painter, const QRectF &rect, qreal radius, const QColor &color,
                    const QStyleOption *option) {
    if(!(option->state & QStyle::State_Enabled))
        return;
    qreal alpha = 0.0;
    if(option->state & QStyle::State_Sunken)
        alpha = kPressedAlpha;
    else if(option->state & QStyle::State_MouseOver)
        alpha = kHoverAlpha;
    if(option->state & QStyle::State_HasFocus)
        alpha = qMax(alpha, kFocusAlpha);
    if(alpha <= 0.0)
        return;

    QColor layer = color;
    layer.setAlphaF(alpha);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(layer);
    painter->drawRoundedRect(rect, radius, radius);
    painter->restore();
}
}

void MaterialStyle::drawSlider(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const {
    const auto *slider = qstyleoption_cast<const QStyleOptionSlider *>(option);
    if(!slider) {
        QlementineStyle::drawComplexControl(CC_Slider, option, painter, widget);
        return;
    }

    const auto &theme = this->theme();
    const bool dark = theme.backgroundColorMain1.lightnessF() < 0.5;
    const bool enabled = slider->state & State_Enabled;
    const bool hovered = enabled && (slider->state & State_MouseOver);
    const bool pressed = enabled && (slider->state & State_Sunken);

    // MD1: 2dp track, 12dp handle that grows while it is being dragged.
    const int thickness = theme.sliderGrooveHeight > 0 ? theme.sliderGrooveHeight : 2;
    const int handleSize = (hovered || pressed) ? 18 : 12;
    const bool horizontal = slider->orientation == Qt::Horizontal;

    QColor active = theme.primaryColor;
    QColor inactive = dark ? QColor(255, 255, 255, 0x3d) : QColor(0, 0, 0, 0x3d);
    if(!enabled) {
        active = theme.primaryColorDisabled;
        inactive = dark ? QColor(255, 255, 255, 0x1a) : QColor(0, 0, 0, 0x1a);
    }

    const QRect rect = slider->rect;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);

    QRectF track;
    if(horizontal)
        track = QRectF(rect.x() + handleSize / 2.0, rect.center().y() - thickness / 2.0,
                       qMax(1, rect.width() - handleSize), thickness);
    else
        track = QRectF(rect.center().x() - thickness / 2.0, rect.y() + handleSize / 2.0,
                       thickness, qMax(1, rect.height() - handleSize));

    painter->setBrush(inactive);
    painter->drawRoundedRect(track, thickness / 2.0, thickness / 2.0);

    const int span = int(horizontal ? track.width() : track.height());
    const int position = QStyle::sliderPositionFromValue(slider->minimum, slider->maximum,
                                                         slider->sliderPosition, span,
                                                         slider->upsideDown);
    QRectF filled = horizontal
                        ? QRectF(track.left(), track.top(), position, track.height())
                        : QRectF(track.left(), track.bottom() - position, track.width(), position);
    painter->setBrush(active);
    if(position > 0)
        painter->drawRoundedRect(filled, thickness / 2.0, thickness / 2.0);

    const QPointF center = horizontal ? QPointF(track.left() + position, track.center().y())
                                      : QPointF(track.center().x(), track.bottom() - position);
    painter->setBrush(active);
    painter->drawEllipse(center, handleSize / 2.0, handleSize / 2.0);
    painter->restore();
}

void MaterialStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                       QPainter *painter, const QWidget *widget) const {
    if(materialThemeActive() && control == CC_ComboBox) {
        // MD1 draws a combo as a text field, so the boxed frame Qlementine paints
        // is replaced by the same fill + underline the line edits use; the label
        // and the arrow still come from Qlementine's CE_ComboBoxLabel.
        const auto *combo = qstyleoption_cast<const QStyleOptionComboBox *>(option);
        if(combo) {
            drawFieldPanel(option, painter);
            return;
        }
    }
    if(materialThemeActive() && control == CC_SpinBox) {
        // A spin box is a field with two buttons: paint the field, then let
        // Qlementine paint only the up/down halves it owns. The value itself is
        // painted by the QLineEdit child, whose own panel is suppressed below.
        drawFieldPanel(option, painter);
        QlementineStyle::drawComplexControl(control, option, painter, widget);
        return;
    }
    if(control == CC_Slider && materialThemeActive()) {
        drawSlider(option, painter, widget);
        return;
    }
    QlementineStyle::drawComplexControl(control, option, painter, widget);
}

bool MaterialStyle::materialThemeActive() const {
    // The two themes qimgv ships are called "Material Light" and "Material
    // Dark"; the stock ones are "Light" and "Dark", so the prefix is enough and
    // does not depend on how the enum ids are ordered.
    return theme().meta.name.startsWith(QStringLiteral("Material"));
}

QFont MaterialStyle::buttonFont(const QStyleOption *option) const {
    Q_UNUSED(option)
    // QStyleOption carries only font metrics, so the theme font is the base.
    QFont font = theme().fontRegular;
    font.setPixelSize(qMax(theme().fontSize, kButtonFontSize));
    font.setWeight(QFont::Medium);
    font.setLetterSpacing(QFont::PercentageSpacing, (1.0 + kLetterSpacing) * 100.0);
    return font;
}

int MaterialStyle::pixelMetric(PixelMetric metric, const QStyleOption *option,
                               const QWidget *widget) const {
    if(materialThemeActive()) {
        // MDUI: 18dp checkbox, 20dp radio. Qlementine takes both from iconSize,
        // and moving that would resize every icon in the window.
        switch(metric) {
            case PM_IndicatorWidth:
            case PM_IndicatorHeight:
                return 18;
            case PM_ExclusiveIndicatorWidth:
            case PM_ExclusiveIndicatorHeight:
                return 18;
            case PM_SmallIconSize:
                return kMenuIconSize;
            default:
                break;
        }
    }
    return QlementineStyle::pixelMetric(metric, option, widget);
}

QSize MaterialStyle::sizeFromContents(ContentsType type, const QStyleOption *option,
                                      const QSize &size, const QWidget *widget) const {
    QSize contents = QlementineStyle::sizeFromContents(type, option, size, widget);
    if(!materialThemeActive())
        return contents;

    if(type == CT_MenuItem) {
        // MD1 menu rows are 32dp; Qlementine derives them from the font metrics.
        // Separators are CT_MenuItem too and must keep their thin line height.
        const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if(!item || item->menuItemType != QStyleOptionMenuItem::Separator)
            contents.setHeight(kMenuItemHeight);
        return contents;
    }

    if(type == CT_LineEdit) {
        // MD1 text fields are 48dp tall.
        contents.setHeight(qMax<int>(contents.height(), kFieldHeight));
        return contents;
    }

    if(type == CT_ItemViewItem) {
        // MD1 lists are 48dp rows, but a file tree at that pitch reads as airy
        // next to 14px text: the tree gets a desktop density instead.
        const bool tree = qobject_cast<const QTreeView *>(widget) != nullptr;
        contents.setHeight(qMax<int>(contents.height(), tree ? kTreeRowHeight : kListItemHeight));
        return contents;
    }

    if(type != CT_PushButton)
        return contents;

    const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
    if(!button)
        return contents;

    const QFontMetrics metrics(buttonFont(button));
    const int labelWidth = metrics.horizontalAdvance(button->text.toUpper());
    const int iconWidth = button->icon.isNull() ? 0 : button->iconSize.width() + 8;
    contents.setWidth(qMax(contents.width(),
                           qMax(kButtonMinWidth, labelWidth + iconWidth + 2 * kButtonPadding))
                      + 2 * kShadowMargin);
    contents.setHeight(qMax<int>(contents.height(), kButtonHeight) + 2 * kShadowMargin);
    return contents;
}

// MDUI's checkbox and radio: an 18dp shape with a 2dp border, the accent colour
// when on, and a translucent halo behind it for the state layer (hover, press,
// focus - the same opacities the buttons use).
void drawSelectorHalo(QPainter *painter, const QRectF &rect, const QColor &color, bool enabled, qreal alpha) {
    if(!enabled || alpha <= 0.0)
        return;
    painter->save();
    painter->setPen(Qt::NoPen);
    QColor halo = color;
    halo.setAlphaF(alpha);
    painter->setBrush(halo);
    const qreal extra = 6.0;
    painter->drawEllipse(rect.center(), rect.width() / 2.0 + extra, rect.height() / 2.0 + extra);
    painter->restore();
}

void drawCheckMark(QPainter *painter, const QRectF &rect, const QColor &color) {
    QPainterPath path;
    path.moveTo(rect.left() + rect.width() * 0.26, rect.top() + rect.height() * 0.52);
    path.lineTo(rect.left() + rect.width() * 0.44, rect.top() + rect.height() * 0.70);
    path.lineTo(rect.left() + rect.width() * 0.76, rect.top() + rect.height() * 0.30);
    painter->save();
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawPath(path);
    painter->restore();
}

void MaterialStyle::drawComboContent(const QStyleOptionComboBox *option, QPainter *painter,
                                     const QWidget *widget) const {
    const auto &theme = this->theme();
    const bool dark = theme.backgroundColorMain1.lightnessF() < 0.5;
    const bool enabled = option->state & State_Enabled;
    // Editable combos paint their text through the QLineEdit inside them; only
    // the arrow is ours then.
    if(!option->editable) {
        QPalette pal = option->palette;
        QColor textColor = enabled ? theme.secondaryColor : (dark ? QColor(255, 255, 255) : QColor(0, 0, 0));
        if(!enabled)
            textColor.setAlphaF(dark ? 0.30 : 0.26);
        pal.setColor(QPalette::WindowText, textColor);
        // Not subControlRect(): Qlementine computes SC_ComboBoxEditField from its
        // own frame, which leaves no room for the text in the new field.
        const QRect textRect = option->rect.adjusted(kFieldPadding, 0,
                                                     -(kFieldPadding + int(kChevronWidth) + 8), 0);
        painter->save();
        QFont font = theme.fontRegular;
        font.setPixelSize(theme.fontSize);
        painter->setFont(font);
        drawItemText(painter, textRect, Qt::AlignLeft | Qt::AlignVCenter, pal, enabled,
                     option->currentText, QPalette::WindowText);
        painter->restore();
    }

    // MD1 uses a single down chevron, not Qt's stacked up/down pair.
    QColor arrow = dark ? QColor(255, 255, 255) : QColor(0, 0, 0);
    arrow.setAlphaF(enabled ? 0.54 : (dark ? 0.30 : 0.26));
    const QRectF rect = QRectF(option->rect);
    const qreal w = kChevronWidth;
    const qreal h = kChevronHeight;
    const QPointF center(rect.right() - kFieldPadding - w / 2.0, rect.center().y());
    // A small filled caret reads better than a stroked chevron at this size.
    QPainterPath path;
    path.moveTo(center.x() - w / 2.0, center.y() - h / 2.0);
    path.lineTo(center.x() + w / 2.0, center.y() - h / 2.0);
    path.lineTo(center.x(), center.y() + h / 2.0);
    path.closeSubpath();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(arrow);
    painter->drawPath(path);
    painter->restore();
}

QRect MaterialStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                                    SubControl subControl, const QWidget *widget) const {
    QRect rect = QlementineStyle::subControlRect(control, option, subControl, widget);
    if(control == CC_SpinBox && subControl == SC_SpinBoxEditField && materialThemeActive())
        rect.adjust(kFieldPadding, 0, 0, 0);
    return rect;
}

QRect MaterialStyle::subElementRect(SubElement element, const QStyleOption *option,
                                    const QWidget *widget) const {
    QRect rect = QlementineStyle::subElementRect(element, option, widget);
    if(element == SE_LineEditContents && materialThemeActive()) {
        // MD1 insets the text by 16dp inside a filled field; Qlementine derives
        // its text rect from the frame width, which is 0 here because the field
        // has no outline.
        rect.adjust(kFieldPadding, 0, -kFieldPadding, 0);
    }
    return rect;
}

void MaterialStyle::drawFieldPanel(const QStyleOption *option, QPainter *painter) const {
    const auto &theme = this->theme();
    const bool dark = theme.backgroundColorMain1.lightnessF() < 0.5;
    const bool enabled = option->state & State_Enabled;
    const bool focused = enabled && (option->state & State_HasFocus);
    const QRectF rect = QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5);

    // MD1: a filled field with no outline, and a 1dp rule that turns into a 2dp
    // accent rule while the field has focus.
    QColor fill = theme.backgroundColorMain2;
    if(!enabled)
        fill = theme.backgroundColorMain1;
    QColor rule = dark ? QColor(255, 255, 255) : QColor(0, 0, 0);
    rule.setAlphaF(focused ? 1.0 : 0.42);
    if(focused)
        rule = theme.primaryColor;
    if(!enabled) {
        rule = dark ? QColor(255, 255, 255) : QColor(0, 0, 0);
        rule.setAlphaF(dark ? 0.30 : 0.26);
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(fill);
    painter->drawRoundedRect(rect, theme.borderRadius, theme.borderRadius);

    painter->setPen(QPen(rule, focused ? 2.0 : 1.0));
    const qreal y = rect.bottom();
    painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    painter->restore();
}

void MaterialStyle::drawSelectorPrimitive(PrimitiveElement element, const QStyleOption *option,
                                          QPainter *painter) const {
    const auto &theme = this->theme();
    const bool dark = theme.backgroundColorMain1.lightnessF() < 0.5;
    const QRectF rect = QRectF(option->rect);
    const bool enabled = option->state & State_Enabled;
    const bool hovered = enabled && (option->state & State_MouseOver);
    const bool focused = enabled && (option->state & State_HasFocus);
    const bool pressed = enabled && (option->state & State_Sunken);
    const bool on = option->state & State_On;
    const bool indeterminate = option->state & State_NoChange;
    const bool radio = element == PE_IndicatorRadioButton;

    // MDUI: unchecked border is 54% on-surface (70% in dark themes), the checked
    // shape is the accent colour, disabled drops to 26/30%.
    QColor onSurface = dark ? QColor(255, 255, 255) : QColor(0, 0, 0);
    QColor accent = theme.primaryColor;
    QColor border = onSurface;
    border.setAlphaF(dark ? 0.70 : 0.54);

    if(!enabled) {
        accent = onSurface;
        accent.setAlphaF(dark ? 0.30 : 0.26);
        border = accent;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // The halo is the checkbox's state layer: it replaces the plain hover tint of
    // the other controls, so it uses the same three opacities.
    qreal layerAlpha = 0.0;
    if(pressed)
        layerAlpha = kPressedAlpha;
    else if(hovered)
        layerAlpha = kHoverAlpha;
    if(focused)
        layerAlpha = qMax(layerAlpha, kFocusAlpha);
    if(!on)
        drawSelectorHalo(painter, rect, onSurface, enabled, layerAlpha);

    if(radio) {
        const QRectF outer = rect.adjusted(kSelectorBorder / 2.0, kSelectorBorder / 2.0,
                                           -kSelectorBorder / 2.0, -kSelectorBorder / 2.0);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(on ? accent : border, kSelectorBorder));
        painter->drawEllipse(outer);
        if(on) {
            // MDUI scales a 14dp dot to .68 of the 18dp circle.
            const qreal dot = outer.width() * 0.53;
            painter->setPen(Qt::NoPen);
            painter->setBrush(accent);
            painter->drawEllipse(outer.center(), dot / 2.0, dot / 2.0);
        }
    } else {
        const QRectF box = rect.adjusted(kSelectorBorder / 2.0, kSelectorBorder / 2.0,
                                         -kSelectorBorder / 2.0, -kSelectorBorder / 2.0);
        painter->setPen(QPen(on ? accent : border, kSelectorBorder));
        painter->setBrush(on ? QBrush(accent) : QBrush(Qt::NoBrush));
        painter->drawRoundedRect(box, kSelectorRadius, kSelectorRadius);
        if(on) {
            QColor mark = enabled ? theme.primaryColorForeground : onSurface;
            if(!indeterminate)
                drawCheckMark(painter, box, mark);
            else {
                painter->setPen(QPen(mark, kSelectorBorder, Qt::SolidLine, Qt::RoundCap));
                const qreal y = box.center().y();
                painter->drawLine(QPointF(box.left() + box.width() * 0.26, y),
                                  QPointF(box.left() + box.width() * 0.74, y));
            }
        }
    }
    painter->restore();
}

void MaterialStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                                  QPainter *painter, const QWidget *widget) const {
    if(materialThemeActive() && element == PE_PanelMenu) {
        // Qlementine gives a menu one wide shadow layer inside the margin its own
        // layout reserves for it. Replacing the shadow in that same margin keeps
        // the menu the same size and in the same place while giving it the MD3
        // two-layer elevation: a tight layer along the edge, a soft one for depth.
        const auto &theme = this->theme();
        const int padding = pixelMetric(PM_MenuPanelWidth, option, widget);
        const QRect frame = option->rect.marginsRemoved(QMargins(padding, padding, padding, padding));
        if(frame.width() > 0 && frame.height() > 0) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            UiStyle::paintElevation(painter, QRectF(frame), theme.borderRadius,
                                   UiStyle::kElevationSurface);
            const QRectF card = QRectF(frame).adjusted(0.5, 0.5, -0.5, -0.5);
            painter->setPen(QPen(menuBorderColor(), theme.borderWidth));
            painter->setBrush(menuBackgroundColor());
            painter->drawRoundedRect(card, theme.borderRadius, theme.borderRadius);
            painter->restore();
            return;
        }
    }
    if(materialThemeActive() && element == PE_PanelButtonTool) {
        // Tool buttons are not covered by the push-button painter (title bar,
        // dialog close button, toolbars, ...), so they get the state layer here.
        // The rest and checked fills still come from Qlementine: only the solid
        // hovered/pressed colour is replaced by the translucent layer.
        if(const auto *tool = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
            // QTabBar's scroll arrows have to stay opaque to hide the tabs
            // scrolling underneath them.
            if(widget && !qobject_cast<QTabBar *>(widget->parentWidget())) {
                const auto &theme = this->theme();
                QStyleOptionToolButton rest = *tool;
                rest.state &= ~(State_MouseOver | State_Sunken | State_HasFocus);
                QlementineStyle::drawPrimitive(element, &rest, painter, widget);
                drawStateLayer(painter, QRectF(tool->rect), theme.borderRadius,
                              onSurfaceColor(theme), tool);
                return;
            }
        }
    }
    if(materialThemeActive() && element == PE_PanelItemViewItem) {
        // Lists, trees, tables and combo popups: Qlementine paints a hovered row
        // with a solid neutral colour, which next to the selection read as a
        // second highlight. An unselected row now gets the MD3 state layer; a
        // selected row keeps Qlementine's selection fill.
        if(const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            if(!(item->state & State_Selected)) {
                const auto &theme = this->theme();
                QStyleOptionViewItem rest = *item;
                rest.state &= ~(State_MouseOver | State_Sunken | State_HasFocus);
                QlementineStyle::drawPrimitive(element, &rest, painter, widget);
                drawStateLayer(painter, QRectF(item->rect), 0.0, onSurfaceColor(theme), item);
                return;
            }
        }
    }
    if(materialThemeActive() && element == PE_PanelTipLabel) {
        // MD1 tooltips are a dark surface with white text, unlike Qlementine's
        // light one, which disappeared over light images.
        const bool dark = theme().backgroundColorMain1.lightnessF() < 0.5;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(dark ? QColor(0x42, 0x42, 0x42) : QColor(0x61, 0x61, 0x61));
        painter->drawRoundedRect(QRectF(option->rect), 2, 2);
        painter->restore();
        return;
    }
    if(materialThemeActive() && element == PE_PanelLineEdit) {
        // A QSpinBox owns a QLineEdit for its value; that one sits inside a field
        // this style already painted, so it must stay transparent.
        const bool insideSpinBox = widget && qobject_cast<const QAbstractSpinBox *>(widget->parentWidget());
        if(!insideSpinBox)
            drawFieldPanel(option, painter);
        return;
    }
    if(materialThemeActive()
       && (element == PE_IndicatorCheckBox || element == PE_IndicatorRadioButton)) {
        drawSelectorPrimitive(element, option, painter);
        return;
    }
    if(element == PE_FrameFocusRect && materialThemeActive()) {
        // MD1 shows focus by other means: a scrim on buttons, the accent rule on
        // fields. Qlementine paints its ring from a separate focus-frame widget,
        // so the ring is identified through the focused widget instead.
        const QWidget *focused = QApplication::focusWidget();
        if(qobject_cast<const QPushButton *>(focused) || qobject_cast<const QComboBox *>(focused)
           || qobject_cast<const QLineEdit *>(focused) || qobject_cast<const QAbstractSpinBox *>(focused))
            return;
    }
    QlementineStyle::drawPrimitive(element, option, painter, widget);
}

void MaterialStyle::drawControl(ControlElement element, const QStyleOption *option,
                                QPainter *painter, const QWidget *widget) const {
    if(element == CE_ComboBoxLabel && materialThemeActive()) {
        // QComboBox::paintEvent draws CC_ComboBox and CE_ComboBoxLabel as two
        // steps, so the arrow has to be replaced here - not in CC_ComboBox.
        if(const auto *combo = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            drawComboContent(combo, painter, widget);
            return;
        }
    }

    if(element != CE_PushButton || !materialThemeActive()) {
        QlementineStyle::drawControl(element, option, painter, widget);
        return;
    }

    const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
    if(!button) {
        QlementineStyle::drawControl(element, option, painter, widget);
        return;
    }

    const auto &theme = this->theme();
    const bool dark = theme.backgroundColorMain1.lightnessF() < 0.5;
    const QColor onSurface = dark ? QColor(255, 255, 255) : QColor(0, 0, 0);
    const bool enabled = button->state & State_Enabled;
    const bool hovered = enabled && (button->state & State_MouseOver);
    const bool pressed = enabled && (button->state & (State_Sunken | State_On));
    // QDialogButtonBox, QMessageBox and friends flag their default button; that
    // is the one Material renders as a raised button. Dialogs built from a plain
    // .ui have no such flag, so they can mark their accept button with the
    // "materialPrimary" property instead (see SettingsDialog).
    const bool raised = button->features.testFlag(QStyleOptionButton::DefaultButton)
                        || (widget && widget->property("materialPrimary").toBool());

    const QRectF body = QRectF(button->rect).adjusted(kShadowMargin + 0.5, kShadowMargin + 0.5,
                                                      -kShadowMargin - 0.5, -kShadowMargin - 0.5);
    const qreal radius = theme.borderRadius > 0 ? theme.borderRadius : kButtonRadius;

    QColor fill = Qt::transparent;
    QColor label;
    // Base colour of the state layer: the on-primary colour over a raised
    // (primary filled) button, the on-surface colour over a flat one.
    QColor stateLayerColor;
    if(raised) {
        if(!enabled) {
            // MD1: disabled raised buttons keep elevation 2 and become a 12%
            // on-surface wash.
            painter->drawPixmap(button->rect.topLeft(), shadowFor(button->rect.size(), radius, 2));
            fill = onSurface;
            fill.setAlphaF(0.12);
            label = onSurface;
            label.setAlphaF(dark ? 0.30 : 0.26);
        } else {
            const int elevation = pressed ? 8 : hovered ? 4 : 2;
            painter->drawPixmap(button->rect.topLeft(), shadowFor(button->rect.size(), radius, elevation));
            fill = theme.primaryColor;
            label = theme.primaryColorForeground;
            stateLayerColor = theme.primaryColorForeground;
        }
    } else {
        label = enabled ? theme.secondaryColor : onSurface;
        if(!enabled)
            label.setAlphaF(dark ? 0.30 : 0.26);
        // A flat button is the surface: every state it has is the layer.
        stateLayerColor = onSurface;
    }

    if(fill.alphaF() > 0.0) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(body, radius, radius);
        painter->restore();
    }
    // Material 3 replaces the old per-state colours with one translucent layer:
    // hover 8%, press 12%, focus 16% of the on-surface (or on-primary) colour.
    // Qlementine's focus ring is suppressed by clearing State_HasFocus below.
    if(stateLayerColor.isValid())
        drawStateLayer(painter, body, radius, stateLayerColor, button);

    // The label is drawn by the base implementation, which reads the colour
    // from the option palette and the font from the painter. MD1 asks for an
    // uppercase, medium, slightly tracked label.
    QStyleOptionButton labelled = *button;
    labelled.rect = body.toRect();
    labelled.text = button->text.toUpper();
    labelled.state &= ~State_HasFocus;
    labelled.palette.setColor(QPalette::ButtonText, label);
    painter->save();
    painter->setFont(buttonFont(button));
    painter->setPen(label);
    QCommonStyle::drawControl(CE_PushButtonLabel, &labelled, painter, widget);
    painter->restore();
}
