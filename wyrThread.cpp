#include "wyrThread.hpp"
#include <pthread.h>
#include <sys/prctl.h>


bool __Thread_t::startCallback(){
    std::clog << "未实现开始回调\n";
    return true;
}

bool __Thread_t::endCallback(){
    std::clog << "未实现结束回调\n";
    return true;
}

bool __Thread_t::_threadLoop(__Thread_t* self){
    std::shared_ptr<__Thread_t> strong(self->mHoldSelf); //引用计数 +1
    std::weak_ptr<__Thread_t> weak(strong); // 创建一个弱引用，引用计数不增加
    self->mHoldSelf.reset(); //释放一个引用，此时的引用计数正常应该是 2

    if (!self->name.empty()){
        // pthread_setname_np(pthread_self(), self->name.c_str()); //修改线程名字
        prctl(PR_SET_NAME, self->name.c_str(), 0, 0, 0);
    }

    std::clog << "线程" << self->name << "启动\n";
    if(self->startCallback()){
        do{
            bool result = self->threadLoop();
            {
                std::unique_lock<std::mutex> lock(self->mMutex);
//                std::lock_guard<std::mutex> lock(self->mMutex);
                if (result == false || self->mExitPending){
                    break;
                }

                while (self->m_suspend && !self->mExitPending)
                {
                    self->m_suspend_cv.wait(lock);
                }

                // 如果在挂起期间被要求退出
                if (self->mExitPending){
                    break;
                }
            }
            strong.reset(); //释放引用，此时引用计数是 1，如果外部的 __Thread_t 的智能指针也释放掉的话，此时引用计数就是0了
            if (!weak.expired()) //可用返回 false
                strong = weak.lock(); // 提取弱引用的指针转为 std::share_ptr，此时引用计数 +1
        }while(strong.get() != NULL); //获取其裸指针是否为空
    }else{
        std::clog << "线程" << self->name << "启动失败\n";
    }

    std::clog << "线程" << self->name << "结束\n";
    self->endCallback();
    std::lock_guard<std::mutex> lock(self->mMutex);
    self->mExitPending = false;
    self->mRunning = false;
    self->m_suspend=false;
    self->cv.notify_one();
    return true;
}

bool __Thread_t::run(const char* __name, threadMode __mode){
    std::lock_guard<std::mutex> lock(mMutex);
    if (mRunning){
        return false;
    }

    mRunning = true;
    m_suspend = false;
    mExitPending = false;
    mHoldSelf = shared_from_this(); // 保存 shared_ptr
    name = __name;

    mThread = std::thread(_threadLoop, this);
    mode = __mode;
    if(mode == threadMode::detach){
        mThread.detach();
    }
    return true;
}

void __Thread_t::join(){
    if(mode == threadMode::join && mThread.joinable()){
        mThread.join();
    }
}

bool __Thread_t::isRunning(){
    bool ret;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        ret = mRunning;
    }
    return ret;
}

void __Thread_t::suspend()
{
    std::lock_guard<std::mutex> lock(mMutex);
    m_suspend = true;
}

void __Thread_t::resume()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        m_suspend = false;
    }
    m_suspend_cv.notify_one();   // 唤醒挂起中的线程
}

bool __Thread_t::isSuspend(){
    return m_suspend;
}

int __Thread_t::requestExitAndWait(joinMode __mode, int time_out){
    std::unique_lock<std::mutex> lock(mMutex);

    if(m_suspend){
        m_suspend = false;
        m_suspend_cv.notify_one();
    }

	mExitPending = true;
    if(__mode == joinMode::BLOCK){
        if(! cv.wait_for(lock, std::chrono::milliseconds(time_out), [this]{ return !mRunning; })){
            std::cout << name << " thread exit wait timeout\n";
			return false; // 超时
        }
    }
    return true;
}


