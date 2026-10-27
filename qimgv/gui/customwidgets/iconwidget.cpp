#include "iconwidget.h"
#include "uistyle.h"

#include <QEvent>
#include <QFontMetrics>
#include <QPainterPath>
#include <QRegularExpression>
#include <QIcon>

namespace {

// Backgrounds qimgv's own buttons used to get from the stylesheet, through
// [accessibleName="..."] selectors in style-template.qss. Qlementine replaces
// that stylesheet with a real QStyle, so nothing paints these widgets any more;
// the same states are drawn here instead, from the colour scheme. Radii and
// insets are copied from the corresponding rules.
struct ButtonChrome {
    bool visible = false;
    QColor fillColor;
    bool border = false;
    QColor borderColor;
    bool leftDivider = false;
    QMargins inset;
    qreal radiusTL = 0.0;
    qreal radiusTR = 0.0;
    qreal radiusBR = 0.0;
    qreal radiusBL = 0.0;
};

ButtonChrome chromeFor(const QString &name, bool hovered, bool pressed, bool checked,
                       const ColorScheme &c)
{
    ButtonChrome ch;
    // "[pressed=true]" and "[checked=true]" share one fill in every rule
    const bool active = pressed || checked;
    const auto uniformRadius = [&ch](qreal r) {
        ch.radiusTL = ch.radiusTR = ch.radiusBR = ch.radiusBL = r;
    };
    const auto solid = [&ch](const QColor &color) {
        ch.visible = true;
        ch.fillColor = color;
    };
    const auto solidWithBorder = [&ch](const QColor &color) {
        ch.visible = true;
        ch.border = true;
        ch.fillColor = color;
        ch.borderColor = color;
    };

    if(name == QLatin1String("Button")) {
        // overlay buttons: always filled
        solid(active ? c.button_pressed : (hovered ? c.button_hover : c.button));
        uniformRadius(3);
    } else if(name == QLatin1String("ButtonSetLeft")) {
        solid(active ? c.button_pressed : (hovered ? c.button_hover : c.button));
        ch.radiusTL = ch.radiusBL = 3;
    } else if(name == QLatin1String("ButtonSetRight")) {
        solid(active ? c.button_pressed : (hovered ? c.button_hover : c.button));
        ch.radiusTR = ch.radiusBR = 3;
        ch.leftDivider = true;
        ch.borderColor = c.button_pressed;
    } else if(name == QLatin1String("ButtonSmall")) {
        uniformRadius(1);
        if(checked)
            solidWithBorder(c.folderview_hc);
        else if(hovered)
            solidWithBorder(c.folderview_hc2);
    } else if(name == QLatin1String("PanelButton") || name == QLatin1String("CheckableButton")) {
        uniformRadius(3);
        ch.inset = QMargins(2, 4, 2, 4);
        if(active)
            solidWithBorder(c.panel_button);
        else if(hovered)
            solidWithBorder(c.panel_button_hover);
    } else if(name == QLatin1String("PanelButtonRE")) {
        uniformRadius(3);
        ch.inset = QMargins(2, 4, 4, 4);
        if(active)
            solidWithBorder(c.panel_button);
        else if(hovered)
            solidWithBorder(c.panel_button_hover);
    } else if(name == QLatin1String("CheckableButtonLE")) {
        uniformRadius(3);
        ch.inset = QMargins(4, 4, 2, 4);
        if(active)
            solidWithBorder(c.panel_button);
        else if(hovered)
            solidWithBorder(c.panel_button_hover);
    } else if(name == QLatin1String("PathBarButton")) {
        ch.radiusTL = ch.radiusBL = 3;
        ch.inset = QMargins(2, 4, 2, 4);
        if(active)
            solidWithBorder(c.panel_button_pressed);
        else if(hovered)
            solidWithBorder(c.panel_button_hover);
    } else if(name == QLatin1String("PlacesPanelButton")) {
        uniformRadius(3);
        if(active)
            solid(c.folderview_button_pressed);
        else if(hovered)
            solid(c.folderview_button_hover);
    } else if(name == QLatin1String("OverlayHeaderButton")) {
        uniformRadius(3);
        ch.inset = QMargins(3, 3, 3, 3);
        if(active)
            solid(c.button_pressed);
        else if(hovered)
            solid(c.button_hover);
    } else if(name == QLatin1String("ContextMenuButton")) {
        if(hovered) {
            QColor accent = c.accent;
            accent.setAlphaF(0.65f);
            solid(accent);
        }
    }
    return ch;
}

QPainterPath roundedPath(const QRectF &rect, qreal tl, qreal tr, qreal br, qreal bl)
{
    QPainterPath path;
    path.moveTo(rect.left() + tl, rect.top());
    path.lineTo(rect.right() - tr, rect.top());
    if(tr > 0)
        path.arcTo(QRectF(rect.right() - 2 * tr, rect.top(), 2 * tr, 2 * tr), 90, -90);
    path.lineTo(rect.right(), rect.bottom() - br);
    if(br > 0)
        path.arcTo(QRectF(rect.right() - 2 * br, rect.bottom() - 2 * br, 2 * br, 2 * br), 0, -90);
    path.lineTo(rect.left() + bl, rect.bottom());
    if(bl > 0)
        path.arcTo(QRectF(rect.left(), rect.bottom() - 2 * bl, 2 * bl, 2 * bl), 270, -90);
    path.lineTo(rect.left(), rect.top() + tl);
    if(tl > 0)
        path.arcTo(QRectF(rect.left(), rect.top(), 2 * tl, 2 * tl), 180, -90);
    path.closeSubpath();
    return path;
}

}

