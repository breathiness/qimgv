#include "apnghandler.h"

#include <QImageIOPlugin>

// Registers the handler for the "apng" key only: qimgv detects the acTL chunk
// itself and then asks for that format explicitly, so a normal PNG keeps going
// through Qt's own plugin.
class ApngPlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QImageIOHandlerFactoryInterface_iid FILE "apng.json")
public:
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const override;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = QByteArray()) const override;
};

QImageIOPlugin::Capabilities ApngPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if(format == "apng")
        return Capabilities(CanRead);
    if(!format.isEmpty())
        return {};
    if(!device || !device->isReadable())
        return {};
    // no format given: only claim files that actually carry the animation chunk
    if(!device->isOpen())
        return {};
    const qint64 position = device->pos();
    const QByteArray head = device->read(4096);
    device->seek(position);
    if(head.left(8) == QByteArray("\x89PNG\r\n\x1a\n", 8) && head.contains("acTL"))
        return Capabilities(CanRead);
    return {};
}

QImageIOHandler *ApngPlugin::create(QIODevice *device, const QByteArray &format) const
{
    if(format == "apng" || format.isEmpty()) {
        ApngHandler *handler = new ApngHandler();
        handler->setDevice(device);
        if(!format.isEmpty())
            handler->setFormat(format);
        return handler;
    }
    return nullptr;
}

#include "main.moc"
