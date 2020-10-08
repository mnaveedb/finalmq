
#include "serializejson/ParserJson.h"
#include "metadata/MetaData.h"
//#include "helpers/BexDefines.h"

#include "conversions/itoa.h"
#include "conversions/dtoa.h"

#include <assert.h>
//#include <memory.h>
#include <iostream>




ParserJson::ParserJson(IParserVisitor& visitor, const char* ptr, int size)
    : m_ptr(ptr)
    , m_size(size)
    , m_visitor(visitor)
    , m_parser(*this)
{
}





bool ParserJson::parseStruct(const std::string& typeName)
{
    assert(m_ptr);
    assert(m_size >= 0);

    const MetaStruct* stru = MetaDataGlobal::instance().getStruct(typeName);
    if (!stru)
    {
        m_visitor.notifyError(m_ptr, "typename not found");
        m_visitor.finished();
        return false;
    }

    MetaField field = {MetaType::TYPE_STRUCT, typeName};
    field.metaStruct = stru;
    m_fieldCurrent = &field;

    const char* str = m_parser.parse(m_ptr, m_size);
    m_visitor.finished();

    return (str != nullptr);
}




void ParserJson::syntaxError(const char* str, const char* message)
{

}

void ParserJson::enterNull()
{

}

template<class T>
void ParserJson::enterNumber(T value)
{
    if (!m_fieldCurrent)
    {
        // unknown key
        return;
    }
    switch (m_fieldCurrent->type)
    {
    case MetaType::TYPE_BOOL:
        m_visitor.enterBool(*m_fieldCurrent, value);
        break;
    case MetaType::TYPE_INT32:
        m_visitor.enterInt32(*m_fieldCurrent, value);
        break;
    case MetaType::TYPE_UINT32:
        m_visitor.enterUInt32(*m_fieldCurrent, value);
        break;
    case MetaType::TYPE_INT64:
        m_visitor.enterInt64(*m_fieldCurrent, value);
        break;
    case MetaType::TYPE_UINT64:
        m_visitor.enterUInt64(*m_fieldCurrent, value);
        break;
    case MetaType::TYPE_FLOAT:
        m_visitor.enterFloat(*m_fieldCurrent, value);
        break;
    case MetaType::TYPE_DOUBLE:
        m_visitor.enterDouble(*m_fieldCurrent, value);
        break;
    case MetaType::TYPE_STRING:
        m_visitor.enterString(*m_fieldCurrent, std::to_string(value));
        break;
    case MetaType::TYPE_ENUM:
        m_visitor.enterEnum(*m_fieldCurrent, value);
        break;
    case MetaType::TYPE_ARRAY_BOOL:
        m_arrayBool.push_back(value);
        break;
    case MetaType::TYPE_ARRAY_INT32:
        m_arrayInt32.push_back(value);
        break;
    case MetaType::TYPE_ARRAY_UINT32:
        m_arrayUInt32.push_back(value);
        break;
    case MetaType::TYPE_ARRAY_INT64:
        m_arrayInt64.push_back(value);
        break;
    case MetaType::TYPE_ARRAY_UINT64:
        m_arrayUInt64.push_back(value);
        break;
    case MetaType::TYPE_ARRAY_FLOAT:
        m_arrayFloat.push_back(value);
        break;
    case MetaType::TYPE_ARRAY_DOUBLE:
        m_arrayDouble.push_back(value);
        break;
    case MetaType::TYPE_ARRAY_STRING:
        m_arrayString.push_back(std::to_string(value));
        break;
    case MetaType::TYPE_ARRAY_BYTES:
        m_arrayString.push_back(std::to_string(value));
        break;
    case MetaType::TYPE_ARRAY_ENUM:
        if (m_arrayString.empty() || !m_arrayInt32.empty())
        {
            m_arrayInt32.push_back(value);
        }
        else
        {
            const std::string& v = MetaDataGlobal::instance().getEnumNameByValue(*m_fieldCurrent, value);
            m_arrayString.push_back(v);
        }
        break;
    default:
        std::cout << "number not expected";
        break;
    }
}



void ParserJson::enterBool(bool value)
{
    enterNumber(value);
}

void ParserJson::enterInt32(std::int32_t value)
{
    enterNumber(value);
}

void ParserJson::enterUInt32(std::uint32_t value)
{
    enterNumber(value);
}

void ParserJson::enterInt64(std::int64_t value)
{
    enterNumber(value);
}

