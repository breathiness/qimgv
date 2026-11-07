#include "contextmenu.h"
#include "ui_contextmenu.h"

#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QStyleOption>
#include <QPair>
#include <QVector>

#include "uistyle.h"

#ifdef USE_QLEMENTINE
#include <oclero/qlementine/utils/ImageUtils.hpp>
#endif

namespace {
// The popup paints its own card, so it keeps a margin for the drop shadow the
// way Qlementine's own menus do (their window is translucent and the shadow is
// drawn inside it).
constexpr int kCardMargin = 8;

// Icon-only buttons have no room for a key column, so their key goes into the
// tooltip instead: "Fit window (1)".
QString actionToolTip(const QString &action) {
    const QString key = actionManager->primaryShortcutForAction(action);
    const QString name = appActions->displayName(action);
    return key.isEmpty() ? name : QStringLiteral("%1 (%2)").arg(name, key);
}

QPixmap shadowFor(const QSize &cardSize, qreal radius, const QColor &color) {
    static QHash<QString, QPixmap> cache;
    const QString key = QStringLiteral("%1x%2|%3")
                            .arg(cardSize.width()).arg(cardSize.height()).arg(qRound(radius));
    const auto cached = cache.constFind(key);
    if(cached != cache.constEnd())
        return cached.value();

    QPixmap shape(cardSize);
    shape.fill(Qt::transparent);
    {
        QPainter painter(&shape);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.drawRoundedRect(QRectF(shape.rect()), radius, radius);
    }
    QPixmap shadow = oclero::qlementine::getDropShadowPixmap(shape, kCardMargin, color);
    if(cache.size() > 8)
        cache.clear();
    cache.insert(key, shadow);
    return shadow;
}
}

