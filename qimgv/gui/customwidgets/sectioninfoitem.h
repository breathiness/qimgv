#ifndef SECTIONINFOITEM_H
#define SECTIONINFOITEM_H

#include <QLabel>
#include <QWidget>

// A heading inside the info list: names the group the entries below belong to.
class SectionInfoItem : public QWidget
{
public:
    explicit SectionInfoItem(QWidget *parent = nullptr);
    void setTitle(const QString &title);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel titleLabel;
};

#endif // SECTIONINFOITEM_H
