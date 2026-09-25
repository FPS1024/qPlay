#pragma once

#include <QPointer>
#include <QQuickFramebufferObject>

namespace QuarkTV::Mpv { class MpvSession; }

namespace QuarkTV::Video {

class MpvVideoItem : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *session READ session WRITE setSession NOTIFY sessionChanged)

public:
    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    Renderer *createRenderer() const override;

    [[nodiscard]] QObject *session() const;
    void setSession(QObject *session);

signals:
    void sessionChanged();

private:
    QPointer<QObject> session_;
};

} // namespace QuarkTV::Video