IconWidget::IconWidget(QWidget *parent)
    : QWidget(parent),
      hiResPixmap(false),
      pixmap(nullptr)
{
    dpr = this->devicePixelRatioF();
    color = settings->colorScheme().icons;
    connect(settings, &Settings::settingsChanged, this, &IconWidget::onSettingsChanged);
}

IconWidget::~IconWidget() {
    if(pixmap)
        delete pixmap;
}

void IconWidget::onSettingsChanged() {
    if(colorMode == ICON_COLOR_THEME && color != settings->colorScheme().icons) {
        color = settings->colorScheme().icons;
        applyColor();
    }
}

void IconWidget::setIconPath(QString path) {
    if(iconPath == path)
        return;
    iconPath = path;
    loadIcon();
}

void IconWidget::loadIcon() {
    auto path = iconPath;
    if(pixmap)
        delete pixmap;
    pixmap = nullptr;

    // vector icon from the active icon theme, if there is an equivalent
    const QString themed = UiStyle::themedIconName(iconPath);
    if(!themed.isEmpty()) {
        int size = 16;
        static const QRegularExpression sizeRe("\\.?(\\d+)\\.png$");
        const auto match = sizeRe.match(iconPath);
        if(match.hasMatch())
            size = match.captured(1).toInt();
        const qreal scale = qMax(1.0, dpr);
        QPixmap pm = QIcon(QStringLiteral(":/qlementine/icons/16/%1.svg").arg(themed))
                         .pixmap(QSize(size, size) * scale);
        if(!pm.isNull()) {
            pm.setDevicePixelRatio(scale);
            pixmap = new QPixmap(pm);
            pixmapDrawScale = scale;
            hiResPixmap = scale > 1.0;
            applyColor();
            update();
            return;
        }
    }

    if(dpr >= (1.0 + 0.001)) {
        path.replace(".", "@2x.");
        hiResPixmap = true;
        pixmap = new QPixmap(path);
        if(dpr >= (2.0 - 0.001))
            pixmapDrawScale = dpr;
        else
            pixmapDrawScale = 2.0;
        pixmap->setDevicePixelRatio(pixmapDrawScale);
    } else {
        hiResPixmap = false;
        pixmap = new QPixmap(path);
        pixmapDrawScale = dpr;
    }
    applyColor();
    if(pixmap->isNull()) {
        delete pixmap;
        pixmap = nullptr;
    }
    update();
}

