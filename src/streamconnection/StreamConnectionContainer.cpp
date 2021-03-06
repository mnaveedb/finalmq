

#include "streamconnection/StreamConnectionContainer.h"
#include "streamconnection/Socket.h"
#include "streamconnection/AddressHelpers.h"
#include "poller/PollerImplEpoll.h"

#if !defined(MSVCPP) && !defined(__MINGW32__)
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/unistd.h>
#include <sys/un.h>
#endif

#include <thread>
#include <assert.h>

//#include <iostream>



// todo: should be removed this definition shall go into BexLibDefines.h
#define SOCKETERROR(err)	err



StreamConnectionContainer::StreamConnectionContainer()
    : m_poller(std::make_shared<PollerImplEpoll>())
    , m_pollerLoopTerminated(CondVar::CONDVAR_MANUAL)
{
}

StreamConnectionContainer::~StreamConnectionContainer()
{
    terminatePollerLoop(10);
}



std::unordered_map<SOCKET, StreamConnectionContainer::BindData>::iterator StreamConnectionContainer::findBindByEndpoint(const std::string& endpoint)
{
    for (auto it = m_sd2binds.begin(); it != m_sd2binds.end(); ++it)
    {
        if (it->second.connectionData.endpoint == endpoint)
        {
            return it;
        }
    }
    return m_sd2binds.end();
}


IStreamConnectionPrivatePtr StreamConnectionContainer::findConnectionBySd(SOCKET sd)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    IStreamConnectionPrivatePtr connection;
    auto it = m_sd2Connection.find(sd);
    if (it != m_sd2Connection.end())
    {
        connection = it->second;
    }
    lock.unlock();
    return connection;
}



// IStreamConnectionContainer

void StreamConnectionContainer::init(int cycleTime, int checkReconnectInterval)
{
    // no mutex lock, because it init is called before the thread will be active.
    m_cycleTime = cycleTime;
    m_checkReconnectInterval = checkReconnectInterval;
    m_poller->init();
}


int StreamConnectionContainer::bind(const std::string& endpoint, bex::hybrid_ptr<IStreamConnectionCallback> callbackDefault)
{
    return bindIntern(endpoint, callbackDefault, false, CertificateData());
}


int StreamConnectionContainer::bindIntern(const std::string& endpoint, bex::hybrid_ptr<IStreamConnectionCallback> callbackDefault, bool ssl, const CertificateData& certificateData)
{
    ConnectionData connectionData = AddressHelpers::endpoint2ConnectionData(endpoint);
    connectionData.ssl = ssl;
    std::shared_ptr<Socket> socket = std::make_shared<Socket>();

    int err = -1;
#ifdef USE_OPENSSL
    if (ssl)
    {
        err = socket->createSslServer(connectionData.af, connectionData.type, connectionData.protocol, certificateData);
    }
    else
#endif
    {
        err = socket->create(connectionData.af, connectionData.type, connectionData.protocol);
    }

    if (err >= 0)
    {
        std::string addr = AddressHelpers::makeSocketAddress(connectionData.hostname, connectionData.port, connectionData.af);
        err = socket->bind((const sockaddr*)addr.c_str(), (int)addr.size());
    }
    if (err == 0)
    {
        // listen for incoming connections
        err = socket->listen(SOMAXCONN);
    }
    if (err == 0)
    {
        SocketDescriptorPtr sd = socket->getSocketDescriptor();
        assert(sd);

        std::unique_lock<std::mutex> locker(m_mutex);

        auto it = findBindByEndpoint(endpoint);
        if (it == m_sd2binds.end())
        {
            assert(m_poller);
            m_sd2binds[sd->getDescriptor()] = {connectionData, socket, callbackDefault};
            m_poller->addSocket(sd);
        }
        locker.unlock();
    }
    return err;
}


void StreamConnectionContainer::unbind(const std::string& endpoint)
{
    std::unique_lock<std::mutex> locker(m_mutex);
    auto it = findBindByEndpoint(endpoint);
    if (it != m_sd2binds.end())
    {
        SocketPtr socket = it->second.socket;
        assert(socket);
        m_poller->removeSocket(socket->getSocketDescriptor());
        m_sd2binds.erase(it);
    }
    locker.unlock();
}

IStreamConnectionPtr StreamConnectionContainer::createConnection(const std::string& endpoint, bex::hybrid_ptr<IStreamConnectionCallback> callback, int reconnectInterval, int totalReconnectDuration)
{
    return createConnectionIntern(endpoint, callback, false, CertificateData(), reconnectInterval, totalReconnectDuration);
}

