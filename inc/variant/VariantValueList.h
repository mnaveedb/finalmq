#pragma once

#include "IVariantValue.h"
#include "metadata/MetaType.h"
#include "VariantValueConvert.h"

#include <deque>

class Variant;

typedef std::deque<Variant> VariantList;

const static int VARTYPE_LIST = TYPE_ARRAY_STRUCT;
class VariantValueList : public IVariantValue
{
public:
    VariantValueList();
    VariantValueList(const VariantValueList& rhs);
    VariantValueList(VariantValueList&& rhs);
    VariantValueList(const VariantList& value);
    VariantValueList(VariantList&& value);

private:
    virtual int getType() const override;
    virtual void* getData() override;
    virtual const void* getData() const override;
    virtual Variant* getVariant(const std::string& name) override;
    virtual const Variant* getVariant(const std::string& name) const override;
    virtual std::shared_ptr<IVariantValue> clone() const override;
    virtual bool operator ==(const IVariantValue& rhs) const override;
    virtual bool add(const std::string& name, const Variant& variant) override;
    virtual bool add(const std::string& name, Variant&& variant) override;
    virtual bool add(const Variant& variant) override;
    virtual bool add(Variant&& variant) override;
    virtual int size() const override;
    virtual void visit(IVariantVisitor& visitor, Variant& variant, int index, int level, int size, const std::string& name) override;


    VariantList::iterator find(const std::string& name);

    std::unique_ptr<VariantList>     m_value;
};



template <>
class MetaTypeInfo<VariantList>
{
public:
    static const int TypeId = MetaTypeId::TYPE_ARRAY_STRUCT;
};


template<>
class VariantValueTypeInfo<VariantList&>
{
public:
    typedef VariantValueList VariantValueType;
    const static int VARTYPE = VARTYPE_LIST;
    typedef Convert<VariantList> ConvertType;
};

template<>
class VariantValueTypeInfo<VariantList>
{
public:
    typedef VariantValueList VariantValueType;
    const static int VARTYPE = VARTYPE_LIST;
    typedef Convert<VariantList> ConvertType;
};

