#include "materialdialog.h"

#include <QBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QWindow>

#include "uistyle.h"

namespace {
// The card floats on the desktop, so it gets the MD3 level-2 elevation. That
// needs a margin inside the (frameless, translucent) window for the shadow; the
// root layout gets the same margin, which keeps the card where a full-bleed one
// would be.
//
// The margin follows from how Qlementine builds a shadow pixmap: it extends the
// surface by 2 * ceil(blur) on every side before blurring. The floating elevation
// uses ambient blur 8, so the layer reaches 16px past the card and, shifted down
// by its 3px offset, 19px at the bottom; the key layer (blur 6, offset 2) needs
// 14px. 20 is the smallest round margin that contains both, and it keeps the
// window small enough for a 1280x800 screen.
constexpr int kShadowMargin = 20;
constexpr int kTitleBarHeight = 44;
// The card edge sits kShadowMargin inside the window, so the shadow margin is
// also the grab area for resizing: grabbing at the card border starts the resize
// instead of doing nothing.
constexpr int kResizeBorder = kShadowMargin;

// The title bar only has to be draggable; letting the window manager do the move
// keeps snapping and multi-monitor behaviour native.
class DialogTitleBar : public QWidget {
public:
    explicit DialogTitleBar(QWidget *parent) : QWidget(parent) {}

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button() == Qt::LeftButton) {
            if(QWindow *handle = window()->windowHandle()) {
                handle->startSystemMove();
                event->accept();
                return;
            }
        }
        QWidget::mousePressEvent(event);
    }
};
} // namespace

MaterialDialog::MaterialDialog(QWidget *parent) : QDialog(parent) {
}

void MaterialDialog::enableMaterialChrome() {
    if(mChrome || !UiStyle::materialTheme())
        return;

    mChrome = true;
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);

    auto *root = layout();
    auto *rootBox = qobject_cast<QBoxLayout *>(root);
    if(!root || !rootBox)
        return;
    rootBox->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);

    auto *bar = new DialogTitleBar(this);
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(16, 0, 8, 0);
    barLayout->setSpacing(8);

    auto *title = new QLabel(windowTitle(), bar);
    QFont titleFont = font();
    titleFont.setPixelSize(16);
    titleFont.setWeight(QFont::Medium);
    title->setFont(titleFont);
    barLayout->addWidget(title);
    barLayout->addStretch(1);

    auto *close = new QToolButton(bar);
    close->setText(QStringLiteral("\u2715"));
    close->setAutoRaise(true);
    close->setCursor(Qt::ArrowCursor);
    close->setToolTip(tr("Close"));
    connect(close, &QToolButton::clicked, this, &QDialog::close);
    barLayout->addWidget(close);

    bar->setFixedHeight(kTitleBarHeight);
    rootBox->insertWidget(0, bar);
    mTitleBar = bar;
}

void MaterialDialog::paintEvent(QPaintEvent *event) {
    if(!mChrome) {
        QDialog::paintEvent(event);
        return;
    }
    UiStyle::ChromePalette pal;
    if(!UiStyle::chromePalette(pal)) {
        QDialog::paintEvent(event);
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF card = QRectF(rect()).adjusted(kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);
    if(card.width() <= 0 || card.height() <= 0)
        return;

    // Level-2 elevation behind the card: the same two-layer shadow the style
    // paints around its menus, so the dialog and the popups belong to one family.
    UiStyle::paintElevation(&painter, card, pal.radius, UiStyle::kElevationFloating);

    painter.setPen(QPen(pal.surfaceLine, 1));
    painter.setBrush(pal.surface);
    painter.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), pal.radius, pal.radius);
}

Qt::Edges MaterialDialog::resizeEdgesAt(const QPoint &pos) const {
    Qt::Edges edges;
    if(pos.x() <= kResizeBorder)
        edges |= Qt::LeftEdge;
    else if(pos.x() >= width() - kResizeBorder)
        edges |= Qt::RightEdge;
    if(pos.y() <= kResizeBorder)
        edges |= Qt::TopEdge;
    else if(pos.y() >= height() - kResizeBorder)
        edges |= Qt::BottomEdge;
    return edges;
}

void MaterialDialog::updateCursor(const QPoint &pos) {
    const Qt::Edges edges = resizeEdgesAt(pos);
    if((edges & Qt::LeftEdge && edges & Qt::TopEdge) || (edges & Qt::RightEdge && edges & Qt::BottomEdge))
        setCursor(Qt::SizeFDiagCursor);
    else if((edges & Qt::RightEdge && edges & Qt::TopEdge) || (edges & Qt::LeftEdge && edges & Qt::BottomEdge))
        setCursor(Qt::SizeBDiagCursor);
    else if(edges & (Qt::LeftEdge | Qt::RightEdge))
        setCursor(Qt::SizeHorCursor);
    else if(edges & (Qt::TopEdge | Qt::BottomEdge))
        setCursor(Qt::SizeVerCursor);
    else
        unsetCursor();
}

bool MaterialDialog::event(QEvent *event) {
    if(!mChrome)
        return QDialog::event(event);

    switch(event->type()) {
        case QEvent::MouseMove: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            updateCursor(mouseEvent->position().toPoint());
            break;
        }
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if(mouseEvent->button() == Qt::LeftButton) {
                const Qt::Edges edges = resizeEdgesAt(mouseEvent->position().toPoint());
                if(edges != Qt::Edges() && windowHandle()) {
                    windowHandle()->startSystemResize(edges);
                    return true;
                }
            }
            break;
        }
        case QEvent::Leave:
            unsetCursor();
            break;
        default:
            break;
    }
    return QDialog::event(event);
}
