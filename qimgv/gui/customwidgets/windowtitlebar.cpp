#include "windowtitlebar.h"

#include <QApplication>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWindow>

#include <functional>

#include "uistyle.h"

namespace {
constexpr int kBarHeight = 44;
// 20 px matches the smallest hand-drawn app PNG that is still legible at the
// title bar height; the ladder below picks an exact device-pixel frame.
constexpr int kIconSize = 20;
constexpr int kLeftMargin = 14;
constexpr int kIconGap = 10;
constexpr int kButtonWidth = 34;
constexpr int kButtonHeight = 28;
constexpr int kButtonRadius = 2;
constexpr int kRightMargin = 6;

// A window button painted by hand: the three glyphs are two lines and a square,
// which is cheaper (and sharper) than shipping icons for them.
class WindowButton : public QWidget {
public:
    enum class Glyph { Minimize, Maximize, Restore, Close };

    WindowButton(Glyph glyph, std::function<void()> onClick, const QString &name, QWidget *parent)
        : QWidget(parent), mGlyph(glyph), mOnClick(std::move(onClick)) {
        setFixedSize(kButtonWidth, kButtonHeight);
        setCursor(Qt::ArrowCursor);
        setAccessibleName(name);
        setFocusPolicy(Qt::NoFocus);
    }

