#pragma once

#include <oclero/qlementine/style/QlementineStyle.hpp>

#include <QFont>

class QPainter;
class QStyleOption;
class QStyleOptionComplex;
class QStyleOptionComboBox;

// Material Design 1 buttons, the way MDUI draws them.
//
// Qlementine paints a QPushButton as a filled neutral rectangle whose label
// colour comes from the theme, the same for every role. MDUI's spec is the
// other way round: a flat text button (transparent, 10%/16.5% state layer on
// hover/press, 2dp corners, 36dp tall, 88dp minimum width, uppercase 14sp
// medium label) plus a raised button for the default action (primary fill,
// elevation 2dp at rest, 4dp hover, 8dp pressed). The theme JSON cannot express
// that, so the button control is repainted here and everything else stays
// Qlementine's.
//
// Only active while a Material theme is installed (the theme files name them
// "Material Light" / "Material Dark"), so the stock themes keep their look.
class MaterialStyle : public oclero::qlementine::QlementineStyle {
public:
    void drawControl(ControlElement element, const QStyleOption *option,
                     QPainter *painter, const QWidget *widget) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option,
                           const QSize &size, const QWidget *widget) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    QRect subElementRect(SubElement element, const QStyleOption *option,
                         const QWidget *widget) const override;
    QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                         SubControl subControl, const QWidget *widget) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                            QPainter *painter, const QWidget *widget) const override;

private:
    bool materialThemeActive() const;
    QFont buttonFont(const QStyleOption *option) const;
    void drawSlider(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    void drawSelectorPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter) const;
    void drawFieldPanel(const QStyleOption *option, QPainter *painter) const;
    void drawComboContent(const QStyleOptionComboBox *option, QPainter *painter, const QWidget *widget) const;
};