IStreamConnectionPtr StreamConnectionContainer::createConnectionIntern(const std::string& endpoint, bex::hybrid_ptr<IStreamConnectionCallback> callback, bool ssl, const CertificateData& certificateData, int reconnectInterval, int totalReconnectDuration)
{
    ConnectionData connectionData = AddressHelpers::endpoint2ConnectionData(endpoint);
    connectionData.incomingConnection = false;
    connectionData.reconnectInterval = reconnectInterval;
    connectionData.totalReconnectDuration = totalReconnectDuration;
    connectionData.startTime = std::chrono::system_clock::now();
    connectionData.ssl = ssl;
    connectionData.connectionState = CONNECTIONSTATE_CREATED;
    std::string addr = AddressHelpers::makeSocketAddress(connectionData.hostname, connectionData.port, connectionData.af);
    connectionData.sockaddr = addr;

    SocketPtr socket = std::make_shared<Socket>();
    int ret = -1;
#ifdef USE_OPENSSL
    if (ssl)
    {
        ret = socket->createSslClient(connectionData.af, connectionData.type, connectionData.protocol, certificateData);
    }
    else
#endif
    {
        ret = socket->create(connectionData.af, connectionData.type, connectionData.protocol);
    }

    IStreamConnectionPrivatePtr connection;

    if (ret >= 0)
    {
        connection = addConnection(socket, connectionData, callback);
        assert(connection);
    }

    return connection;
}


std::vector< IStreamConnectionPtr > StreamConnectionContainer::getAllConnections() const
{
    std::vector< IStreamConnectionPtr > connections;

    std::unique_lock<std::mutex> lock(m_mutex);
    connections.reserve(m_connectionId2Connection.size());
    for (auto it = m_connectionId2Connection.begin(); it != m_connectionId2Connection.end(); ++it)
    {
        connections.push_back(it->second);
    }
    lock.unlock();

    return connections;
}

IStreamConnectionPtr StreamConnectionContainer::getConnection(std::int64_t connectionId) const
{
    IStreamConnectionPtr connection;

    std::unique_lock<std::mutex> lock(m_mutex);
    auto it = m_connectionId2Connection.find(connectionId);
    if (it != m_connectionId2Connection.end())
    {
        connection = it->second;
    }
    lock.unlock();

    return connection;
}




//////////////

void StreamConnectionContainer::removeConnection(const SocketDescriptorPtr& sd, std::int64_t connectionId)
{
    assert(sd);

    std::unique_lock<std::mutex> lock(m_mutex);
    m_sd2Connection.erase(sd->getDescriptor());
    m_connectionId2Connection.erase(connectionId);
    lock.unlock();
}


void StreamConnectionContainer::disconnectIntern(const IStreamConnectionPrivatePtr& connectionDisconnect, const SocketDescriptorPtr& sd)
{
    bool removeConn = connectionDisconnect->changeStateForDisconnect();
    if (removeConn)
    {
        removeConnection(sd, connectionDisconnect->getConnectionData().connectionId);
        connectionDisconnect->disconnected(connectionDisconnect);
    }
}

//////////////




void StreamConnectionContainer::threadEntry()
{
    pollerLoop();
}


bool StreamConnectionContainer::terminatePollerLoop(int timeout)
{
    terminatePollerLoop();
    return m_pollerLoopTerminated.wait(timeout);
}



#ifdef USE_OPENSSL

int StreamConnectionContainer::bindSsl(const std::string& endpoint, bex::hybrid_ptr<IStreamConnectionCallback> callback, const CertificateData& certificateData)
{
    return bindIntern(endpoint, callback, true, certificateData);
}
IStreamConnectionPtr StreamConnectionContainer::createConnectionSsl(const std::string& endpoint, bex::hybrid_ptr<IStreamConnectionCallback> callback, const CertificateData& certificateData, int reconnectInterval, int totalReconnectDuration)
{
    return createConnectionIntern(endpoint, callback, true, certificateData, reconnectInterval, totalReconnectDuration);
}

#endif



void StreamConnectionContainer::terminatePollerLoop()
{
    m_terminatePollerLoop = true;
    m_poller->releaseWait();
}



IStreamConnectionPrivatePtr StreamConnectionContainer::addConnection(const SocketPtr& socket, ConnectionData& connectionData, bex::hybrid_ptr<IStreamConnectionCallback> callback)
{
    SocketDescriptorPtr sd = socket->getSocketDescriptor();
    assert(sd);
    connectionData.sd = sd->getDescriptor();
    AddressHelpers::addr2peer((sockaddr*)connectionData.sockaddr.c_str(), connectionData);

    std::unique_lock<std::mutex> lock(m_mutex);
    int connectionId = m_nextConnectionId++;
    connectionData.connectionId = connectionId;
    IStreamConnectionPrivatePtr connection = std::make_shared<StreamConnection>(connectionData, socket, m_poller, callback);
    m_connectionId2Connection[connectionId] = connection;
    m_sd2Connection[connectionData.sd] = connection;
    lock.unlock();

    return connection;
}



