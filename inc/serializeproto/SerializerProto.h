#pragma once

#include "metadata/MetaStruct.h"
#include "helpers/IZeroCopyBuffer.h"
#include "serialize/IParserVisitor.h"

#include <deque>


//TODO: derive from ParserConverter, see SerializerJson
class SerializerProto : public IParserVisitor
{
public:
    SerializerProto(IZeroCopyBuffer& buffer, int maxBlockSize = 1024);

private:
    // IParserVisitor
    virtual void notifyError(const char* str, const char* message) override;
    virtual void finished() override;

    virtual void enterStruct(const MetaField& field) override;
    virtual void exitStruct(const MetaField& field) override;

    virtual void enterArrayStruct(const MetaField& field) override;
    virtual void exitArrayStruct(const MetaField& field) override;

    virtual void enterBool(const MetaField& field, bool value) override;
    virtual void enterInt32(const MetaField& field, std::int32_t value) override;
    virtual void enterUInt32(const MetaField& field, std::uint32_t value) override;
    virtual void enterInt64(const MetaField& field, std::int64_t value) override;
    virtual void enterUInt64(const MetaField& field, std::uint64_t value) override;
    virtual void enterFloat(const MetaField& field, float value) override;
    virtual void enterDouble(const MetaField& field, double value) override;
    virtual void enterString(const MetaField& field, std::string&& value) override;
    virtual void enterString(const MetaField& field, const char* value, int size) override;
    virtual void enterBytes(const MetaField& field, Bytes&& value) override;
    virtual void enterBytes(const MetaField& field, const unsigned char* value, int size) override;
    virtual void enterEnum(const MetaField& field, std::int32_t value) override;
    virtual void enterEnum(const MetaField& field, std::string&& value) override;
    virtual void enterEnum(const MetaField& field, const char* value, int size) override;

    virtual void enterArrayBoolMove(const MetaField& field, std::vector<bool>&& value) override;
    virtual void enterArrayBool(const MetaField& field, const std::vector<bool>& value) override;
    virtual void enterArrayInt32(const MetaField& field, std::vector<std::int32_t>&& value) override;
    virtual void enterArrayInt32(const MetaField& field, const std::int32_t* value, int size) override;
    virtual void enterArrayUInt32(const MetaField& field, std::vector<std::uint32_t>&& value) override;
    virtual void enterArrayUInt32(const MetaField& field, const std::uint32_t* value, int size) override;
    virtual void enterArrayInt64(const MetaField& field, std::vector<std::int64_t>&& value) override;
    virtual void enterArrayInt64(const MetaField& field, const std::int64_t* value, int size) override;
    virtual void enterArrayUInt64(const MetaField& field, std::vector<std::uint64_t>&& value) override;
    virtual void enterArrayUInt64(const MetaField& field, const std::uint64_t* value, int size) override;
    virtual void enterArrayFloat(const MetaField& field, std::vector<float>&& value) override;
    virtual void enterArrayFloat(const MetaField& field, const float* value, int size) override;
    virtual void enterArrayDouble(const MetaField& field, std::vector<double>&& value) override;
    virtual void enterArrayDouble(const MetaField& field, const double* value, int size) override;
    virtual void enterArrayStringMove(const MetaField& field, std::vector<std::string>&& value) override;
    virtual void enterArrayString(const MetaField& field, const std::vector<std::string>& value) override;
    virtual void enterArrayBytesMove(const MetaField& field, std::vector<Bytes>&& value) override;
    virtual void enterArrayBytes(const MetaField& field, const std::vector<Bytes>& value) override;
    virtual void enterArrayEnum(const MetaField& field, std::vector<std::int32_t>&& value) override;
    virtual void enterArrayEnum(const MetaField& field, const std::int32_t* value, int size) override;
    virtual void enterArrayEnumMove(const MetaField& field, std::vector<std::string>&& value) override;
    virtual void enterArrayEnum(const MetaField& field, const std::vector<std::string>& value) override;

    int m_levelStruct = 0;

    template<bool ignoreZeroLength>
    void serializeString(int id, const char* value, int size);

    char* serializeStruct(int id);

    template<class T>
    void serializeVarintValue(int id, T value);

    template<class T>
    void serializeZigZagValue(int id, T value);

    inline std::uint32_t zigzag(std::int32_t value);
    inline std::uint64_t zigzag(std::int64_t value);

    template<class T, int WIRETYPE>
    void serializeFixedValue(int id, T value);

    template<class T>
    void serializeArrayFixed(int id, const T* value, int size);

    void serializeArrayBool(int id, const std::vector<bool>& value);
    void serializeArrayString(int id, const std::vector<std::string>& value);
    void serializeArrayBytes(int id, const std::vector<Bytes>& value);

    template<class T>
    void serializeArrayVarint(int id, const T* value, int size);

    template<class T>
    void serializeArrayZigZag(int id, const T* value, int size);

    inline void serializeVarint(std::uint64_t value);
    void reserveSpace(int space);
    void resizeBuffer();
    int calculateStructSize(int& structSize);
    void fillRemainingStruct(int remainingSize);

    struct StructData
    {
        StructData(char* bstart, char* bsize, char* b, bool ae)
            : bufferStructStart(bstart)
            , bufferStructSize(bsize)
            , buffer(b)
            , arrayEntry(ae)
        {
        }
        char*   bufferStructStart = nullptr;
        char*   bufferStructSize = nullptr;
        char*   buffer = nullptr;
        int     size = 0;
        bool    allocateNextDataBuffer = false;
        bool    arrayParent = false;
        bool    arrayEntry = false;
    };

    IZeroCopyBuffer&        m_zeroCopybuffer;
    int                     m_maxBlockSize = 1024;
    char*                   m_bufferStart = nullptr;
    char*                   m_buffer = nullptr;
    char*                   m_bufferEnd = nullptr;
    std::deque<StructData>  m_stackStruct;
};
