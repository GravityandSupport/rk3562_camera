#include "DmaBufRenderer.h"
#include <QDebug>
#include <linux/videodev2.h>
#include <drm/drm_fourcc.h>
#include <chrono>
#include <iostream>

#include "drmdumbbuffer.h"

DmaBufRenderer::DmaBufRenderer(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_eglDisplay(EGL_NO_DISPLAY)
    , m_eglImage(EGL_NO_IMAGE_KHR)
    , m_texture(0)
    , m_vbo(QOpenGLBuffer::VertexBuffer)
    , m_program(nullptr)
    , show_queue(10)
{
    // 强制使用 OpenGL ES 2.0 + 强制开启 DMA-BUF 支持
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    fmt.setVersion(2, 0);
    setFormat(fmt);

//    connect(this, &DmaBufRenderer::onNewDmaBuf_signals, this, &DmaBufRenderer::onNewDmaBuf_slots);
}

DmaBufRenderer::~DmaBufRenderer()
{
    makeCurrent();
    cleanupEGLImage();
    if (m_program) delete m_program;
    if (m_texture) glDeleteTextures(1, &m_texture);
    doneCurrent();
}

bool DmaBufRenderer::checkEGLExtensions()
{
    m_eglDisplay = eglGetCurrentDisplay();
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        qWarning() << "No EGL display";
        return false;
    }

    const char* exts = eglQueryString(m_eglDisplay, EGL_EXTENSIONS);
    if (!exts || !strstr(exts, "EGL_EXT_image_dma_buf_import")) {
        qWarning() << "EGL_EXT_image_dma_buf_import not supported!";
        return false;
    }

    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    if (!eglCreateImageKHR || !eglDestroyImageKHR || !glEGLImageTargetTexture2DOES) {
        qWarning() << "Failed to get EGL/GL function pointers";
        return false;
    }
    return true;
}

uint32_t DmaBufRenderer::getDrmFormat(uint32_t fourcc)
{
    switch (fourcc) {
    case V4L2_PIX_FMT_NV12:  return DRM_FORMAT_NV12;
    case V4L2_PIX_FMT_NV21:  return DRM_FORMAT_NV21;
    case V4L2_PIX_FMT_YUV420: return DRM_FORMAT_YUV420;
    case V4L2_PIX_FMT_YUYV:  return DRM_FORMAT_YUYV;
    case V4L2_PIX_FMT_RGB565: return DRM_FORMAT_RGB565;
    case V4L2_PIX_FMT_XRGB32: return DRM_FORMAT_XRGB8888;
    default:
        qWarning() << "Unsupported format:" << fourcc;
        return 0;
    }
}

void DmaBufRenderer::initializeGL()
{
    initializeOpenGLFunctions();

    if (!checkEGLExtensions()) {
        qCritical() << "EGL extensions not supported! Cannot render DMA-BUF.";
        return;
    }

    // 创建着色器程序
    m_program = new QOpenGLShaderProgram(this);

    const char* vshader = R"(
        attribute vec4 position;
        attribute vec2 texCoord;
        varying vec2 vTexCoord;
        void main() {
            gl_Position = position;
            vTexCoord = texCoord;
        }
    )";

    // 关键：必须用 samplerExternalOES + 开启扩展
    const char* fshader = R"(
        #extension GL_OES_EGL_image_external : require
        precision mediump float;
        varying vec2 vTexCoord;
        uniform samplerExternalOES texture;   // 不是 sampler2D！！！
        void main() {
            vec3 color = texture2D(texture, vTexCoord).rgb;
            gl_FragColor = vec4(color, 1.0);
        }
    )";

    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vshader);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fshader);
    if (!m_program->link()) {
        qCritical() << "Shader link failed:" << m_program->log();
    }

    // 创建纹理（必须是 EXTERNAL）
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_texture);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 顶点数据（两个三角形铺满屏幕）
    static const GLfloat vertices[] = {
        -1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f
    };

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void DmaBufRenderer::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}


void DmaBufRenderer::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_eglImage == EGL_NO_IMAGE_KHR) {
        return;
    }

    m_program->bind();

    // 每帧都要重新绑定 EGLImage 到纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)m_eglImage);

    m_program->setUniformValue("texture", 0);

    m_vbo.bind();
    int posLoc = m_program->attributeLocation("position");
    int texLoc = m_program->attributeLocation("texCoord");

    glEnableVertexAttribArray(posLoc);
    glEnableVertexAttribArray(texLoc);
    glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

    glDrawArrays(GL_TRIANGLES, 0, 6);

//    GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
//    FramePtr frame = std::make_shared<Frame>();
//    frame->fence = fence;
//    frame->drm_fd =

    m_program->release();
}

void DmaBufRenderer::cleanupEGLImage()
{
    if (m_eglImage != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(m_eglDisplay, m_eglImage);
        m_eglImage = EGL_NO_IMAGE_KHR;
    }
}

bool DmaBufRenderer::setupEGLImage(VideoBase::VideoDrmBufPtr frame)
{
    DrmDumbBuffer* drm_buf = static_cast<DrmDumbBuffer*>(frame->slot->getdata());
    int dmabuf_fd=drm_buf->get_dmabuf_fd();
    int width=drm_buf->width();
    int height=drm_buf->height();
    uint32_t v4l2_fourcc=V4L2_PIX_FMT_NV12;

    makeCurrent();
    cleanupEGLImage();

    uint32_t drmFormat = getDrmFormat(v4l2_fourcc);
    if (drmFormat == 0) return false;

    // 注意：stride 很多平台是 width 向上对齐到 128/256/512，这里先用 width
    // 如果画面错位再改成真实的 stride
    EGLint stride = width;

    EGLint attribs[] = {
        EGL_WIDTH,                     width,
        EGL_HEIGHT,                    height,
        EGL_LINUX_DRM_FOURCC_EXT,      (EGLint)drmFormat,

        EGL_DMA_BUF_PLANE0_FD_EXT,     dmabuf_fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT,  stride,

        EGL_DMA_BUF_PLANE1_FD_EXT,     dmabuf_fd,
        EGL_DMA_BUF_PLANE1_OFFSET_EXT, stride * height,
        EGL_DMA_BUF_PLANE1_PITCH_EXT,  stride,

        EGL_NONE
    };

    m_eglImage = eglCreateImageKHR(m_eglDisplay, EGL_NO_CONTEXT,
                                   EGL_LINUX_DMA_BUF_EXT, nullptr, attribs);

    if (m_eglImage == EGL_NO_IMAGE_KHR) {
        qWarning() << "eglCreateImageKHR failed:" << Qt::hex << eglGetError();
        doneCurrent();
        return false;
    }

    // 绑定到纹理（每帧都必须重新绑定）
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)m_eglImage);

    if (glGetError() != GL_NO_ERROR) {
        qWarning() << "glEGLImageTargetTexture2DOES failed";
        cleanupEGLImage();
        doneCurrent();
        return false;
    }

    m_videoWidth = width;
    m_videoHeight = height;
    doneCurrent();
    return true;
}

//void DmaBufRenderer::process_frames(VideoDrmBufPtr frame){
//    ISlot* slot = frame->slot;
//    if(slot==nullptr) {return ;}
//    slot->retain();
//    show_queue.push(frame);
//    emit onNewDmaBuf_signals();
//}

void DmaBufRenderer::onNewDmaBuf_slots(){
    VideoBase::VideoDrmBufPtr frame;
    while(show_queue.try_pop(frame)){
        if(setupEGLImage(frame)){
            update();
        }

        ISlot* slot = frame->slot;
        if(slot==nullptr) {return ;}
        slot->release();
    }
}
