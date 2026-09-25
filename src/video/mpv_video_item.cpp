#include "mpv_video_item.h"

#include "mpv/mpv_session.h"

#include <QMetaObject>
#include <QDebug>
#include <QOpenGLFramebufferObject>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickOpenGLUtils>

#include <mpv/render_gl.h>

namespace QuarkTV::Video {
namespace {

void *getProcAddress(void *, const char *name)
{
    QOpenGLContext *context = QOpenGLContext::currentContext();
    return context ? reinterpret_cast<void *>(context->getProcAddress(name)) : nullptr;
}

class MpvRenderer final : public QQuickFramebufferObject::Renderer
{
public:
    ~MpvRenderer() override
    {
        if (renderContext_) mpv_render_context_free(renderContext_);
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        auto *videoItem = static_cast<MpvVideoItem *>(item);
        auto *session = qobject_cast<QuarkTV::Mpv::MpvSession *>(videoItem->session());
        mpv_handle *nextHandle = session ? session->handle() : nullptr;
        if (nextHandle == handle_) return;
        if (renderContext_) {
            mpv_render_context_free(renderContext_);
            renderContext_ = nullptr;
        }
        handle_ = nextHandle;
        if (!handle_ || !QOpenGLContext::currentContext()) return;

        mpv_opengl_init_params glInit{getProcAddress, nullptr, nullptr};
        mpv_render_param parameters[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        if (mpv_render_context_create(&renderContext_, handle_, parameters) < 0) {
            renderContext_ = nullptr;
            handle_ = nullptr;
            qWarning("libmpv could not create its OpenGL render context");
            return;
        }
        item_ = videoItem;
        if (QObject *sessionObject = videoItem->session()) {
            QMetaObject::invokeMethod(sessionObject, "setRenderContextReady",
                                      Qt::QueuedConnection);
        }
        mpv_render_context_set_update_callback(renderContext_, [](void *opaque) {
            auto *renderer = static_cast<MpvRenderer *>(opaque);
            const QPointer<MpvVideoItem> item = renderer->item_;
            if (!item) return;
            QMetaObject::invokeMethod(item, [item] {
                if (item) item->update();
            }, Qt::QueuedConnection);
        }, this);
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setInternalTextureFormat(GL_RGBA8);
        return new QOpenGLFramebufferObject(size, format);
    }

    void render() override
    {
        if (!framebufferObject()) return;
        QOpenGLFunctions *gl = QOpenGLContext::currentContext()->functions();
        gl->glClearColor(0.02f, 0.03f, 0.035f, 1.0f);
        gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!renderContext_) {
            QQuickOpenGLUtils::resetOpenGLState();
            return;
        }
        mpv_opengl_fbo target{};
        target.fbo = static_cast<int>(framebufferObject()->handle());
        target.w = framebufferObject()->width();
        target.h = framebufferObject()->height();
        target.internal_format = 0;
        // Qt Quick's FBO texture convention already accounts for the GL
        // origin here. A second Y flip turns the whole video upside down.
        int flipY = 0;
        mpv_render_param parameters[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &target},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context_render(renderContext_, parameters);
        QQuickOpenGLUtils::resetOpenGLState();
    }

private:
    mpv_handle *handle_ = nullptr;
    mpv_render_context *renderContext_ = nullptr;
    QPointer<MpvVideoItem> item_;
};

} // namespace

MpvVideoItem::MpvVideoItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    setTextureFollowsItemSize(true);
}

QQuickFramebufferObject::Renderer *MpvVideoItem::createRenderer() const
{
    return new MpvRenderer;
}

QObject *MpvVideoItem::session() const
{
    return session_;
}

void MpvVideoItem::setSession(QObject *session)
{
    if (session_ == session) return;
    session_ = session;
    emit sessionChanged();
    update();
}

} // namespace QuarkTV::Video
