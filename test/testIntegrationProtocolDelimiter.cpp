
#include "gtest/gtest.h"


#include "protocolconnection/ProtocolSessionContainer.h"
#include "MockIProtocolSessionCallback.h"
#include "protocols/ProtocolDelimiter.h"
#include "testHelper.h"

#include <thread>
//#include <chrono>


using ::testing::_;
using ::testing::Return;
//using ::testing::InSequence;


//using namespace std::chrono_literals;

static const std::string MESSAGE1_BUFFER = "Hello";
static const std::string MESSAGE2_BUFFER(500000, 'A');
static const std::string DELIMITER = "lolololololololololololololololololololololololololololololololololo\n";


MATCHER_P(ReceivedMessage, msg, "")
{
    BufferRef buffer = arg->getReceivePayload();
    std::string str(buffer.first, buffer.second);
    return str == msg;
}


class TestIntegrationProtocolDelimiterSessionContainer: public testing::Test
{
public:

protected:
    virtual void SetUp()
    {
        m_factoryProtocol = std::make_shared<ProtocolDelimiterFactory>(DELIMITER);
        m_mockClientCallback = std::make_shared<MockIProtocolSessionCallback>();
        m_mockServerCallback = std::make_shared<MockIProtocolSessionCallback>();
        m_sessionContainer = std::make_unique<ProtocolSessionContainer>();
        m_sessionContainer->init(1, 1);
        IProtocolSessionContainer* sessionContainerRaw = m_sessionContainer.get();
        m_thread = std::make_unique<std::thread>([sessionContainerRaw] () {
            sessionContainerRaw->threadEntry();
        });
    }

    virtual void TearDown()
    {
        EXPECT_EQ(m_sessionContainer->terminatePollerLoop(100), true);
        m_sessionContainer = nullptr;
        m_thread->join();
    }

    std::shared_ptr<IProtocolSessionContainer>              m_sessionContainer;
    std::shared_ptr<MockIProtocolSessionCallback>           m_mockClientCallback;
    std::shared_ptr<MockIProtocolSessionCallback>           m_mockServerCallback;
    std::shared_ptr<IProtocolFactory>                       m_factoryProtocol;

    std::unique_ptr<std::thread>                            m_thread;
};




TEST_F(TestIntegrationProtocolDelimiterSessionContainer, testStartAndStopThreadIntern)
{
}




TEST_F(TestIntegrationProtocolDelimiterSessionContainer, testBind)
{
    int res = m_sessionContainer->bind("tcp://localhost:3333", m_mockServerCallback, m_factoryProtocol);
    EXPECT_EQ(res, 0);
}


TEST_F(TestIntegrationProtocolDelimiterSessionContainer, testUnbind)
{
    int res = m_sessionContainer->bind("tcp://*:3333", m_mockServerCallback, m_factoryProtocol);
    EXPECT_EQ(res, 0);
    m_sessionContainer->unbind("tcp://*:3333");
}


TEST_F(TestIntegrationProtocolDelimiterSessionContainer, testBindConnect)
{
    int res = m_sessionContainer->bind("tcp://*:3333", m_mockServerCallback, m_factoryProtocol);
    EXPECT_EQ(res, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    IProtocolSessionPtr connConnect;
    EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1)
                                            .WillOnce(testing::SaveArg<0>(&connConnect));
    EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1);
    auto& expectReceive = EXPECT_CALL(*m_mockServerCallback, received(_, ReceivedMessage(MESSAGE1_BUFFER))).Times(1);

    IProtocolSessionPtr connection = m_sessionContainer->connect("tcp://localhost:3333", m_mockClientCallback, std::make_shared<ProtocolDelimiter>(DELIMITER));
    IMessagePtr message = connection->createMessage();
    message->addSendPayload(MESSAGE1_BUFFER);
    connection->sendMessage(message);

    waitTillDone(expectReceive, 5000);

    EXPECT_EQ(connConnect, connection);
}



