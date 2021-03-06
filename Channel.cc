#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;//表示对应的文件描述符有紧急的数据可读
const int Channel::kWriteEvent = EPOLLOUT;

//EventLoop : ChannelList Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false)
{}

Channel::~Channel()
{
}

// 一个TcpConnection新连接创建的时候，一个弱智能指针指向上层的TcpConnection对象
void Channel::tie(const std::shared_ptr<void>&obj)
{
    tie_ = obj;
    tied_ = true;
}

//当改变Channel所表示的events事件后，update负责在poller里面更改fd相应的事件
//相当于epoll_ctl
void Channel::update()
{
    //通过Channel所属的eventloop，调用poller里的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在Channel所属的Eventloop中， 把当前的Channel删除
void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

// 根据poller通知的channel发生的具体事件， 由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{

    LOG_INFO("channel handleEvent revents:%d", revents_);

    // error
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    //read
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }
    //write
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}