#include "materialdialog.h"

#include <QBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QWindow>

#include "uistyle.h"

#ifdef USE_QLEMENTINE
#include <oclero/qlementine/utils/ImageUtils.hpp>
#endif

namespace {
// No margin: this window is not reliably translucent, so anything painted
// outside the card shows as a flat band instead of a shadow (the same problem
// the context menu hit). The card is full-bleed with a 1px border instead.
constexpr int kShadowMargin = 0;
constexpr int kTitleBarHeight = 44;
constexpr int kResizeBorder = 5;
constexpr int kShadowQuantization = 16;

QPixmap dialogShadow(const QSize &cardSize, qreal radius, const QColor &color) {
    // Dragging the resize border changes the size on every mouse move, and each
    // new size used to rebuild the blur (5-15ms for a dialog this size). The
    // cache is keyed on a coarse grid instead: the shadow is centred on the card
    // when drawn, so being up to kShadowQuantization/2 off is invisible.
    const auto quantize = [](int value) {
        return ((value + kShadowQuantization - 1) / kShadowQuantization) * kShadowQuantization;
    };
    const QSize shadowSize(quantize(cardSize.width()), quantize(cardSize.height()));

    static QHash<QString, QPixmap> cache;
    const QString key = QStringLiteral("%1x%2|%3")
                            .arg(shadowSize.width()).arg(shadowSize.height()).arg(qRound(radius));
    const auto cached = cache.constFind(key);
    if(cached != cache.constEnd())
        return cached.value();

    QPixmap shadow = oclero::qlementine::getDropShadowPixmap(shadowSize, radius, kShadowMargin, color);
    if(cache.size() > 4)
        cache.clear();
    cache.insert(key, shadow);
    return shadow;
}

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
