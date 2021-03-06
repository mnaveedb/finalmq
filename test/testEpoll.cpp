#include "gtest/gtest.h"
#include "gmock/gmock.h"


#include "poller/PollerImplEpoll.h"
#include "helpers/OperatingSystem.h"


#include "MockIOperatingSystem.h"


using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::DoAll;


static const std::string BUFFER = "Hello";

static const int EPOLL_FD = 3;
static const int CONTROLSOCKET_READ = 4;
static const int CONTROLSOCKET_WRITE = 5;
static const int TESTSOCKET = 7;

static const int NUMBER_OF_BYTES_TO_READ = 20;
static const int TIMEOUT = 10;


MATCHER_P(Event, event, "")
{
    return (arg->events == event->events &&
            arg->data.fd == event->data.fd);
}


class TestEpoll: public testing::Test
{
protected:
    virtual void SetUp()
    {
        m_mockMockOperatingSystem = new MockIOperatingSystem;
        std::unique_ptr<IOperatingSystem> iOperatingSystem(m_mockMockOperatingSystem);
        OperatingSystem::setInstance(iOperatingSystem);
        m_select = std::make_unique<PollerImplEpoll>();

        EXPECT_CALL(*m_mockMockOperatingSystem, epoll_create1(EPOLL_CLOEXEC)).Times(1)
                    .WillRepeatedly(Return(EPOLL_FD));

        SocketDescriptorPtr sd1 = std::make_shared<SocketDescriptor>(CONTROLSOCKET_READ);
        SocketDescriptorPtr sd2 = std::make_shared<SocketDescriptor>(CONTROLSOCKET_WRITE);
        EXPECT_CALL(*m_mockMockOperatingSystem, makeSocketPair(_, _)).Times(1)
                    .WillRepeatedly(DoAll(testing::SetArgReferee<0>(sd1), testing::SetArgReferee<1>(sd2), Return(0)));

        epoll_event evCtl;
        evCtl.events = EPOLLIN;
        evCtl.data.fd = CONTROLSOCKET_READ;
        EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_ADD, CONTROLSOCKET_READ, Event(&evCtl))).Times(1);

        m_select->init();
        testing::Mock::VerifyAndClearExpectations(m_mockMockOperatingSystem);
    }

    virtual void TearDown()
    {
        EXPECT_CALL(*m_mockMockOperatingSystem, close(EPOLL_FD)).Times(1).WillRepeatedly(Return(0));
        EXPECT_CALL(*m_mockMockOperatingSystem, closeSocket(_)).WillRepeatedly(Return(0));
        m_select = nullptr;
        std::unique_ptr<IOperatingSystem> resetOperatingSystem;
        OperatingSystem::setInstance(resetOperatingSystem);
    }

    MockIOperatingSystem* m_mockMockOperatingSystem = nullptr;
    std::unique_ptr<IPoller> m_select;
};


TEST_F(TestEpoll, timeout)
{
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_pwait(EPOLL_FD, _, _, TIMEOUT, nullptr)).Times(1)
                .WillRepeatedly(Return(0));

    const PollerResult& result = m_select->wait(10);
    EXPECT_EQ(result.error, false);
    EXPECT_EQ(result.timeout, true);
    EXPECT_EQ(result.descriptorInfos.size(), 0);
}



TEST_F(TestEpoll, testAddSocketReadableWait)
{
    SocketDescriptorPtr socket = std::make_shared<SocketDescriptor>(TESTSOCKET);

    epoll_event evCtl;
    evCtl.events = EPOLLIN;
    evCtl.data.fd = socket->getDescriptor();

    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(_, EPOLL_CTL_ADD, socket->getDescriptor(), Event(&evCtl))).Times(1);

    m_select->addSocket(socket);

    struct epoll_event events;
    events.data.fd = socket->getDescriptor();
    events.events = EPOLLIN;

    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_pwait(EPOLL_FD, _, _, TIMEOUT, nullptr)).Times(1)
                .WillRepeatedly(testing::DoAll(testing::SetArgPointee<1>(events), Return(1)));
    EXPECT_CALL(*m_mockMockOperatingSystem, ioctlInt(socket->getDescriptor(), FIONREAD, _)).Times(1)
                                                        .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(NUMBER_OF_BYTES_TO_READ), Return(0)));

    const PollerResult& result = m_select->wait(TIMEOUT);
    EXPECT_EQ(result.error, false);
    EXPECT_EQ(result.timeout, false);
    EXPECT_EQ(result.descriptorInfos.size(), 1);
    if (result.descriptorInfos.size() == 1)
    {
        EXPECT_EQ(result.descriptorInfos[0].sd, socket->getDescriptor());
        EXPECT_EQ(result.descriptorInfos[0].disconnected, false);
        EXPECT_EQ(result.descriptorInfos[0].readable, true);
        EXPECT_EQ(result.descriptorInfos[0].writable, false);
        EXPECT_EQ(result.descriptorInfos[0].bytesToRead, NUMBER_OF_BYTES_TO_READ);
    }
}