    void setGlyph(Glyph glyph) {
        if(mGlyph != glyph) {
            mGlyph = glyph;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override {
        UiStyle::ChromePalette pal;
        if(!UiStyle::chromePalette(pal))
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        if(mHovered) {
            QColor scrim = (mGlyph == Glyph::Close) ? pal.error : pal.text;
            scrim.setAlphaF(mGlyph == Glyph::Close ? 0.14f : 0.10f);
            painter.setPen(Qt::NoPen);
            painter.setBrush(scrim);
            painter.drawRoundedRect(body, kButtonRadius, kButtonRadius);
        }

        QColor glyphColor = (mGlyph == Glyph::Close && mHovered) ? pal.error : pal.text;
        painter.setPen(QPen(glyphColor, 1.3));
        painter.setBrush(Qt::NoBrush);

        const QPointF c = QRectF(rect()).center();
        const qreal s = 5.0;
        switch(mGlyph) {
            case Glyph::Minimize:
                painter.drawLine(QPointF(c.x() - s, c.y()), QPointF(c.x() + s, c.y()));
                break;
            case Glyph::Maximize:
                painter.drawRect(QRectF(c.x() - s, c.y() - s, 2 * s, 2 * s));
                break;
            case Glyph::Restore:
                painter.drawRect(QRectF(c.x() - s + 2, c.y() - s + 2, 2 * s - 2, 2 * s - 2));
                painter.drawLine(QPointF(c.x() - s + 2, c.y() - s + 2), QPointF(c.x() - s, c.y() - s));
                painter.drawLine(QPointF(c.x() - s, c.y() - s), QPointF(c.x() + s - 2, c.y() - s));
                painter.drawLine(QPointF(c.x() + s - 2, c.y() - s), QPointF(c.x() + s - 2, c.y() + s - 4));
                painter.drawLine(QPointF(c.x() - s + 2, c.y() - s + 2), QPointF(c.x() - s, c.y() - s + 2));
                break;
            case Glyph::Close:
                painter.drawLine(QPointF(c.x() - s, c.y() - s), QPointF(c.x() + s, c.y() + s));
                painter.drawLine(QPointF(c.x() + s, c.y() - s), QPointF(c.x() - s, c.y() + s));
                break;
        }
    }

    void enterEvent(QEnterEvent *) override {
        mHovered = true;
        update();
    }

    void leaveEvent(QEvent *) override {
        mHovered = false;
        update();
    }

    void mousePressEvent(QMouseEvent *event) override {
        // Claim the press: otherwise it bubbles up to the title bar, which starts
        // a window move and swallows the click before release.
        if(event->button() == Qt::LeftButton) {
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if(event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            if(mOnClick)
                mOnClick();
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    Glyph mGlyph;
    std::function<void()> mOnClick;
    bool mHovered = false;
};
} // namespace

WindowTitleBar::WindowTitleBar(QWidget *parent) : QWidget(parent) {
    setFixedHeight(kBarHeight);
    setCursor(Qt::ArrowCursor);
    setAccessibleName("WindowTitleBar");

    // Draw a hand-drawn frame at its own size instead of rasterising the vector:
    // at 125 % the request lands on 25 device pixels, and an SVG rendered at that
    // size (then scaled into the 20 px slot) came out soft next to the pixel art
    // the rest of the window icon uses.
    const int device = qRound(kIconSize * qMax(1.0, devicePixelRatioF()));
    static const int kFrames[] = {16, 20, 22, 24, 30, 32, 36, 40};
    int frame = kFrames[0];
    for(int size : kFrames) {
        if(qAbs(size - device) < qAbs(frame - device))
            frame = size;
    }
    mIcon = QPixmap(QStringLiteral(":/res/icons/common/logo/app/%1.png").arg(frame));
    mIcon.setDevicePixelRatio(qreal(frame) / qreal(kIconSize));

    createButtons();
}

WindowTitleBar::~WindowTitleBar() = default;

void WindowTitleBar::createButtons() {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, kRightMargin, 0);
    layout->setSpacing(0);
    layout->addStretch(1);

    mButtonBox = new QWidget(this);
    mButtonLayout = new QHBoxLayout(mButtonBox);
    mButtonLayout->setContentsMargins(0, 0, 0, 0);
    mButtonLayout->setSpacing(2);

    mMinimizeButton = new WindowButton(WindowButton::Glyph::Minimize,
                                       [this]() { window()->showMinimized(); },
                                       tr("Minimize"), mButtonBox);
    mMaximizeButton = new WindowButton(WindowButton::Glyph::Maximize,
                                       [this]() { toggleMaximized(); },
                                       tr("Maximize"), mButtonBox);
    mCloseButton = new WindowButton(WindowButton::Glyph::Close,
                                    [this]() { window()->close(); },
                                    tr("Close"), mButtonBox);

    mButtonLayout->addWidget(mMinimizeButton);
    mButtonLayout->addWidget(mMaximizeButton);
    mButtonLayout->addWidget(mCloseButton);

    layout->addWidget(mButtonBox);
}

void WindowTitleBar::setTitle(const QString &title) {
    if(mTitle == title)
        return;
    mTitle = title;
    update();
}

void WindowTitleBar::setMaximized(bool maximized) {
    if(mMaximized == maximized)
        return;
    mMaximized = maximized;
    if(auto *button = static_cast<WindowButton *>(mMaximizeButton))
        button->setGlyph(maximized ? WindowButton::Glyph::Restore : WindowButton::Glyph::Maximize);
    mMaximizeButton->setAccessibleName(maximized ? tr("Restore") : tr("Maximize"));
}

void WindowTitleBar::toggleMaximized() {
    QWidget *w = window();
    if(!w)
        return;
    if(w->isMaximized())
        w->showNormal();
    else
        w->showMaximized();
}

void WindowTitleBar::paintEvent(QPaintEvent *) {
    UiStyle::ChromePalette pal;
    const bool themed = UiStyle::chromePalette(pal);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), themed ? pal.surface : palette().color(QPalette::Window));
    if(themed) {
        painter.setPen(pal.surfaceLine);
        painter.drawLine(0, height() - 1, width(), height() - 1);
    }

    // application icon
    const int iconY = (height() - kIconSize) / 2;
    if(!mIcon.isNull())
        painter.drawPixmap(kLeftMargin, iconY, mIcon);

    // window title, elided in the middle so a long file name keeps its extension
    QFont titleFont = font();
    titleFont.setPixelSize(14);
    titleFont.setWeight(QFont::Medium);
    painter.setFont(titleFont);
    painter.setPen(themed ? pal.text : palette().color(QPalette::WindowText));

    const int textLeft = kLeftMargin + kIconSize + kIconGap;
    const int textRight = mButtonBox ? mButtonBox->x() - kIconGap : width() - kRightMargin;
    const QRect textRect(textLeft, 0, qMax(0, textRight - textLeft), height());
    if(textRect.width() > 0) {
        const QString elided = QFontMetrics(titleFont).elidedText(mTitle, Qt::ElideMiddle, textRect.width());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
    }
}

void WindowTitleBar::mousePressEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        QWidget *w = window();
        QWindow *handle = w ? w->windowHandle() : nullptr;
        if(handle) {
            if(w->isMaximized()) {
                // Windows restores a maximized window when its title bar is
                // dragged. Do the same: put back the size it will restore to and
                // keep the cursor over the same spot of the bar.
                const QRect maximizedRect = w->geometry();
                const QPoint globalPos = event->globalPosition().toPoint();
                const qreal fraction = maximizedRect.width() > 0
                                           ? qreal(globalPos.x() - maximizedRect.left()) / maximizedRect.width()
                                           : 0.5;
                w->showNormal();
                const QRect restored = w->geometry();
                w->move(globalPos.x() - qRound(fraction * restored.width()),
                        globalPos.y() - event->position().toPoint().y());
            }
            // let the window manager run the move: snapping, multi-monitor and
            // the "restore on drag" behaviour stay native
            handle->startSystemMove();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void WindowTitleBar::mouseDoubleClickEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        toggleMaximized();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void WindowTitleBar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    update();
}
