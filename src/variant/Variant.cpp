#include "variant/Variant.h"


Variant::Variant()
{
}

Variant::Variant(std::shared_ptr<IVariantValue> value)
    : m_value(value)
{

}


Variant::Variant(const Variant& rhs)
    : m_value((rhs.m_value) ? rhs.m_value->clone() : nullptr)
{
}

const Variant& Variant::operator =(const Variant& rhs)
{
    if (this == &rhs)
    {
        return *this;
    }
    if (rhs.m_value)
    {
        m_value = rhs.m_value->clone();
    }
    else
    {
        m_value = nullptr;
    }
    return *this;
}

Variant::Variant(Variant&& rhs)
    : m_value(std::move(rhs.m_value))
{

}

Variant& Variant::operator =(Variant&& rhs)
{
    if (this == &rhs)
    {
        return *this;
    }
    m_value = std::move(rhs.m_value);
    return *this;
}


void Variant::visit(IVariantVisitor& visitor, int index, int level, int size, const std::string& name)
{

    if (m_value)
    {
        m_value->visit(visitor, *this, index, level, size, name);
    }
    else
    {
       visitor.enterLeaf(*this, VARTYPE_NONE, index, level, size, name);
    }
}



int Variant::getType() const
{
    if (m_value)
    {
        return m_value->getType();
    }
    else
    {
        return VARTYPE_NONE;
    }
}


//std::shared_ptr<IVariantValue> Variant::getValue(const std::string& name)
//{
//    if (name.empty() || m_value == nullptr)
//    {
//        return m_value;
//    }
//    return m_value->getValue(name);
//}

Variant* Variant::getVariant(const std::string& name)
{
    std::string nameRemaining;
    return getVariant(name, nameRemaining);
}

const Variant* Variant::getVariant(const std::string& name) const
{
    std::string nameRemaining;
    return getVariant(name, nameRemaining);
}



Variant* Variant::getVariant(const std::string& name, std::string& nameRemaining)
{
    if (name.empty())
    {
        return this;
    }
    if (m_value == nullptr)
    {
        return nullptr;
    }

    bool found = false;
    Variant* result = m_value->getVariant(name, found, nameRemaining);
    if (result)
    {
        return result;
    }
    if (found)
    {
        return this;
    }
    return nullptr;
}


const Variant* Variant::getVariant(const std::string& name, std::string& nameRemaining) const
{
    return const_cast<Variant*>(this)->getVariant(name, nameRemaining);
}


bool Variant::add(const std::string& name, const Variant& variant)
{
    if (m_value)
    {
        return m_value->add(name, variant);
    }
    return false;
}
bool Variant::add(const std::string& name, Variant&& variant)
{
    if (m_value)
    {
        return m_value->add(name, std::move(variant));
    }
    return false;
}

bool Variant::add(const Variant& variant)
{
    if (m_value)
    {
        return m_value->add(variant);
    }
    return false;
}
bool Variant::add(Variant&& variant)
{
    if (m_value)
    {
        return m_value->add(std::move(variant));
    }
    return false;
}

int Variant::size() const
{
    if (m_value)
    {
        return m_value->size();
    }
    return 0;
}