TEST_F(TestEpoll, testAddSocketReadableEINTR)
{
    SocketDescriptorPtr socket = std::make_shared<SocketDescriptor>(TESTSOCKET);

    epoll_event evCtl;
    evCtl.events = EPOLLIN;
    evCtl.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_ADD, socket->getDescriptor(), Event(&evCtl))).Times(1);

    m_select->addSocket(socket);

    struct epoll_event events;
    events.data.fd = socket->getDescriptor();
    events.events = EPOLLIN;

    {
        InSequence seq;

        EXPECT_CALL(*m_mockMockOperatingSystem, epoll_pwait(_, _, _, TIMEOUT, nullptr)).Times(1)
                    .WillRepeatedly(Return(-1));
        EXPECT_CALL(*m_mockMockOperatingSystem, epoll_pwait(_, _, _, TIMEOUT, nullptr)).Times(1)
                    .WillRepeatedly(testing::DoAll(testing::SetArgPointee<1>(events), Return(1)));
    }
    EXPECT_CALL(*m_mockMockOperatingSystem, getLastError()).Times(1)
                        .WillOnce(Return(SOCKETERROR(EINTR)));
    EXPECT_CALL(*m_mockMockOperatingSystem, ioctlInt(socket->getDescriptor(), FIONREAD, _)).Times(1)
                                                        .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(NUMBER_OF_BYTES_TO_READ), Return(0)));

    const PollerResult& result = m_select->wait(TIMEOUT);
    EXPECT_EQ(result.error, false);
    EXPECT_EQ(result.timeout, false);
    EXPECT_EQ(result.descriptorInfos.size(), 1);
    if (result.descriptorInfos.size() == 1)
    {
        EXPECT_EQ(result.descriptorInfos[0].sd, socket->getDescriptor());
        EXPECT_EQ(result.descriptorInfos[0].disconnected, false);
        EXPECT_EQ(result.descriptorInfos[0].readable, true);
        EXPECT_EQ(result.descriptorInfos[0].writable, false);
        EXPECT_EQ(result.descriptorInfos[0].bytesToRead, NUMBER_OF_BYTES_TO_READ);
    }
}




TEST_F(TestEpoll, testAddSocketReadableError)
{
    SocketDescriptorPtr socket = std::make_shared<SocketDescriptor>(TESTSOCKET);

    epoll_event evCtl;
    evCtl.events = EPOLLIN;
    evCtl.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_ADD, socket->getDescriptor(), Event(&evCtl))).Times(1);

    m_select->addSocket(socket);

    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_pwait(EPOLL_FD, _, _, TIMEOUT, nullptr)).Times(1)
                .WillRepeatedly(Return(-1));
    EXPECT_CALL(*m_mockMockOperatingSystem, getLastError()).Times(1)
                        .WillOnce(Return(SOCKETERROR(EACCES)));

    const PollerResult& result = m_select->wait(TIMEOUT);
    EXPECT_EQ(result.error, true);
    EXPECT_EQ(result.timeout, false);
    EXPECT_EQ(result.descriptorInfos.size(), 0);
}