ContextMenu::ContextMenu(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ContextMenu)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Popup);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoMousePropagation, true);
    // Room for the shadow the card paints in paintEvent(); the stylesheet mode
    // keeps the geometry its .ui describes.
    if(UiStyle::useQlementine() && layout())
        layout()->setContentsMargins(1, 1, 1, 1);
    this->hide();

    // -------------------------------------------------------------------------
    // setup actions
    // top zoom buttons (tooltips carry the key, see applyActionHints())
    ui->zoomIn->setAction("zoomIn");
    ui->zoomIn->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-in18.png");
    ui->zoomIn->setTriggerMode(TriggerMode::PressTrigger);
    ui->zoomOut->setAction("zoomOut");
    ui->zoomOut->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-out18.png");
    ui->zoomOut->setTriggerMode(TriggerMode::PressTrigger);
    ui->zoomOriginal->setAction("fitNormal");
    ui->zoomOriginal->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-original18.png");
    ui->zoomOriginal->setTriggerMode(TriggerMode::PressTrigger);
    ui->fitWidth->setAction("fitWidth");
    ui->fitWidth->setIconPath(":/res/icons/common/buttons/contextmenu/fit-width18.png");
    ui->fitWidth->setTriggerMode(TriggerMode::PressTrigger);
    ui->fitWindow->setAction("fitWindow");
    ui->fitWindow->setIconPath(":/res/icons/common/buttons/contextmenu/fit-window18.png");
    ui->fitWindow->setTriggerMode(TriggerMode::PressTrigger);
    ui->fitWindowStretch->setAction("fitWindowStretch");
    ui->fitWindowStretch->setIconPath(":/res/icons/common/buttons/contextmenu/fit-height-stretch18.png");
    ui->fitWindowStretch->setTriggerMode(TriggerMode::PressTrigger);
    // -------------------------------------------------------------------------
    // transform buttons
    ui->rotateLeft->setAction("rotateLeft");
    ui->rotateLeft->setIconPath(":/res/icons/common/menuitem/rotate-left16.png");
    ui->rotateLeft->setTriggerMode(TriggerMode::PressTrigger);
    ui->rotateRight->setAction("rotateRight");
    ui->rotateRight->setIconPath(":/res/icons/common/menuitem/rotate-right16.png");
    ui->rotateRight->setTriggerMode(TriggerMode::PressTrigger);
    ui->flipH->setAction("flipH");
    ui->flipH->setIconPath(":/res/icons/common/menuitem/flip-h16.png");
    ui->flipH->setTriggerMode(TriggerMode::PressTrigger);
    ui->flipV->setAction("flipV");
    ui->flipV->setIconPath(":/res/icons/common/menuitem/flip-v16.png");
    ui->flipV->setTriggerMode(TriggerMode::PressTrigger);
    ui->crop->setAction("crop");
    ui->crop->setIconPath(":/res/icons/common/menuitem/image-crop16.png");
    ui->crop->setTriggerMode(TriggerMode::PressTrigger);
    ui->resize->setAction("resize");
    ui->resize->setIconPath(":/res/icons/common/menuitem/resize16.png");
    ui->resize->setTriggerMode(TriggerMode::PressTrigger);
    // -------------------------------------------------------------------------
    //  entries
    ui->print->setAction("print");
    ui->print->setText(tr("Print"));
    ui->print->setIconPath(":/res/icons/common/menuitem/print16.png");

    ui->copy->setAction("copyFile");
    ui->copy->setText(tr("Quick copy"));
    ui->copy->setIconPath(":/res/icons/common/menuitem/copy16.png");

    ui->move->setAction("moveFile");
    ui->move->setText(tr("Quick move"));
    ui->move->setIconPath(":/res/icons/common/menuitem/move16.png");

    ui->trash->setAction("moveToTrash");
    ui->trash->setText(tr("Move to trash"));
    ui->trash->setIconPath(":/res/icons/common/menuitem/trash16.png");
    // -------------------------------------------------------------------------
    ui->open->setAction("open");
    ui->open->setText(tr("Open"));
    ui->open->setIconPath(":/res/icons/common/menuitem/open16.png");

    ui->folderView->setAction("folderView");
    ui->folderView->setText(tr("Folder View"));
    ui->folderView->setIconPath(":/res/icons/common/menuitem/folderview16.png");

    ui->settings->setAction("openSettings");
    ui->settings->setText(tr("Settings"));
    ui->settings->setIconPath(":/res/icons/common/menuitem/settings16.png");
    // -------------------------------------------------------------------------
    ui->openWith->setText(tr("Open with..."));
    ui->openWith->setIconPath(":/res/icons/common/menuitem/run16.png");
    ui->openWith->setPassthroughClicks(false);
    connect(ui->openWith, &ContextMenuItem::pressed, this, &ContextMenu::switchToScriptsPage);
    // -------------------------------------------------------------------------
    ui->showLocation->setAction("showInDirectory");
    ui->showLocation->setText(tr("Show in folder"));
    ui->showLocation->setIconPath(":/res/icons/common/menuitem/folder16.png");
    // -------------------------------------------------------------------------
    // force resize to fit new menuitem width
    this->adjustSize();

    // Scripts page
    // -------------------------------------------------------------------------
    ui->backButton->setText(tr("Back"));
    ui->backButton->setIconPath(":/res/icons/common/menuitem/back16.png");
    ui->backButton->setPassthroughClicks(false);
    ui->scriptSetupButton->setText(tr("Configure menu"));
    ui->scriptSetupButton->setIconPath(":/res/icons/common/menuitem/settings16.png");
    connect(ui->backButton, &ContextMenuItem::pressed, this, &ContextMenu::switchToMainPage);
    connect(ui->scriptSetupButton, &ContextMenuItem::pressed, this, &ContextMenu::showScriptSettings);
    fillOpenWithMenu();
    applyActionHints();
}

ContextMenu::~ContextMenu() {
    delete ui;
}