void ParserJson::enterUInt64(std::uint64_t value)
{
    enterNumber(value);
}

void ParserJson::enterDouble(double value)
{
    enterNumber(value);
}

void ParserJson::enterString(const char* value, int size)
{
    if (!m_fieldCurrent)
    {
        // unknown key
        return;
    }
    switch (m_fieldCurrent->type)
    {
    case MetaType::TYPE_BOOL:
        {
            bool v = (size == 4 && (memcmp(value, "true", 4) == 0));
            m_visitor.enterBool(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_INT32:
        {
            std::int32_t v = strtol(value, nullptr, 10);
            m_visitor.enterInt32(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_UINT32:
        {
            std::uint32_t v = strtoul(value, nullptr, 10);
            m_visitor.enterUInt32(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_INT64:
        {
            std::int64_t v = strtoll(value, nullptr, 10);
            m_visitor.enterInt64(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_UINT64:
        {
            std::uint64_t v = strtoull(value, nullptr, 10);
            m_visitor.enterUInt64(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_FLOAT:
        {
            float v = strtof32(value, nullptr);
            m_visitor.enterFloat(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_DOUBLE:
        {
            double v = strtof64(value, nullptr);
            m_visitor.enterDouble(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_STRING:
        m_visitor.enterString(*m_fieldCurrent, value, size);
        break;
    case MetaType::TYPE_BYTES:
        // todo: convert from base64
        m_visitor.enterBytes(*m_fieldCurrent, value, size);
        break;
    case MetaType::TYPE_ENUM:
        m_visitor.enterEnum(*m_fieldCurrent, value, size);
        break;
    case MetaType::TYPE_ARRAY_BOOL:
        {
            bool v = (size == 4 && (memcmp(value, "true", 4) == 0));
            m_arrayBool.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_INT32:
        {
            std::int32_t v = strtol(value, nullptr, 10);
            m_arrayInt32.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_UINT32:
        {
            std::uint32_t v = strtoull(value, nullptr, 10);
            m_arrayUInt32.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_INT64:
        {
            std::int64_t v = strtoll(value, nullptr, 10);
            m_arrayInt64.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_UINT64:
        {
            std::uint64_t v = strtoull(value, nullptr, 10);
            m_arrayUInt64.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_FLOAT:
        {
            float v = strtof32(value, nullptr);
            m_arrayFloat.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_DOUBLE:
        {
            double v = strtof64(value, nullptr);
            m_arrayDouble.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_STRING:
        m_arrayString.emplace_back(value, size);
        break;
    case MetaType::TYPE_ARRAY_BYTES:
        {
            // todo: convert from base64
            m_arrayBytes.emplace_back(value, value + size);
        }
        break;
    case MetaType::TYPE_ARRAY_ENUM:
        if (!m_arrayString.empty() || m_arrayInt32.empty())
        {
            m_arrayString.emplace_back(value, size);
        }
        else
        {
            std::int32_t valueInt = MetaDataGlobal::instance().getEnumValueByName(*m_fieldCurrent, std::string(value, size));
            m_arrayInt32.push_back(valueInt);
        }
        break;
    default:
        std::cout << "string not expected";
        break;
    }
}


void ParserJson::enterString(std::string&& value)
{
    if (!m_fieldCurrent)
    {
        // unknown key
        return;
    }
    switch (m_fieldCurrent->type)
    {
    case MetaType::TYPE_BOOL:
        {
            bool v = (value.size() == 4 && (memcmp(value.c_str(), "true", 4) == 0));
            m_visitor.enterBool(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_INT32:
        {
            std::int32_t v = strtol(value.c_str(), nullptr, 10);
            m_visitor.enterInt32(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_UINT32:
        {
            std::uint32_t v = strtoul(value.c_str(), nullptr, 10);
            m_visitor.enterUInt32(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_INT64:
        {
            std::int64_t v = strtoll(value.c_str(), nullptr, 10);
            m_visitor.enterInt64(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_UINT64:
        {
            std::uint64_t v = strtoull(value.c_str(), nullptr, 10);
            m_visitor.enterUInt64(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_FLOAT:
        {
            float v = strtof32(value.c_str(), nullptr);
            m_visitor.enterFloat(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_DOUBLE:
        {
            double v = strtof64(value.c_str(), nullptr);
            m_visitor.enterDouble(*m_fieldCurrent, v);
        }
        break;
    case MetaType::TYPE_STRING:
        m_visitor.enterString(*m_fieldCurrent, std::move(value));
        break;
    case MetaType::TYPE_BYTES:
        // todo: convert from base64
        m_visitor.enterBytes(*m_fieldCurrent, value.data(), value.size());
        break;
    case MetaType::TYPE_ENUM:
        m_visitor.enterEnum(*m_fieldCurrent, std::move(value));
        break;
    case MetaType::TYPE_ARRAY_BOOL:
        {
            bool v = (value.size() == 4 && (memcmp(value.c_str(), "true", 4) == 0));
            m_arrayBool.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_INT32:
        {
            std::int32_t v = strtol(value.c_str(), nullptr, 10);
            m_arrayInt32.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_UINT32:
        {
            std::uint32_t v = strtoul(value.c_str(), nullptr, 10);
            m_arrayUInt32.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_INT64:
        {
            std::int64_t v = strtoll(value.c_str(), nullptr, 10);
            m_arrayInt64.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_UINT64:
        {
            std::uint64_t v = strtoull(value.c_str(), nullptr, 10);
            m_arrayUInt64.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_FLOAT:
        {
            float v = strtof32(value.c_str(), nullptr);
            m_arrayFloat.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_DOUBLE:
        {
            double v = strtof64(value.c_str(), nullptr);
            m_arrayDouble.push_back(v);
        }
        break;
    case MetaType::TYPE_ARRAY_STRING:
        m_arrayString.push_back(std::move(value));
        break;
    case MetaType::TYPE_ARRAY_BYTES:
        {
            // todo: convert from base64
            m_arrayBytes.emplace_back(value.data(), value.data() + value.size());
        }
        break;
    case MetaType::TYPE_ARRAY_ENUM:
        if (!m_arrayString.empty() || m_arrayInt32.empty())
        {
            m_arrayString.push_back(std::move(value));
        }
        else
        {
            std::int32_t valueInt = MetaDataGlobal::instance().getEnumValueByName(*m_fieldCurrent, value);
            m_arrayInt32.push_back(valueInt);
        }
        break;
    default:
        std::cout << "string not expected";
        break;
    }
}

void ParserJson::enterArray()
{
    if (m_fieldCurrent && ((int)m_fieldCurrent->type & (int)MetaType::TYPE_ARRAY_FLAG))
    {
        switch (m_fieldCurrent->type)
        {
        case MetaType::TYPE_ARRAY_BOOL:
            m_arrayBool.clear();
            break;
        case MetaType::TYPE_ARRAY_INT32:
            m_arrayInt32.clear();
            break;
        case MetaType::TYPE_ARRAY_UINT32:
            m_arrayUInt32.clear();
            break;
        case MetaType::TYPE_ARRAY_INT64:
            m_arrayInt64.clear();
            break;
        case MetaType::TYPE_ARRAY_UINT64:
            m_arrayUInt64.clear();
            break;
        case MetaType::TYPE_ARRAY_FLOAT:
            m_arrayFloat.clear();
            break;
        case MetaType::TYPE_ARRAY_DOUBLE:
            m_arrayDouble.clear();
            break;
        case MetaType::TYPE_ARRAY_STRING:
            m_arrayString.clear();
            break;
        case MetaType::TYPE_ARRAY_BYTES:
            m_arrayString.clear();
            break;
        case MetaType::TYPE_ARRAY_ENUM:
            m_arrayInt32.clear();
            m_arrayString.clear();
            break;
        case MetaType::TYPE_ARRAY_STRUCT:
            m_visitor.enterArrayStruct(*m_fieldCurrent);
            m_stack.emplace_back(m_fieldCurrent);
            m_fieldCurrent = MetaDataGlobal::instance().getArrayField(*m_fieldCurrent);
            m_stack.emplace_back(m_fieldCurrent);
            m_structCurrent = nullptr;
            break;
        default:
            assert(false);
            break;
        }
    }
}

void ParserJson::exitArray()
{
    if (!m_fieldCurrent)
    {
        // unknown key
        return;
    }

    if ((int)m_fieldCurrent->type & (int)MetaType::TYPE_ARRAY_FLAG)
    {
        switch (m_fieldCurrent->type)
        {
        case MetaType::TYPE_ARRAY_BOOL:
            m_visitor.enterArrayBoolMove(*m_fieldCurrent, std::move(m_arrayBool));
            m_arrayBool.clear();
            break;
        case MetaType::TYPE_ARRAY_INT32:
            m_visitor.enterArrayInt32(*m_fieldCurrent, std::move(m_arrayInt32));
            m_arrayInt32.clear();
            break;
        case MetaType::TYPE_ARRAY_UINT32:
            m_visitor.enterArrayUInt32(*m_fieldCurrent, std::move(m_arrayUInt32));
            m_arrayUInt32.clear();
            break;
        case MetaType::TYPE_ARRAY_INT64:
            m_visitor.enterArrayInt64(*m_fieldCurrent, std::move(m_arrayInt64));
            m_arrayInt64.clear();
            break;
        case MetaType::TYPE_ARRAY_UINT64:
            m_visitor.enterArrayUInt64(*m_fieldCurrent, std::move(m_arrayUInt64));
            m_arrayUInt64.clear();
            break;
        case MetaType::TYPE_ARRAY_FLOAT:
            m_visitor.enterArrayFloat(*m_fieldCurrent, std::move(m_arrayFloat));
            m_arrayFloat.clear();
            break;
        case MetaType::TYPE_ARRAY_DOUBLE:
            m_visitor.enterArrayDouble(*m_fieldCurrent, std::move(m_arrayDouble));
            m_arrayDouble.clear();
            break;
        case MetaType::TYPE_ARRAY_STRING:
            m_visitor.enterArrayStringMove(*m_fieldCurrent, std::move(m_arrayString));
            m_arrayString.clear();
            break;
        case MetaType::TYPE_ARRAY_BYTES:
            m_visitor.enterArrayBytesMove(*m_fieldCurrent, std::move(m_arrayBytes));
            m_arrayBytes.clear();
            break;
        case MetaType::TYPE_ARRAY_ENUM:
            if (!m_arrayString.empty())
            {
                m_visitor.enterArrayEnum(*m_fieldCurrent, std::move(m_arrayString));
            }
            else
            {
                m_visitor.enterArrayEnum(*m_fieldCurrent, std::move(m_arrayInt32));
            }
            m_arrayInt32.clear();
            m_arrayString.clear();
            break;
        case MetaType::TYPE_ARRAY_STRUCT:
            assert(false);
            break;
        default:
            assert(false);
            break;
        }
    }
    else if (m_fieldCurrent->type == MetaType::TYPE_STRUCT)
    {
        m_structCurrent = nullptr;
        m_fieldCurrent = nullptr;
        if (!m_stack.empty())
        {
            m_stack.pop_back();
            m_fieldCurrent = m_stack.back().field;
            m_stack.pop_back();
        }
        m_visitor.exitArrayStruct(*m_fieldCurrent);
    }
}

void ParserJson::enterObject()
{
    m_stack.emplace_back(m_fieldCurrent);
    m_structCurrent = nullptr;
    if (m_fieldCurrent && m_fieldCurrent->type == MetaType::TYPE_STRUCT)
    {
        const MetaStruct* stru = MetaDataGlobal::instance().getStruct(*m_fieldCurrent);
        if (stru)
        {
            m_structCurrent = stru;
            m_visitor.enterStruct(*m_fieldCurrent);
            m_fieldCurrent = nullptr;
        }
        else
        {
            m_fieldCurrent = nullptr;
        }
    }
    else
    {
//        std::cout << "struct not expected" << std::endl;
        m_fieldCurrent = nullptr;
    }
}

void ParserJson::exitObject()
{
    m_structCurrent = nullptr;
    m_fieldCurrent = nullptr;
    if (!m_stack.empty())
    {
        m_fieldCurrent = m_stack.back().field;
        if (m_fieldCurrent)
        {
            m_visitor.exitStruct(*m_fieldCurrent);
        }
        m_stack.pop_back();
    }
    if (!m_stack.empty())
    {
        m_fieldCurrent = m_stack.back().field;
        if (m_fieldCurrent)
        {
            m_structCurrent = MetaDataGlobal::instance().getStruct(*m_fieldCurrent);
        }
    }
}

void ParserJson::enterKey(const char* key, int size)
{
    enterKey(std::string(key, size));
}

void ParserJson::enterKey(std::string&& key)
{
    m_fieldCurrent = nullptr;
    if (m_structCurrent)
    {
        m_fieldCurrent = m_structCurrent->getFieldByName(key);
    }
}

void ParserJson::finished()
{
}