TEST_F(TestIntegrationProtocolDelimiterSessionContainer, testConnectBind)
{
    auto& expectConnected = EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1);
    EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1);

    IProtocolSessionPtr connection = m_sessionContainer->connect("tcp://localhost:3333", m_mockClientCallback, std::make_shared<ProtocolDelimiter>(DELIMITER), 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    int res = m_sessionContainer->bind("tcp://*:3333", m_mockServerCallback, m_factoryProtocol);
    EXPECT_EQ(res, 0);

    waitTillDone(expectConnected, 5000);

    EXPECT_EQ(connection->getConnectionData().connectionState, CONNECTIONSTATE_CONNECTED);
    EXPECT_EQ(m_sessionContainer->getSession(connection->getSessionId()), connection);
}


TEST_F(TestIntegrationProtocolDelimiterSessionContainer, testSendConnectBind)
{
    EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1);
    EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1);
    auto& expectReceive = EXPECT_CALL(*m_mockServerCallback, received(_, ReceivedMessage(MESSAGE1_BUFFER))).Times(1);

    IProtocolSessionPtr connection = m_sessionContainer->connect("tcp://localhost:3333", m_mockClientCallback, std::make_shared<ProtocolDelimiter>(DELIMITER), 1);
    IMessagePtr message = connection->createMessage();
    message->addSendPayload(MESSAGE1_BUFFER);
    connection->sendMessage(message);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    int res = m_sessionContainer->bind("tcp://*:3333", m_mockServerCallback, m_factoryProtocol);
    EXPECT_EQ(res, 0);

    waitTillDone(expectReceive, 5000);

    EXPECT_EQ(connection->getConnectionData().connectionState, CONNECTIONSTATE_CONNECTED);
    EXPECT_EQ(m_sessionContainer->getSession(connection->getSessionId()), connection);
}



TEST_F(TestIntegrationProtocolDelimiterSessionContainer, testReconnectExpires)
{
    EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(0);
    auto& expectDisconnected = EXPECT_CALL(*m_mockClientCallback, disconnected(_)).Times(1);

    IProtocolSessionPtr connection = m_sessionContainer->connect("tcp://localhost:3333", m_mockClientCallback, std::make_shared<ProtocolDelimiter>(DELIMITER), 1, 1);
    IMessagePtr message = connection->createMessage();
    message->addSendPayload(MESSAGE1_BUFFER);
    connection->sendMessage(message);

    waitTillDone(expectDisconnected, 5000);

    EXPECT_EQ(connection->getConnectionData().connectionState, CONNECTIONSTATE_DISCONNECTED);
    EXPECT_EQ(m_sessionContainer->getSession(connection->getSessionId()), nullptr);
}




TEST_F(TestIntegrationProtocolDelimiterSessionContainer, testBindConnectDisconnect)
{
    EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1);
    auto& expectConnected = EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1);
    EXPECT_CALL(*m_mockServerCallback, received(_, ReceivedMessage(MESSAGE1_BUFFER))).Times(1);
    EXPECT_CALL(*m_mockClientCallback, disconnected(_)).Times(1);
    auto& expectDisconnected = EXPECT_CALL(*m_mockServerCallback, disconnected(_)).Times(1);

    int res = m_sessionContainer->bind("tcp://*:3333", m_mockServerCallback, m_factoryProtocol);
    EXPECT_EQ(res, 0);

    IProtocolSessionPtr connection = m_sessionContainer->connect("tcp://localhost:3333", m_mockClientCallback, std::make_shared<ProtocolDelimiter>(DELIMITER));
    IMessagePtr message = connection->createMessage();
    message->addSendPayload(MESSAGE1_BUFFER);
    connection->sendMessage(message);

    waitTillDone(expectConnected, 5000);

    connection->disconnect();

    waitTillDone(expectDisconnected, 5000);

    EXPECT_EQ(connection->getConnectionData().connectionState, CONNECTIONSTATE_DISCONNECTED);
    EXPECT_EQ(m_sessionContainer->getSession(connection->getSessionId()), nullptr);
}


