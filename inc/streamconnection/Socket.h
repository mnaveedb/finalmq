#pragma once

#include "helpers/SocketDescriptor.h"
#include "OpenSsl.h"

#include <memory>
#include <vector>
#include <mutex>

#if !defined(MSVCPP) && !defined(__MINGW32__)
#include <netdb.h>
#endif

class Socket;
typedef std::shared_ptr<Socket> SocketPtr;


class Socket
{
public:
    Socket();
    ~Socket();

    int create(int af, int type, int protocol);
#ifdef USE_OPENSSL
    int createSslServer(int af, int type, int protocol, const CertificateData& certificateData);
    int createSslClient(int af, int type, int protocol, const CertificateData& certificateData);
#endif
    int connect(const sockaddr* addr, int addrlen);
    int accept(sockaddr* addr, socklen_t* addrlen, SocketPtr& socketAccept);
    int bind(const sockaddr* addr, int namelen);
    int listen(int backlog);
    int send(const char* buf, int len, int flags = 0);
    int receive(char* buf, int len, int flags = 0);
    void destroy();
    void attach(int sd);
    int pendingRead() const;
    SocketDescriptorPtr getSocketDescriptor() const;

    bool isValid() const;

    static int getLastError();

private:
    Socket(const Socket& obj) = delete;
    const Socket& operator =(const Socket& obj) = delete;

    int handleError(int err, const char* funcName);

    SocketDescriptorPtr m_sd;
    int                 m_af = 0;
    int                 m_protocol = 0;
    std::string         m_name;

#ifdef USE_OPENSSL
public:
    SSL_CTX* getSslCtx();
    SSL* getSslSocket();
    SslSocket::IoState sslAccepting();
    SslSocket::IoState sslConnecting();
    bool isSsl() const;
    bool isReadWhenWritable() const;
    bool isWriteWhenReadable() const;
    int sslPending();
private:
    void startSslAccept(const std::shared_ptr<SslContext>& sslContext);
    std::shared_ptr<SslContext> m_sslContext;
    std::shared_ptr<SslSocket>  m_sslSocket;
    bool                        m_readWhenWritable = false;
    bool                        m_writeWhenReadable = false;
#endif
    //std::mutex          m_mtx;
};


