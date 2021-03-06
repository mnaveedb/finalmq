#include "gtest/gtest.h"


#include "streamconnection/StreamConnectionContainer.h"
#include "MockIStreamConnectionCallback.h"
#include "helpers/OperatingSystem.h"
#include "protocols/ProtocolStream.h"
#include "protocolconnection/ProtocolMessage.h"
#include "testHelper.h"

#include <thread>
//#include <chrono>


using ::testing::_;
using ::testing::Return;
using ::testing::DoAll;


//using namespace std::chrono_literals;

static const std::string MESSAGE1_BUFFER = "Hello";



class TestIntegrationStreamConnectionContainer: public testing::Test
{
public:
    void receivedServer(const IStreamConnectionPtr& connection, const SocketPtr& socket, int bytesToRead)
    {
        std::string message;
        message.resize(bytesToRead);
        socket->receive((char*)message.data(), message.size());
        m_messagesServer.push_back(std::move(message));
    }

protected:
    virtual void SetUp()
    {
        m_mockBindCallback = std::make_shared<MockIStreamConnectionCallback>();
        m_mockClientCallback = std::make_shared<MockIStreamConnectionCallback>();
        m_mockServerCallback = std::make_shared<MockIStreamConnectionCallback>();
        m_connectionContainer = std::make_unique<StreamConnectionContainer>();
        m_connectionContainer->init(1, 1);
        IStreamConnectionContainer* connectionContainerRaw = m_connectionContainer.get();
        m_thread = std::make_unique<std::thread>([connectionContainerRaw] () {
            connectionContainerRaw->threadEntry();
        });
    }

    virtual void TearDown()
    {
        EXPECT_EQ(m_connectionContainer->terminatePollerLoop(100), true);
        m_connectionContainer = nullptr;
        m_thread->join();
    }

    std::shared_ptr<IStreamConnectionContainer>             m_connectionContainer;
    std::shared_ptr<MockIStreamConnectionCallback>          m_mockBindCallback;
    std::shared_ptr<MockIStreamConnectionCallback>          m_mockClientCallback;
    std::shared_ptr<MockIStreamConnectionCallback>          m_mockServerCallback;
    std::unique_ptr<std::thread>                            m_thread;
//    std::vector<std::string>                                m_messagesClient;
    std::vector<std::string>                                m_messagesServer;
};





TEST_F(TestIntegrationStreamConnectionContainer, testStartAndStopThreadIntern)
{
}



TEST_F(TestIntegrationStreamConnectionContainer, testBind)
{
    int res = m_connectionContainer->bind("tcp://localhost:3333", m_mockBindCallback);
    EXPECT_EQ(res, 0);
}

TEST_F(TestIntegrationStreamConnectionContainer, testUnbind)
{
    int res = m_connectionContainer->bind("tcp://*:3333", m_mockBindCallback);
    EXPECT_EQ(res, 0);
    m_connectionContainer->unbind("tcp://*:3333");
}


TEST_F(TestIntegrationStreamConnectionContainer, testBindConnect)
{
    int res = m_connectionContainer->bind("tcp://*:3333", m_mockBindCallback);
    EXPECT_EQ(res, 0);

    IStreamConnectionPtr connBind;
    IStreamConnectionPtr connConnect;
    EXPECT_CALL(*m_mockBindCallback, connected(_)).Times(1)
                                            .WillOnce(DoAll(testing::SaveArg<0>(&connBind), Return(m_mockServerCallback)));
    auto& expectConnectedClient = EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1)
                                            .WillOnce(DoAll(testing::SaveArg<0>(&connConnect), Return(nullptr)));
    auto& expectConnectedServer = EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1);

    IStreamConnectionPtr connection = m_connectionContainer->createConnection("tcp://localhost:3333", m_mockClientCallback);
    connection->connect();

    waitTillDone(expectConnectedClient, 5000);
    waitTillDone(expectConnectedServer, 5000);

    EXPECT_EQ(connConnect, connection);
    EXPECT_EQ(connBind->getConnectionData().endpoint, "tcp://*:3333");
}