QSize IconWidget::minimumSizeHint() const {
    if(pixmap && !pixmap->isNull())
        return pixmap->size() / dpr;
    else
        return QWidget::minimumSizeHint();
}

void IconWidget::setIconOffset(int x, int y) {
    iconOffset.setX(x);
    iconOffset.setY(y);
    update();
}

void IconWidget::setColorMode(IconColorMode _mode) {
    if(colorMode != _mode && _mode == ICON_COLOR_SOURCE) {
        colorMode = _mode;
        // reload uncolored
        loadIcon();
    } else {
        colorMode = _mode;
        applyColor();
    }
}

void IconWidget::setColor(QColor _color) {
    colorMode = ICON_COLOR_CUSTOM;
    color = _color;
    applyColor();
}

void IconWidget::applyColor() {
    if(!pixmap || pixmap->isNull() || colorMode == ICON_COLOR_SOURCE)
        return;
    ImageLib::recolor(*pixmap, color);
}

void IconWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter p(this);
    if(!this->isEnabled())
        p.setOpacity(0.5f);
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    paintChrome(p);
    if(pixmap) {
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QPointF pos;
        if(hiResPixmap) {
            pos = QPointF(width()  / 2 - pixmap->width()  / (2 * pixmapDrawScale),
                          height() / 2 - pixmap->height() / (2 * pixmapDrawScale));
        } else {
            pos = QPointF(width()  / 2 - pixmap->width()  / 2,
                          height() / 2 - pixmap->height() / 2);
        }
        p.drawPixmap(pos + iconOffset, *pixmap);
    }
}

bool IconWidget::event(QEvent *event) {
    switch(event->type()) {
    case QEvent::Enter:
        if(!mHovered) {
            mHovered = true;
            update();
        }
        break;
    case QEvent::Leave:
        if(mHovered) {
            mHovered = false;
            update();
        }
        break;
    case QEvent::Polish:
        applyChromeMetrics();
        break;
    default:
        break;
    }
    return QWidget::event(event);
}

// Sizes the stylesheet used to supply through min-width/min-height. The .ui
// files carry no geometry for these, so without it the buttons shrink to the
// size of their pixmap.
void IconWidget::applyChromeMetrics() {
    if(mChromeMetricsDone)
        return;
    mChromeMetricsDone = true;
    if(!UiStyle::useQlementine())
        return;
    if(accessibleName() == QLatin1String("OverlayHeaderButton")) {
        const QFontMetrics fm(font());
        const int textHeight = fm.height();
        const int textPadding = static_cast<int>(textHeight * 0.10f);
        const int size = qMax(textHeight + textPadding * 2, 30);
        setMinimumSize(size, size);
    }
}

void IconWidget::paintChrome(QPainter &painter) {
    // In classic mode the stylesheet paints these widgets, see style-template.qss
    if(!UiStyle::useQlementine())
        return;
    const QString name = accessibleName();
    if(name.isEmpty())
        return;
    const ButtonChrome chrome = chromeFor(name,
                                          mHovered,
                                          property("pressed").toBool(),
                                          property("checked").toBool(),
                                          settings->colorScheme());
    if(!chrome.visible)
        return;
    QRectF r = QRectF(rect()).adjusted(chrome.inset.left(), chrome.inset.top(),
                                       -chrome.inset.right(), -chrome.inset.bottom());
    if(r.width() < 2.0 || r.height() < 2.0)
        return;
    r.adjust(0.5, 0.5, -0.5, -0.5);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPainterPath path = roundedPath(r, chrome.radiusTL, chrome.radiusTR,
                                          chrome.radiusBR, chrome.radiusBL);
    painter.fillPath(path, chrome.fillColor);
    if(chrome.border) {
        painter.setPen(QPen(chrome.borderColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }
    if(chrome.leftDivider) {
        painter.setPen(QPen(chrome.borderColor, 1.0));
        painter.drawLine(QPointF(r.left(), r.top()), QPointF(r.left(), r.bottom()));
    }
    painter.restore();
}