TEST_F(TestEpoll, testAddSocketReadableWaitSocketDescriptorsChanged)
{
    SocketDescriptorPtr socket = std::make_shared<SocketDescriptor>(TESTSOCKET);

    epoll_event evCtl;
    evCtl.events = EPOLLIN;
    evCtl.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_ADD, socket->getDescriptor(), Event(&evCtl))).Times(1);

    m_select->addSocket(socket);

    struct epoll_event events;
    events.data.fd = socket->getDescriptor();
    events.events = EPOLLIN;

    epoll_event evCtlRemove;
    evCtlRemove.events = 0;
    evCtlRemove.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_DEL, socket->getDescriptor(), Event(&evCtlRemove))).Times(1);

    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_pwait(EPOLL_FD, _, _, TIMEOUT, nullptr)).Times(1)
            .WillRepeatedly(
                testing::DoAll(
                    testing::Invoke([this, &socket](int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t* sigmask){
                        m_select->removeSocket(socket);
                    }),
                    testing::SetArgPointee<1>(events),
                    Return(1)
                )
            );
    EXPECT_CALL(*m_mockMockOperatingSystem, ioctlInt(socket->getDescriptor(), FIONREAD, _)).Times(1)
                                                        .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(NUMBER_OF_BYTES_TO_READ), Return(0)));

    EXPECT_CALL(*m_mockMockOperatingSystem, closeSocket(socket->getDescriptor())).WillRepeatedly(Return(0));

    const PollerResult& result = m_select->wait(TIMEOUT);
    EXPECT_EQ(result.error, false);
    EXPECT_EQ(result.timeout, false);
    EXPECT_EQ(result.descriptorInfos.size(), 1);
    if (result.descriptorInfos.size() == 1)
    {
        EXPECT_EQ(result.descriptorInfos[0].sd, socket->getDescriptor());
        EXPECT_EQ(result.descriptorInfos[0].disconnected, false);
        EXPECT_EQ(result.descriptorInfos[0].readable, true);
        EXPECT_EQ(result.descriptorInfos[0].writable, false);
        EXPECT_EQ(result.descriptorInfos[0].bytesToRead, NUMBER_OF_BYTES_TO_READ);
    }
}

TEST_F(TestEpoll, testAddSocketDisconnectRead)
{
    SocketDescriptorPtr socket = std::make_shared<SocketDescriptor>(TESTSOCKET);

    epoll_event evCtl;
    evCtl.events = EPOLLIN;
    evCtl.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_ADD, socket->getDescriptor(), Event(&evCtl))).Times(1);

    m_select->addSocket(socket);

    struct epoll_event events;
    events.data.fd = socket->getDescriptor();
    events.events = EPOLLIN;

    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_pwait(EPOLL_FD, _, _, TIMEOUT, nullptr)).Times(1)
                                                .WillRepeatedly(testing::DoAll(testing::SetArgPointee<1>(events), Return(1)));
    EXPECT_CALL(*m_mockMockOperatingSystem, ioctlInt(socket->getDescriptor(), FIONREAD, _)).Times(1)
                                                .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(0), Return(0)));

    const PollerResult& result = m_select->wait(TIMEOUT);
    EXPECT_EQ(result.error, false);
    EXPECT_EQ(result.timeout, false);
    EXPECT_EQ(result.descriptorInfos.size(), 1);
    if (result.descriptorInfos.size() == 1)
    {
        EXPECT_EQ(result.descriptorInfos[0].sd, socket->getDescriptor());
        EXPECT_EQ(result.descriptorInfos[0].disconnected, false);
        EXPECT_EQ(result.descriptorInfos[0].readable, true);
        EXPECT_EQ(result.descriptorInfos[0].writable, false);
        EXPECT_EQ(result.descriptorInfos[0].bytesToRead, 0);
    }
}



TEST_F(TestEpoll, testAddSocketDisconnectEpollError)
{
    SocketDescriptorPtr socket = std::make_shared<SocketDescriptor>(TESTSOCKET);

    epoll_event evCtl;
    evCtl.events = EPOLLIN;
    evCtl.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_ADD, socket->getDescriptor(), Event(&evCtl))).Times(1);

    m_select->addSocket(socket);

    struct epoll_event events;
    events.data.fd = socket->getDescriptor();
    events.events = EPOLLERR;

    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_pwait(EPOLL_FD, _, _, TIMEOUT, nullptr)).Times(1)
                                                .WillRepeatedly(testing::DoAll(testing::SetArgPointee<1>(events), Return(1)));

    const PollerResult& result = m_select->wait(TIMEOUT);
    EXPECT_EQ(result.error, false);
    EXPECT_EQ(result.timeout, false);
    EXPECT_EQ(result.descriptorInfos.size(), 1);
    if (result.descriptorInfos.size() == 1)
    {
        EXPECT_EQ(result.descriptorInfos[0].sd, socket->getDescriptor());
        EXPECT_EQ(result.descriptorInfos[0].disconnected, true);
        EXPECT_EQ(result.descriptorInfos[0].readable, false);
        EXPECT_EQ(result.descriptorInfos[0].writable, false);
        EXPECT_EQ(result.descriptorInfos[0].bytesToRead, 0);
    }
}

