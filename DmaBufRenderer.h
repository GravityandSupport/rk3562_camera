#ifndef DMABUFRENDERER_H
#define DMABUFRENDERER_H

#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <vector>
#include "videobase.h"
#include "ThreadSafeBoundedQueue.h"

#ifndef GLeglImageOES
using GLeglImageOES = void*;
#endif

// EGL 函数指针类型
typedef EGLImageKHR (EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC)(EGLDisplay, EGLImageKHR);
typedef void (EGLAPIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum, GLeglImageOES);

class DmaBufRenderer : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit DmaBufRenderer(QWidget *parent = nullptr);
    ~DmaBufRenderer();
signals:
    void onNewDmaBuf_signals();
public slots:
    void onNewDmaBuf_slots();
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

//    void process_frames(VideoDrmBufPtr frame) override;
private:
    bool checkEGLExtensions();
    uint32_t getDrmFormat(uint32_t v4l2_fourcc);
    bool setupEGLImage(VideoBase::VideoDrmBufPtr);
    void cleanupEGLImage();

    // EGL 相关
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLImageKHR m_eglImage = EGL_NO_IMAGE_KHR;

    // GL 相关
    GLuint m_texture = 0;
    QOpenGLBuffer m_vbo;
    QOpenGLShaderProgram *m_program = nullptr;

    // 函数指针
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = nullptr;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = nullptr;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = nullptr;

    int m_videoWidth = 0;
    int m_videoHeight = 0;

    struct Frame{
        int drm_fd;
        GLsync fence;
    };
    using FramePtr = std::shared_ptr<Frame>;
    std::vector<FramePtr> flight_queue;
public:
    ThreadSafeBoundedQueue<VideoBase::VideoDrmBufPtr> show_queue;
};

#endif // DMABUFRENDERER_H