void StreamConnectionContainer::handleConnectionEvents(const IStreamConnectionPrivatePtr& connection, const SocketPtr& socket, const DescriptorInfo& info)
{
    SocketDescriptorPtr sd = socket->getSocketDescriptor();
    assert(sd);
    bool disconnected = (info.disconnected || (info.readable && info.bytesToRead == 0));
    if (disconnected)
    {
        disconnectIntern(connection, sd);
    }
    else
    {
        int bytesToRead = info.bytesToRead;
        bool writable = info.writable;
        bool readable = info.readable;
#ifdef USE_OPENSSL
        if (socket->isSsl())
        {
            if (connection->getConnectionData().connectionState == CONNECTIONSTATE_CONNECTING)
            {
                SslSocket::IoState state = socket->sslConnecting();
                if (state == SslSocket::IoState::WANT_WRITE)
                {
                    m_poller->enableWrite(sd);
                }
                else if (state == SslSocket::IoState::WANT_READ)
                {
                    m_poller->disableWrite(sd);
                }
                else if (state == SslSocket::IoState::ERROR)
                {
                    disconnectIntern(connection, sd);
                }
                else if (state == SslSocket::IoState::SUCCESS)
                {
                    bool edgeConnection = connection->checkEdgeConnected();
                    if (edgeConnection)
                    {
                        connection->connected(connection);
                    }
                    m_poller->enableWrite(sd);
                }
                return;
            }
            if (readable)
            {
                char c = 0;
                socket->receive(&c, 0);
                bytesToRead = socket->sslPending();
            }
        }
#endif
        if (writable)
        {
            bool edgeConnection = connection->checkEdgeConnected();
            if (edgeConnection)
            {
                connection->connected(connection);
            }
#ifdef USE_OPENSSL
            if (socket->isReadWhenWritable())
            {
//                std::cout << "isReadWhenWritable" << info.sd << std::endl;
                connection->received(connection, socket, 0);
            }
#endif
            connection->sendPendingMessages();
        }
        if (readable)
        {
#ifdef USE_OPENSSL
            if (socket->isWriteWhenReadable())
            {
//                std::cout << "isReadWhenWritable " << info.sd << std::endl;
                connection->sendPendingMessages();
            }
#endif
            int maxloop = 10;
            while (bytesToRead > 0)
            {
                connection->received(connection, socket, bytesToRead);
                maxloop--;
                if (maxloop > 0)
                {
                    bytesToRead = socket->pendingRead();
#ifdef USE_OPENSSL
                    if (bytesToRead > 0 && socket->isSsl())
                    {
                        char c = 0;
                        socket->receive(&c, 0);
                        bytesToRead = socket->sslPending();
                    }
#endif
                }
                else
                {
                    bytesToRead = 0;
                }
            }

#ifdef USE_OPENSSL
            if (socket->isReadWhenWritable())
            {
                m_poller->enableWrite(sd);
            }
#endif
        }
    }
}

void StreamConnectionContainer::handleBindEvents(const DescriptorInfo& info)
{
    if (info.readable)
    {
        BindData bindData;
        std::unique_lock<std::mutex> lock(m_mutex);
        auto it = m_sd2binds.find(info.sd);
        if (it != m_sd2binds.end())
        {
            bindData = it->second;
        }
        lock.unlock();

        if (bindData.socket)
        {
            std::string addr;
            addr.resize(400);
            socklen_t addrlen = addr.size();
            SocketPtr socketAccept;
            bindData.socket->accept((sockaddr*)addr.c_str(), &addrlen, socketAccept);
            if (socketAccept)
            {
                ConnectionData connectionData = bindData.connectionData;
                connectionData.incomingConnection = true;
                connectionData.startTime = std::chrono::system_clock::now();
                connectionData.sockaddr = addr;
                connectionData.connectionState = CONNECTIONSTATE_CONNECTED;

#ifdef USE_OPENSSL
                if (connectionData.ssl)
                {
                    SocketDescriptorPtr sd = socketAccept->getSocketDescriptor();
                    assert(sd);
                    SslAcceptingData sslAcceptingData = {socketAccept, connectionData, bindData.callback};
                    m_sslAcceptings[sd->getDescriptor()] = sslAcceptingData;
                    sslAccepting(sslAcceptingData);
                }
                else
#endif
                {
                    IStreamConnectionPrivatePtr connection = addConnection(socketAccept, connectionData, bindData.callback);
                    connection->connected(connection);
                }
                SocketDescriptorPtr sd = socketAccept->getSocketDescriptor();
                assert(sd);
                m_poller->addSocket(sd);
            }
        }
    }
}