TEST_F(TestIntegrationStreamConnectionContainer, testBindConnectSend)
{
    int res = m_connectionContainer->bind("tcp://*:3333", m_mockBindCallback);
    EXPECT_EQ(res, 0);

    IStreamConnectionPtr connBind;
    IStreamConnectionPtr connConnect;
    EXPECT_CALL(*m_mockBindCallback, connected(_)).Times(1)
                                            .WillOnce(DoAll(testing::SaveArg<0>(&connBind), Return(m_mockServerCallback)));
    EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1)
                                            .WillOnce(DoAll(testing::SaveArg<0>(&connConnect), Return(nullptr)));
    EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1);
    auto& expectReceive = EXPECT_CALL(*m_mockServerCallback, received(_, _, _)).Times(1)
                                                   .WillRepeatedly(Invoke(this, &TestIntegrationStreamConnectionContainer::receivedServer));

    IStreamConnectionPtr connection = m_connectionContainer->createConnection("tcp://localhost:3333", m_mockClientCallback);
    connection->connect();
    IMessagePtr message = std::make_shared<ProtocolMessage>(0);
    message->addSendPayload(MESSAGE1_BUFFER);
    connection->sendMessage(message);

    waitTillDone(expectReceive, 5000);

    EXPECT_EQ(connConnect, connection);
    EXPECT_EQ(connBind->getConnectionData().endpoint, "tcp://*:3333");

    EXPECT_EQ(m_messagesServer.size(), 1);
    EXPECT_EQ(m_messagesServer[0], MESSAGE1_BUFFER);
}



TEST_F(TestIntegrationStreamConnectionContainer, testConnectBind)
{
    EXPECT_CALL(*m_mockBindCallback, connected(_)).Times(1)
                                            .WillOnce(Return(m_mockServerCallback));
    auto& expectConnected = EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1)
                                            .WillOnce(Return(nullptr));
    EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1);

    IStreamConnectionPtr connection = m_connectionContainer->createConnection("tcp://localhost:3333", m_mockClientCallback, 1);
    connection->connect();

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    int res = m_connectionContainer->bind("tcp://*:3333", m_mockBindCallback);
    EXPECT_EQ(res, 0);

    waitTillDone(expectConnected, 5000);

    EXPECT_EQ(connection->getConnectionData().connectionState, CONNECTIONSTATE_CONNECTED);
    EXPECT_EQ(m_connectionContainer->getConnection(connection->getConnectionData().connectionId), connection);
}



TEST_F(TestIntegrationStreamConnectionContainer, testSendConnectBind)
{
    EXPECT_CALL(*m_mockBindCallback, connected(_)).Times(1)
                                            .WillOnce(Return(m_mockServerCallback));
    EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1)
                                            .WillOnce(Return(nullptr));
    EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1);
    auto& expectReceive = EXPECT_CALL(*m_mockServerCallback, received(_, _, _)).Times(1)
                                                   .WillRepeatedly(Invoke(this, &TestIntegrationStreamConnectionContainer::receivedServer));

    IStreamConnectionPtr connection = m_connectionContainer->createConnection("tcp://localhost:3333", m_mockClientCallback, 1);
    IMessagePtr message = std::make_shared<ProtocolMessage>(0);
    message->addSendPayload(MESSAGE1_BUFFER);
    connection->sendMessage(message);
    connection->connect();

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    int res = m_connectionContainer->bind("tcp://*:3333", m_mockBindCallback);
    EXPECT_EQ(res, 0);

    waitTillDone(expectReceive, 5000);

    EXPECT_EQ(connection->getConnectionData().connectionState, CONNECTIONSTATE_CONNECTED);
    EXPECT_EQ(m_connectionContainer->getConnection(connection->getConnectionData().connectionId), connection);
    EXPECT_EQ(m_messagesServer.size(), 1);
    EXPECT_EQ(m_messagesServer[0], MESSAGE1_BUFFER);
}



