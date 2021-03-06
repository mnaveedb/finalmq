
#include "protocols/ProtocolDelimiter.h"
#include "protocolconnection/ProtocolMessage.h"
#include "streamconnection/Socket.h"


//---------------------------------------
// ProtocolDelimiter
//---------------------------------------


ProtocolDelimiter::ProtocolDelimiter(const std::string& delimiter)
    : m_delimiter(delimiter)
{
}


// IProtocol
void ProtocolDelimiter::setCallback(const std::weak_ptr<IProtocolCallback>& callback)
{
    m_callback = callback;
}

int ProtocolDelimiter::getProtocolId() const
{
    return m_protocolId;
}

bool ProtocolDelimiter::areMessagesResendable() const
{
    return true;
}

IMessagePtr ProtocolDelimiter::createMessage() const
{
    return std::make_shared<ProtocolMessage>(m_protocolId, 0, m_delimiter.size());
}

std::vector<int> ProtocolDelimiter::findEndOfMessage(const char* buffer, int size)
{
    if (m_delimiter.empty())
    {
        return {size};
    }

    int sizeDelimiterPartial = m_delimiterPartial.size();

    std::vector<int> positions;
    int indexStartBuffer = 0;
    // was the delimiter not finished in last buffer?
    if (m_indexDelimiter != -1)
    {
        int a = 0;
        size_t b = m_indexDelimiter;
        for ( ; a < size && b < m_delimiter.size(); ++a, ++b)
        {
            if (buffer[a] != m_delimiter[b])
            {
                break;
            }
        }
        if (b == m_delimiter.size())
        {
            positions.push_back(0 - m_indexDelimiter - sizeDelimiterPartial);
            indexStartBuffer = a;
            m_indexDelimiter = -1;
            m_delimiterPartial.clear();
        }
        else if (a == size)
        {
            m_indexDelimiter = b;
            indexStartBuffer = a;
            m_delimiterPartial += buffer;
        }
        else
        {
            // missmatch
            m_delimiterPartial.clear();
        }
    }

    char c = m_delimiter[0];
    for (int i = indexStartBuffer; i < size; ++i)
    {
        m_delimiterPartial.clear();
        if (buffer[i] == c)
        {
            int a = i + 1;
            size_t b = 1;
            for ( ; a < size && b < m_delimiter.size(); ++a, ++b)
            {
                if (buffer[a] != m_delimiter[b])
                {
                    break;
                }
            }
            if (b == m_delimiter.size())
            {
                positions.push_back(i - sizeDelimiterPartial);
                i = a - 1;  // -1, because ot the ++i in the for loop
            }
            else if (a == size)
            {
                m_indexDelimiter = b;
                m_delimiterPartial = std::string(&buffer[i], a - i);
                break;
            }
        }
    }
    return positions;
}



