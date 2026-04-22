#ifndef IMAGEDISPLAY_H
#define IMAGEDISPLAY_H

#include <QObject>

#include "videobase.h"
#include "safe_thread.h"
#include "ThreadSafeBoundedQueue.h"

#include "DmaBufRenderer.h"

class ImageDisplay : public QObject, public VideoBase
{
    Q_OBJECT
public:
    DmaBufRenderer* render=nullptr;

    void create(DmaBufRenderer* _render=nullptr);

    explicit ImageDisplay(QObject *parent = nullptr);
    virtual ~ImageDisplay();
signals:
    void onNewDmaBuf_signals();
protected:
    virtual void process_frames(VideoDrmBufPtr frame);
private:
    ThreadSafeBoundedQueue<VideoDrmBufPtr> process_queue;
    SafeThread thread_;
};

#endif // IMAGEDISPLAY_H
