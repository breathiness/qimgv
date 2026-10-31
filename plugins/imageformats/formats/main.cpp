#include <QImageIOPlugin>
#include <QFileDevice>
#include <QFileInfo>

#include "formatshandler.h"

// One plugin for the formats Qt cannot read here. Claiming them as keys means
// QImageReader picks them up by name ("heic", "jxl", "psd", ...) and by file
// suffix, and Settings::supportedFormats() in the application lists them on its
// own - no code in the viewer has to know about this plugin.
class FormatsPlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QImageIOHandlerFactoryInterface_iid FILE "formats.json")
public:
    Capabilities capabilities(QIODevice *device, const QByteArray &format) const override;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = QByteArray()) const override;
};

QImageIOPlugin::Capabilities FormatsPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if(!format.isEmpty()) {
        const QString requested = QString::fromLatin1(format).toLower();
        if(FormatsHandler::isSupportedSuffix(requested))
            return Capabilities(CanRead);
        // Qt asks with the sniffed format name; ignore anything else
        return {};
    }
    if(!device)
        return {};
    if(auto *fileDevice = qobject_cast<QFileDevice *>(device)) {
        if(FormatsHandler::isSupportedSuffix(QFileInfo(fileDevice->fileName()).suffix()))
            return Capabilities(CanRead);
    }
    return {};
}

QImageIOHandler *FormatsPlugin::create(QIODevice *device, const QByteArray &format) const
{
    const QString requested = QString::fromLatin1(format).toLower();
    bool known = !requested.isEmpty() && FormatsHandler::isSupportedSuffix(requested);
    if(!known) {
        if(auto *fileDevice = qobject_cast<QFileDevice *>(device))
            known = FormatsHandler::isSupportedSuffix(QFileInfo(fileDevice->fileName()).suffix());
    }
    if(!known)
        return nullptr;

    auto *handler = new FormatsHandler();
    handler->setDevice(device);
    if(!format.isEmpty())
        handler->setFormat(format);
    return handler;
}

#include "main.moc"