TEST_F(TestIntegrationProtocolDelimiterSessionContainer, testGetAllConnections)
{
    int res = m_sessionContainer->bind("tcp://*:3333", m_mockServerCallback, m_factoryProtocol);
    EXPECT_EQ(res, 0);

    IProtocolSessionPtr connConnect;
    IProtocolSessionPtr connBind;
    auto& expectConnectedClient = EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1)
                                            .WillOnce(testing::SaveArg<0>(&connConnect));
    auto& expectConnectedServer = EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1)
                                            .WillOnce(testing::SaveArg<0>(&connBind));

    IProtocolSessionPtr connection = m_sessionContainer->connect("tcp://localhost:3333", m_mockClientCallback, std::make_shared<ProtocolDelimiter>(DELIMITER));

    waitTillDone(expectConnectedClient, 5000);
    waitTillDone(expectConnectedServer, 5000);

    EXPECT_EQ(connConnect, connection);

    std::vector< IProtocolSessionPtr > sessions = m_sessionContainer->getAllSessions();
    EXPECT_EQ(sessions.size(), 2);
    if (sessions[0] == connBind)
    {
        std::swap(sessions[0], sessions[1]);
    }
    EXPECT_EQ(sessions[0], connConnect);
    EXPECT_EQ(sessions[1], connBind);
}


TEST_F(TestIntegrationProtocolDelimiterSessionContainer, testSendMultipleMessages)
{
    int res = m_sessionContainer->bind("tcp://*:3333", m_mockServerCallback, m_factoryProtocol);
    EXPECT_EQ(res, 0);

    IProtocolSessionPtr connConnect;
    IProtocolSessionPtr bindConnect;
    auto& expectConnectedClient = EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1)
                                            .WillOnce(testing::SaveArg<0>(&connConnect));
    auto& expectConnectedServer = EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1)
                                            .WillOnce(testing::SaveArg<0>(&bindConnect));
    auto& expectReceiveClient = EXPECT_CALL(*m_mockClientCallback, received(_, ReceivedMessage(MESSAGE1_BUFFER))).Times(1000);
    auto& expectReceiveServer = EXPECT_CALL(*m_mockServerCallback, received(_, ReceivedMessage(MESSAGE1_BUFFER))).Times(1000);

    IProtocolSessionPtr connection = m_sessionContainer->connect("tcp://localhost:3333", m_mockClientCallback, std::make_shared<ProtocolDelimiter>(DELIMITER));
    IMessagePtr message = connection->createMessage();
    message->addSendPayload(MESSAGE1_BUFFER);

    for (int i = 0; i < 1000; ++i)
    {
        connection->sendMessage(message);
    }

    waitTillDone(expectConnectedClient, 5000);
    waitTillDone(expectConnectedServer, 5000);

    for (int i = 0; i < 1000; ++i)
    {
        bindConnect->sendMessage(message);
    }

    EXPECT_EQ(connConnect, connection);

    waitTillDone(expectReceiveClient, 10000);
    waitTillDone(expectReceiveServer, 10000);
}


TEST_F(TestIntegrationProtocolDelimiterSessionContainer, testSendBigMultipleMessages)
{
    int res = m_sessionContainer->bind("tcp://*:3333", m_mockServerCallback, m_factoryProtocol);
    EXPECT_EQ(res, 0);

    IProtocolSessionPtr connConnect;
    IProtocolSessionPtr bindConnect;
    auto& expectConnectedClient = EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1)
                                            .WillOnce(testing::SaveArg<0>(&connConnect));
    auto& expectConnectedServer = EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1)
                                            .WillOnce(testing::SaveArg<0>(&bindConnect));
    auto& expectReceiveClient = EXPECT_CALL(*m_mockClientCallback, received(_, ReceivedMessage(MESSAGE2_BUFFER))).Times(100);
    auto& expectReceiveServer = EXPECT_CALL(*m_mockServerCallback, received(_, ReceivedMessage(MESSAGE2_BUFFER))).Times(100);

    IProtocolSessionPtr connection = m_sessionContainer->connect("tcp://localhost:3333", m_mockClientCallback, std::make_shared<ProtocolDelimiter>(DELIMITER));
    IMessagePtr message = connection->createMessage();
    message->addSendPayload(MESSAGE2_BUFFER);

    for (int i = 0; i < 100; ++i)
    {
        connection->sendMessage(message);
    }

    waitTillDone(expectConnectedClient, 5000);
    waitTillDone(expectConnectedServer, 5000);

    for (int i = 0; i < 100; ++i)
    {
        bindConnect->sendMessage(message);
    }

    EXPECT_EQ(connConnect, connection);

    waitTillDone(expectReceiveClient, 10000);
    waitTillDone(expectReceiveServer, 10000);
}
