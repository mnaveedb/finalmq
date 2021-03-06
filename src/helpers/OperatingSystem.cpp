
#include "helpers/OperatingSystem.h"


#if defined(WIN32) || defined(__MINGW32__)
#else
#include <sys/unistd.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/unistd.h>
#endif


OperatingSystemImpl::OperatingSystemImpl()
{
}
    

// IOperatingSystem
int OperatingSystemImpl::close(int fd)
{
#if defined(WIN32)
#pragma warning(suppress : 4996)
#endif
	return ::close(fd);
}


int OperatingSystemImpl::closeSocket(int fd)
{
#if defined(WIN32) || defined(__MINGW32__)
    return ::closesocket(fd);
#else
    return ::close(fd);
#endif
}


int OperatingSystemImpl::socket(int af, int type, int protocol)
{
	return ::socket(af, type, protocol);
}

int OperatingSystemImpl::bind(int fd, const struct sockaddr* name, socklen_t namelen)
{
	return ::bind(fd, name, namelen);
}

int OperatingSystemImpl::accept(int fd, struct sockaddr* addr, socklen_t* addrlen)
{
	return ::accept(fd, addr, addrlen);
}

int OperatingSystemImpl::listen(int fd, int backlog)
{
	return ::listen(fd, backlog);
}

int OperatingSystemImpl::connect(int fd, const struct sockaddr* name, socklen_t namelen)
{
	return ::connect(fd, name, namelen);
}


int OperatingSystemImpl::setsockopt(int fd, int level, int optname, const char* optval, int optlen)
{
	return ::setsockopt(fd, level, optname, optval, optlen);
}

int OperatingSystemImpl::getsockname(int fd, struct sockaddr* name, socklen_t* namelen)
{
	return ::getsockname(fd, name, namelen);
}


int OperatingSystemImpl::write(int fd, const void* buffer, size_t len)
{
    return ::write(fd, buffer,len);
}

int OperatingSystemImpl::read(int fd, void* buffer, size_t len)
{
    return ::read(fd, buffer,len);
}

int OperatingSystemImpl::send(int fd, const void* buffer, size_t len, int flags)
{
    return ::send(fd, buffer, len, flags);
}

int OperatingSystemImpl::recv(int fd, void* buffer, size_t len, int flags)
{
    return ::recv(fd, buffer, len, flags);
}

int OperatingSystemImpl::getLastError()
{
	int err = -1;
#if defined(MSVCPP) || defined(__MINGW32__)
	err = ::GetLastError();
#else
	err = errno;
#endif
	if (err == SOCKETERROR(EINPROGRESS) || EAGAIN)
	{
		err = SOCKETERROR(EWOULDBLOCK);
	}
	return err;
}


int OperatingSystemImpl::select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout)
{
    int err = ::select(nfds, readfds, writefds, exceptfds, timeout);
    return err;
}



int OperatingSystemImpl::epoll_create1(int flags) 
{
    int err = ::epoll_create1(flags);
	return err;
}

int OperatingSystemImpl::epoll_ctl(int epfd, int op, int fd, struct epoll_event* event)
{
    int err = ::epoll_ctl(epfd, op, fd, event);
	return err;
}

int OperatingSystemImpl::epoll_pwait(int epfd, struct epoll_event* events, int maxevents, int timeout, const sigset_t* sigmask)
{
    int err = ::epoll_pwait(epfd, events, maxevents, timeout, sigmask);
	return err;
}

#if defined(WIN32) || defined(__MINGW32__)