TEST_F(TestEpoll, testAddSocketIoCtlError)
{
    SocketDescriptorPtr socket = std::make_shared<SocketDescriptor>(TESTSOCKET);

    epoll_event evCtl;
    evCtl.events = EPOLLIN;
    evCtl.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_ADD, socket->getDescriptor(), Event(&evCtl))).Times(1);

    m_select->addSocket(socket);

    struct epoll_event events;
    events.data.fd = socket->getDescriptor();
    events.events = EPOLLIN;

    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_pwait(EPOLL_FD, _, _, TIMEOUT, nullptr)).Times(1)
                                                .WillRepeatedly(testing::DoAll(testing::SetArgPointee<1>(events), Return(1)));
    EXPECT_CALL(*m_mockMockOperatingSystem, ioctlInt(socket->getDescriptor(), FIONREAD, _)).Times(1)
                                                .WillRepeatedly(testing::DoAll(testing::SetArgPointee<2>(0), Return(-1)));

    const PollerResult& result = m_select->wait(TIMEOUT);
    EXPECT_EQ(result.error, false);
    EXPECT_EQ(result.timeout, false);
    EXPECT_EQ(result.descriptorInfos.size(), 1);
    if (result.descriptorInfos.size() == 1)
    {
        EXPECT_EQ(result.descriptorInfos[0].sd, socket->getDescriptor());
        EXPECT_EQ(result.descriptorInfos[0].disconnected, false);
        EXPECT_EQ(result.descriptorInfos[0].readable, true);
        EXPECT_EQ(result.descriptorInfos[0].writable, false);
        EXPECT_EQ(result.descriptorInfos[0].bytesToRead, 0);
    }
}


TEST_F(TestEpoll, testAddSocketWritableWait)
{
    SocketDescriptorPtr socket = std::make_shared<SocketDescriptor>(TESTSOCKET);

    epoll_event evCtl;
    evCtl.events = EPOLLIN;
    evCtl.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_ADD, socket->getDescriptor(), Event(&evCtl))).Times(1);

    m_select->addSocket(socket);

    epoll_event evCtlWrite;
    evCtlWrite.events = EPOLLIN | EPOLLOUT;
    evCtlWrite.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_MOD, socket->getDescriptor(), Event(&evCtlWrite))).Times(1);

    m_select->enableWrite(socket);

    struct epoll_event events;
    events.data.fd = socket->getDescriptor();
    events.events = EPOLLOUT;

    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_pwait(EPOLL_FD, _, _, TIMEOUT, nullptr)).Times(1)
                                                .WillRepeatedly(testing::DoAll(testing::SetArgPointee<1>(events), Return(1)));

    const PollerResult& result = m_select->wait(TIMEOUT);
    EXPECT_EQ(result.error, false);
    EXPECT_EQ(result.timeout, false);
    EXPECT_EQ(result.descriptorInfos.size(), 1);
    if (result.descriptorInfos.size() == 1)
    {
        EXPECT_EQ(result.descriptorInfos[0].sd, socket->getDescriptor());
        EXPECT_EQ(result.descriptorInfos[0].disconnected, false);
        EXPECT_EQ(result.descriptorInfos[0].readable, false);
        EXPECT_EQ(result.descriptorInfos[0].writable, true);
        EXPECT_EQ(result.descriptorInfos[0].bytesToRead, 0);
    }
}



TEST_F(TestEpoll, testAddSocketDisableWritableWait)
{
    SocketDescriptorPtr socket = std::make_shared<SocketDescriptor>(TESTSOCKET);

    epoll_event evCtl;
    evCtl.events = EPOLLIN;
    evCtl.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_ADD, socket->getDescriptor(), Event(&evCtl))).Times(1);

    m_select->addSocket(socket);

    epoll_event evCtlWrite;
    evCtlWrite.events = EPOLLIN | EPOLLOUT;
    evCtlWrite.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_MOD, socket->getDescriptor(), Event(&evCtlWrite))).Times(1);

    m_select->enableWrite(socket);

    epoll_event evCtlDisableWrite;
    evCtlDisableWrite.events = EPOLLIN;
    evCtlDisableWrite.data.fd = socket->getDescriptor();
    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_ctl(EPOLL_FD, EPOLL_CTL_MOD, socket->getDescriptor(), Event(&evCtlDisableWrite))).Times(1);

    m_select->disableWrite(socket);

    EXPECT_CALL(*m_mockMockOperatingSystem, epoll_pwait(EPOLL_FD, _, _, TIMEOUT, nullptr)).Times(1)
                                                .WillRepeatedly(Return(0));

    const PollerResult& result = m_select->wait(TIMEOUT);
    EXPECT_EQ(result.error, false);
    EXPECT_EQ(result.timeout, true);
    EXPECT_EQ(result.descriptorInfos.size(), 0);
}


