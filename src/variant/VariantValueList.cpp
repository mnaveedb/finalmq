#include "variant/VariantValueList.h"
#include "variant/Variant.h"
#include <utility>
#include <assert.h>


VariantValueList::VariantValueList()
{
}

VariantValueList::VariantValueList(const VariantValueList& rhs)
    : m_value(std::make_unique<VariantList>(*rhs.m_value))
{
}

VariantValueList::VariantValueList(VariantValueList&& rhs)
    : m_value(std::move(rhs.m_value))
{
}

VariantValueList::VariantValueList(const VariantList& value)
    : m_value(std::make_unique<VariantList>(value))
{
}

VariantValueList::VariantValueList(VariantList&& value)
    : m_value(std::make_unique<VariantList>(std::move(value)))
{
}

int VariantValueList::getType() const
{
    return VARTYPE_LIST;
}

void* VariantValueList::getData()
{
    return m_value.get();
}


const void* VariantValueList::getData() const
{
    return const_cast<VariantValueList*>(this)->getData();
}


VariantList::iterator VariantValueList::find(const std::string& name)
{
    int index = std::atoi(name.c_str());
    if (index >= 0 && index < static_cast<int>(m_value->size()))
    {
        return m_value->begin() + index;
    }
    return m_value->end();
}




Variant* VariantValueList::getVariant(const std::string& name)
{
    if (name.empty())
    {
        return nullptr;
    }

    std::string partname;
    std::string restname;

    //sperate first key ansd second key
    size_t cntp = name.find('.');
    if (cntp != std::string::npos)
    {
        partname = name.substr(0, cntp);
        cntp++;
        restname = name.substr(cntp);
    }
    else
    {
        partname = name;
    }

    // check if next key is in map (if not -> nullptr)
    auto it = find(partname);   // auto: std::unordered_map<std::string, Variant>::iterator
    if (it == m_value->end())
    {
        return nullptr;
    }

    // m_value[name].getValue( with remaining name )
    return it->getVariant(restname);
}


const Variant* VariantValueList::getVariant(const std::string& name) const
{
    return const_cast<VariantValueList*>(this)->getVariant(name);
}



std::shared_ptr<IVariantValue> VariantValueList::clone() const
{
    return std::make_shared<VariantValueList>(*this);
}


bool VariantValueList::operator ==(const IVariantValue& rhs) const
{
    if (this == &rhs)
    {
        return true;
    }

    if (getType() != rhs.getType())
    {
        return false;
    }

    const VariantList* rhsData = static_cast<const VariantList*>(rhs.getData());
    assert(rhsData);

    assert(m_value);

    return *m_value == *rhsData;
}


bool VariantValueList::add(const std::string& name, const Variant& variant)
{
    return false;
}
bool VariantValueList::add(const std::string& name, Variant&& variant)
{
    return false;
}

bool VariantValueList::add(const Variant& variant)
{
    m_value->push_back(variant);
    return true;
}
bool VariantValueList::add(Variant&& variant)
{
    m_value->push_back(std::move(variant));
    return true;
}

int VariantValueList::size() const
{
    return m_value->size();
}

void VariantValueList::visit(IVariantVisitor& visitor, Variant& variant, int index, int level, int size, const std::string& name)
{
    visitor.enterList(variant, VARTYPE_LIST, index, level, size, name);
    ++level;
    int i = 0;
    int subsize = m_value->size();
    for (auto it = m_value->begin(); it != m_value->end(); ++it)
    {
        Variant& subVariant = *it;
        subVariant.visit(visitor, i, level, subsize, "");
        i++;
    }
    --level;
    visitor.exitList(variant, VARTYPE_LIST, index, level, size, name);
}