void ProtocolDelimiter::receive(const SocketPtr& socket, int bytesToRead)
{
    std::string receiveBuffer;
    int sizeDelimiterPartial = m_delimiterPartial.size();
    receiveBuffer.resize(sizeDelimiterPartial + bytesToRead);
    if (sizeDelimiterPartial > 0)
    {
        memcpy(const_cast<char*>(receiveBuffer.data()), &m_delimiterPartial[0], sizeDelimiterPartial);
    }
    int res = socket->receive(const_cast<char*>(receiveBuffer.data() + sizeDelimiterPartial), bytesToRead);
    if (res > 0)
    {
        int bytesReceived = res;
        assert(bytesReceived <= bytesToRead);
        int sizeOfReceiveBuffer = sizeDelimiterPartial + bytesReceived;
        std::vector<int> positions = findEndOfMessage(receiveBuffer.data(), sizeOfReceiveBuffer);
        if (!positions.empty())
        {
            int pos = positions[0];
            IMessagePtr message = std::make_shared<ProtocolMessage>(0);
            message->resizeReceivePayload(m_characterCounter + pos);
            m_characterCounter += bytesReceived;
            assert(m_characterCounter >= 0);

            int offset = 0;

            if (pos < 0)
            {
                for (auto it = m_receiveBuffers.begin(); it != m_receiveBuffers.end(); )
                {
                    const std::string& buffer = *it;
                    ++it;
                    int size = buffer.size() - m_indexStartBuffer;
                    if (it == m_receiveBuffers.end())
                    {
                        size += pos;
                    }
                    memcpy(message->getReceivePayload().first + offset, buffer.c_str() + m_indexStartBuffer, size);
                    offset += size;
                    m_characterCounter -= size;
                    assert(m_characterCounter >= 0);
                    m_indexStartBuffer = 0;
                }
                m_characterCounter -= m_delimiter.size();
            }
            else
            {
                for (auto it = m_receiveBuffers.begin(); it != m_receiveBuffers.end(); ++it)
                {
                    const std::string& buffer = *it;
                    int sizeBuffer = buffer.size();
                    int size = sizeBuffer - m_indexStartBuffer;
                    assert(size >= 0);
                    memcpy(message->getReceivePayload().first + offset, buffer.c_str() + m_indexStartBuffer, size);
                    offset += size;
                    m_characterCounter -= size;
                    assert(m_characterCounter >= 0);
                    m_indexStartBuffer = 0;
                }
                memcpy(message->getReceivePayload().first + offset, receiveBuffer.c_str() + sizeDelimiterPartial, pos);
                m_characterCounter -= pos + m_delimiter.size();
                assert(m_characterCounter >= 0);
            }
            m_receiveBuffers.clear();
            auto callback = m_callback.lock();
            if (callback)
            {
                callback->received(message);
            }

            m_indexStartBuffer = pos + m_delimiter.size();

            for (size_t i = 1; i < positions.size(); ++i)
            {
                pos = positions[i];
                message = std::make_shared<ProtocolMessage>(0);
                int size = pos - m_indexStartBuffer;
                message->resizeReceivePayload(size);
                memcpy(message->getReceivePayload().first, receiveBuffer.c_str() + m_indexStartBuffer + sizeDelimiterPartial, size);
                m_characterCounter -= size + m_delimiter.size();
                assert(m_characterCounter >= 0);
                m_indexStartBuffer = pos + m_delimiter.size();
                if (callback)
                {
                    callback->received(message);
                }
            }
        }
        else
        {
            m_characterCounter += bytesReceived;
        }

        if (m_characterCounter == 0)
        {
            m_indexStartBuffer = 0;
        }
        else
        {
            if (sizeDelimiterPartial == 0)
            {
                m_receiveBuffers.push_back(std::move(receiveBuffer));
            }
            else
            {
                m_receiveBuffers.emplace_back(&receiveBuffer[sizeDelimiterPartial], receiveBuffer.size() - sizeDelimiterPartial);
            }
        }
    }
}


void ProtocolDelimiter::prepareMessageToSend(IMessagePtr message)
{
    if (!message->wasSent())
    {
        const std::list<BufferRef>& buffers = message->getAllSendBuffers();
        if (!buffers.empty() && !m_delimiter.empty())
        {
            const BufferRef& buffer = *buffers.rbegin();
            assert(buffer.second >= static_cast<int>(m_delimiter.size()));
            memcpy(buffer.first + buffer.second - m_delimiter.size(), m_delimiter.data(), m_delimiter.size());
            message->prepareMessageToSend();
        }
    }
}

void ProtocolDelimiter::socketConnected()
{
    auto callback = m_callback.lock();
    if (callback)
    {
        callback->connected();
    }
}

void ProtocolDelimiter::socketDisconnected()
{
    auto callback = m_callback.lock();
    if (callback)
    {
        callback->disconnected();
    }
}



//---------------------------------------
// ProtocolDelimiterFactory
//---------------------------------------


ProtocolDelimiterFactory::ProtocolDelimiterFactory(const std::string& delimiter)
    : m_delimiter(delimiter)
{

}


// IProtocolFactory
IProtocolPtr ProtocolDelimiterFactory::createProtocol()
{
    return std::make_shared<ProtocolDelimiter>(m_delimiter);
}