// The popup is built once and reused, and the user can edit the shortcuts in the
// preferences between two right-clicks, so the hints are re-read instead of being
// frozen at construction time.
void ContextMenu::applyActionHints() {
    const auto items = findChildren<ContextMenuItem *>();
    for(auto *item : items) {
        const QString action = item->action();
        if(!action.isEmpty())
            item->setAction(action);
    }

    // icon-only buttons: the key goes into the tooltip
    const QVector<QPair<ActionButton *, QString>> buttons = {
        {ui->zoomIn,           QStringLiteral("zoomIn")},
        {ui->zoomOut,          QStringLiteral("zoomOut")},
        {ui->zoomOriginal,     QStringLiteral("fitNormal")},
        {ui->fitWidth,         QStringLiteral("fitWidth")},
        {ui->fitWindow,        QStringLiteral("fitWindow")},
        {ui->fitWindowStretch, QStringLiteral("fitWindowStretch")},
        {ui->rotateLeft,       QStringLiteral("rotateLeft")},
        {ui->rotateRight,      QStringLiteral("rotateRight")},
        {ui->flipH,            QStringLiteral("flipH")},
        {ui->flipV,            QStringLiteral("flipV")},
        {ui->crop,             QStringLiteral("crop")},
        {ui->resize,           QStringLiteral("resize")},
    };
    for(const auto &entry : buttons)
        entry.first->setToolTip(actionToolTip(entry.second));
}

void ContextMenu::fillOpenWithMenu() {
    auto scripts = scriptManager->allScripts();
    QMap<QString, Script>::iterator i;
    for (i = scripts.begin(); i != scripts.end(); ++i) {
        if(!i.value().command.isEmpty()) {
            auto btn = new ContextMenuItem();
            btn->setAction("s:"+i.key());
            btn->setIconPath(":/res/icons/common/menuitem/open16.png");
            btn->setText(i.key());
            ui->scriptsLayout->addWidget(btn);
        }
    }
}

void ContextMenu::switchToMainPage() {
    ui->stackedWidget->setCurrentIndex(0);
}

void ContextMenu::switchToScriptsPage() {
    ui->stackedWidget->setCurrentIndex(1);
}

void ContextMenu::setImageEntriesEnabled(bool mode) {
    ui->rotateLeft->setEnabled(mode);
    ui->rotateRight->setEnabled(mode);
    ui->flipH->setEnabled(mode);
    ui->flipV->setEnabled(mode);
    ui->crop->setEnabled(mode);
    ui->resize->setEnabled(mode);

    ui->copy->setEnabled(mode);
    ui->move->setEnabled(mode);
    ui->trash->setEnabled(mode);
    ui->openWith->setEnabled(mode);
    ui->showLocation->setEnabled(mode);
}

void ContextMenu::showAt(QPoint pos) {
    switchToMainPage();
    applyActionHints();
    // the hints can be wider than the ones the popup was built with
    adjustSize();
    QRect geom = geometry();
    geom.moveTopLeft(pos);
    setGeometry(geom);
    show();
}

void ContextMenu::setGeometry(QRect geom) {
    auto screen = QGuiApplication::screenAt(cursor().pos());
    if(!screen) // fallback
        screen = QGuiApplication::primaryScreen();
    if(screen) {
        // fit inside the current screen
        if(geom.bottom() > screen->geometry().bottom())
            geom.moveBottom(cursor().pos().y());
        if(geom.right() > screen->geometry().right())
            geom.moveRight(screen->geometry().right());
    }
    QWidget::setGeometry(geom);
}

void ContextMenu::mousePressEvent(QMouseEvent *event) {
    QWidget::mousePressEvent(event);
    hide();
}

void ContextMenu::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter p(this);
#ifdef USE_QLEMENTINE
    if(UiStyle::useQlementine()) {
        // Qlementine leaves PE_Widget empty, and the popup window is
        // translucent, so the card has to be painted here or the menu is a
        // transparent rectangle with unreadable text on top of the image.
        UiStyle::ChromePalette pal;
        if(!UiStyle::chromePalette(pal))
            return;
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF card = QRectF(rect());
        if(card.width() <= 0 || card.height() <= 0)
            return;

        // A 30% shadow at this size read as a black ring around the card; MD1
        // menus carry a soft 8dp elevation, not an outline.
        p.setPen(QPen(pal.surfaceLine, 1));
        p.setBrush(pal.surface);
        p.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), pal.radius, pal.radius);
        return;
    }
#endif
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ContextMenu::keyPressEvent(QKeyEvent *event) {
    quint32 nativeScanCode = event->nativeScanCode();
    QString key = actionManager->keyForNativeScancode(nativeScanCode);
    // todo: keyboard navigation
    if(key == "Up") {}
    if(key == "Down") {}
    if(key == "Esc")
        hide();
    if(key == "Enter") {}
    else
        actionManager->processEvent(event);
}