int SocketPair::makeSocketPair(SocketDescriptorPtr& socket1, SocketDescriptorPtr& socket2)
{
    socket1 = nullptr;
    socket2 = nullptr;

    int fdAccept = OperatingSystem::instance().socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fdAccept == -1) {
        perror("socket");
        return -1;
    }
    SocketDescriptorPtr fileDescriptorAccept(std::make_shared<SocketDescriptor>(fdAccept));

    int fdClient = OperatingSystem::instance().socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fdClient == -1) {
        perror("socket");
        return -1;
    }
    SocketDescriptorPtr fileDescriptorClient = std::make_shared<SocketDescriptor>(fdClient);
    setLinger(fdClient);
    setNoDelay(fdClient);

    int b = 1;
    if (OperatingSystem::instance().setsockopt(fdAccept, SOL_SOCKET, SO_REUSEADDR, (const char*)&b, sizeof(int)) == -1) {
        perror("setsockopt");
        return -1;
    }

    struct sockaddr_in addrBind = { 0, 0, 0, 0 };
    addrBind.sin_family = AF_INET;
    addrBind.sin_port = 0;
    addrBind.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (OperatingSystem::instance().bind(fdAccept, (struct sockaddr *)&addrBind, sizeof(addrBind)) == -1) {
        perror("bind");
        return -1;
    }

    if (OperatingSystem::instance().listen(fdAccept, 50) == -1) {
        perror("listen");
        return -1;
    }

    struct sockaddr addr = { 0, 0, 0, 0 };
    socklen_t lenaddr = sizeof(addr);
    if (OperatingSystem::instance().getsockname(fdAccept, &addr, &lenaddr) == -1) {
        perror("getsockname");
        return -1;
    }
    if (OperatingSystem::instance().connect(fdClient, &addr, sizeof(addr)) == -1) {
        fprintf(stderr, "connect wsaerrr %d\n", OperatingSystem::instance().getLastError());
        return -1;
    }

    int fdServer = OperatingSystem::instance().accept(fdAccept, &addr, &lenaddr);
    if (fdServer == -1) {
        perror("accept");
        return -1;
    }
    socket1 = std::make_shared<SocketDescriptor>(fdServer);
    socket2 = fileDescriptorClient;

    setLinger(fdServer);
    setNoDelay(fdServer);

    fileDescriptorAccept = nullptr;

    return 0;
}

#else

int OperatingSystemImpl::makeSocketPair(SocketDescriptorPtr& socket1, SocketDescriptorPtr& socket2)
{
    int sds[2];
    int res = socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, sds);
    if (res == 0)
    {
        socket1 = std::make_shared<SocketDescriptor>(sds[0]);
        socket2 = std::make_shared<SocketDescriptor>(sds[1]);
    }
    return res;
}

#endif



int OperatingSystemImpl::ioctlInt(int fd, unsigned long int request, int* value)
{
    int err = ::ioctl(fd, request, value);
    return err;
}




int OperatingSystemImpl::setNoDelay(int fd, bool noDelay)
{
    int val = noDelay ? 1 : 0;
    int res = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(val));
    return res;
}


int OperatingSystemImpl::setNonBlocking(int fd, bool nonBlock)
{
#if defined(MSVCPP) || defined(__MINGW32__)
    unsigned long val = (nonBlock) ? 1 : 0;	// set non-blocking
    int res = ioctlsocket(fd, FIONBIO, &val);
#else
    int res = fcntl(fd, F_GETFL, 0);
    if (res != -1)
    {
        int val = (nonBlock) ? (res | O_NONBLOCK) : (res & ~O_NONBLOCK);
        res = fcntl(fd, F_SETFL, val);
    }
#endif
    return res;
}


int OperatingSystemImpl::setLinger(int fd, bool on, int timeToLinger)
{
    struct linger l;
    l.l_onoff = true;
    l.l_linger = 0;
    int res = setsockopt(fd, SOL_SOCKET, SO_LINGER, (const char*)&l, sizeof(l));
    return res;
}




//////////////////////////////////////
/// OperatingSystem

std::unique_ptr<IOperatingSystem> OperatingSystem::m_instance;

IOperatingSystem& OperatingSystem::instance()
{
    if (!m_instance)
    {
        m_instance = std::make_unique<OperatingSystemImpl>();
    }
    return *m_instance;
}

void OperatingSystem::setInstance(std::unique_ptr<IOperatingSystem>& instance)
{
    m_instance = std::move(instance);
}