TEST_F(TestIntegrationStreamConnectionContainer, testReconnectExpires)
{
    EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(0);
    auto& expectDisconnected = EXPECT_CALL(*m_mockClientCallback, disconnected(_)).Times(1);

    IStreamConnectionPtr connection = m_connectionContainer->createConnection("tcp://localhost:3333", m_mockClientCallback, 1, 1);
    connection->connect();
    IMessagePtr message = std::make_shared<ProtocolMessage>(0);
    message->addSendPayload(MESSAGE1_BUFFER);
    connection->sendMessage(message);

    waitTillDone(expectDisconnected, 5000);

    EXPECT_EQ(connection->getConnectionData().connectionState, CONNECTIONSTATE_DISCONNECTED);
    EXPECT_EQ(m_connectionContainer->getConnection(connection->getConnectionData().connectionId), nullptr);
}




TEST_F(TestIntegrationStreamConnectionContainer, testBindConnectDisconnect)
{
    EXPECT_CALL(*m_mockBindCallback, connected(_)).Times(1)
                                                  .WillRepeatedly(Return(m_mockServerCallback));
    EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1)
                                                  .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1);
    auto& expectReceive = EXPECT_CALL(*m_mockServerCallback, received(_, _, _)).Times(1)
                                                   .WillRepeatedly(Invoke(this, &TestIntegrationStreamConnectionContainer::receivedServer));
    auto& expectDisconnectedClient = EXPECT_CALL(*m_mockClientCallback, disconnected(_)).Times(1);
    auto& expectDisconnectedServer = EXPECT_CALL(*m_mockServerCallback, disconnected(_)).Times(1);

    int res = m_connectionContainer->bind("tcp://*:3333", m_mockBindCallback);
    EXPECT_EQ(res, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    IStreamConnectionPtr connection = m_connectionContainer->createConnection("tcp://localhost:3333", m_mockClientCallback);
    connection->connect();
    IMessagePtr message = std::make_shared<ProtocolMessage>(0);
    message->addSendPayload(MESSAGE1_BUFFER);
    connection->sendMessage(message);

    waitTillDone(expectReceive, 5000);

    connection->disconnect();

    waitTillDone(expectDisconnectedClient, 5000);
    waitTillDone(expectDisconnectedServer, 5000);

    EXPECT_EQ(connection->getConnectionData().connectionState, CONNECTIONSTATE_DISCONNECTED);
    EXPECT_EQ(m_connectionContainer->getConnection(connection->getConnectionData().connectionId), nullptr);
    EXPECT_EQ(m_messagesServer.size(), 1);
    EXPECT_EQ(m_messagesServer[0], MESSAGE1_BUFFER);
}


TEST_F(TestIntegrationStreamConnectionContainer, testGetAllConnections)
{
    int res = m_connectionContainer->bind("tcp://*:3333", m_mockBindCallback);
    EXPECT_EQ(res, 0);

    IStreamConnectionPtr connBind;
    IStreamConnectionPtr connConnect;
    EXPECT_CALL(*m_mockBindCallback, connected(_)).Times(1)
                                            .WillOnce(DoAll(testing::SaveArg<0>(&connBind), Return(m_mockServerCallback)));
    auto& expectConnectedClient = EXPECT_CALL(*m_mockClientCallback, connected(_)).Times(1)
                                            .WillOnce(DoAll(testing::SaveArg<0>(&connConnect), Return(nullptr)));
    auto& expectConnectedServer = EXPECT_CALL(*m_mockServerCallback, connected(_)).Times(1);

    IStreamConnectionPtr connection = m_connectionContainer->createConnection("tcp://localhost:3333", m_mockClientCallback);
    connection->connect();

    waitTillDone(expectConnectedClient, 5000);
    waitTillDone(expectConnectedServer, 5000);

    EXPECT_EQ(connBind->getConnectionData().endpoint, "tcp://*:3333");
    EXPECT_EQ(connConnect, connection);

    std::vector< IStreamConnectionPtr > connections = m_connectionContainer->getAllConnections();
    EXPECT_EQ(connections.size(), 2);
    if (connections[0] == connBind)
    {
        std::swap(connections[0], connections[1]);
    }
    EXPECT_EQ(connections[0], connConnect);
    EXPECT_EQ(connections[1], connBind);
}