#ifdef USE_OPENSSL
bool StreamConnectionContainer::sslAccepting(SslAcceptingData& sslAcceptingData)
{
    assert(sslAcceptingData.socket);

    SslSocket::IoState state = sslAcceptingData.socket->sslAccepting();
    SocketDescriptorPtr sd = sslAcceptingData.socket->getSocketDescriptor();
    assert(sd);

    if (state == SslSocket::IoState::WANT_WRITE)
    {
        m_poller->enableWrite(sd);
    }
    else
    {
        m_poller->disableWrite(sd);
    }

    if (state == SslSocket::IoState::SUCCESS)
    {
        IStreamConnectionPrivatePtr connection = addConnection(sslAcceptingData.socket, sslAcceptingData.connectionData, sslAcceptingData.callback);
        connection->connected(connection);
    }
    if (state == SslSocket::IoState::SUCCESS || state == SslSocket::IoState::ERROR)
    {
        m_sslAcceptings.erase(sd->getDescriptor());
    }
    return (state == SslSocket::IoState::SUCCESS);
}
#endif

void StreamConnectionContainer::doReconnect()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    for (auto it = m_sd2Connection.begin(); it != m_sd2Connection.end(); ++it)
    {
        const IStreamConnectionPrivatePtr& connection = it->second;
        connection->doReconnect();
    }
    lock.unlock();
}



bool StreamConnectionContainer::isReconnectTimerExpired()
{
    bool expired = false;
    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

    // reconnect timer
    std::chrono::duration<double> dur = now - m_lastReconnectTime;
    int delta = dur.count() * 1000;
    if (delta < 0 || delta >= m_checkReconnectInterval)
    {
        m_lastReconnectTime = now;
        expired = true;
    }

    return expired;
}




void StreamConnectionContainer::pollerLoop()
{
    m_lastReconnectTime = std::chrono::system_clock::now();
    while (!m_terminatePollerLoop)
    {
        const PollerResult& result = m_poller->wait(m_cycleTime);

        if (result.releaseWait)
        {
            std::vector<IStreamConnectionPrivatePtr> connectionsDisconnect;
            connectionsDisconnect.reserve(m_connectionId2Connection.size());
            std::unique_lock<std::mutex> lock(m_mutex);
            for (auto it = m_connectionId2Connection.begin(); it != m_connectionId2Connection.end(); ++it)
            {
                const IStreamConnectionPrivatePtr& connection = it->second;
                assert(connection);
                if (connection->getDisconnectFlag())
                {
                    connectionsDisconnect.push_back(it->second);
                }
            }
            lock.unlock();

            for (size_t i = 0; i < connectionsDisconnect.size(); ++i)
            {
                const IStreamConnectionPrivatePtr& connectionDisconnect = connectionsDisconnect[i];
                SocketPtr socket = connectionDisconnect->getSocketPrivate();
                {
                    SocketDescriptorPtr sd = socket->getSocketDescriptor();
                    disconnectIntern(connectionDisconnect, sd);
                }
            }
        }


        if (result.error)
        {
            // error of the poller
            terminatePollerLoop();
        }
        else if (result.timeout)
        {

        }
        else
        {
            for (size_t i = 0; i < result.descriptorInfos.size(); ++i)
            {
                const DescriptorInfo& info = result.descriptorInfos[i];
                IStreamConnectionPrivatePtr connection = findConnectionBySd(info.sd);
                if (connection)
                {
                    SocketPtr socket = connection->getSocketPrivate();
                    if (socket)
                    {
                        handleConnectionEvents(connection, socket, info);
                    }
                }
                else
                {
#ifdef USE_OPENSSL
                    auto itSslAccepting = m_sslAcceptings.find(info.sd);
                    if (itSslAccepting != m_sslAcceptings.end())
                    {
                        bool success = sslAccepting(itSslAccepting->second);
                        if (success)
                        {
                            IStreamConnectionPrivatePtr connection = findConnectionBySd(info.sd);
                            if (connection)
                            {
                                SocketPtr socket = connection->getSocketPrivate();
                                if (socket)
                                {
                                    handleConnectionEvents(connection, socket, info);
                                }
                            }
                        }
                    }
                    else
#endif
                    {
                        handleBindEvents(info);
                    }
                }
            }
        }

        if (isReconnectTimerExpired())
        {
            doReconnect();
        }
    }
    m_pollerLoopTerminated = true;
}


