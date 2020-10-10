#pragma once

#include <string>
#include <memory>
#include <string>
#include <assert.h>

#include "IVariantValue.h"
#include "metadata/MetaType.h"





//////////////////////

const static int VARTYPE_NONE = 0;

class Variant
{
public:
    Variant();

    Variant(std::shared_ptr<IVariantValue> value);

    template<class T>
    Variant(T data)
        : m_value(std::make_shared<typename VariantValueTypeInfo<T>::VariantValueType>(std::move(data)))
    {
    }

    template<class T>
    operator T*()
    {
        if (m_value)
        {
            if (m_value->getType() == VariantValueTypeInfo<T>::VARTYPE)
            {
                return static_cast<T*>(m_value->getData());
            }
        }
        return nullptr;
    }

    template<class T>
    operator const T*() const
    {
        if (m_value)
        {
            if (m_value->getType() == VariantValueTypeInfo<T>::VARTYPE)
            {
                return static_cast<const T*>(m_value->getData());
            }
        }
        return nullptr;
    }

    template<class T>
    operator T() const
    {
        return VariantValueTypeInfo<T>::ConvertType::convert(*this);
    }

    template<class T>
    void convertVariantTo()
    {
        *this = VariantValueTypeInfo<T>::ConvertType::convert(*this);
    }

    template<class T>
    T* getData(const std::string& name)
    {
        std::string nameRemaining;
        Variant* variant = getVariant(name, nameRemaining);
        if (variant)
        {
            std::shared_ptr<IVariantValue>& value = variant->m_value;
            if (value)
            {
                if (!nameRemaining.empty())
                {
                    int type = value->getType();
                    type &= ~TYPE_ARRAY_FLAG;
                    if (type == VariantValueTypeInfo<T>::VARTYPE)
                    {
                        return static_cast<T*>(value->getData(nameRemaining));
                    }
                }
                else
                {
                    if (m_value->getType() == VariantValueTypeInfo<T>::VARTYPE)
                    {
                        return static_cast<T*>(m_value->getData());
                    }
                }
            }
        }
        return nullptr;
    }

    template<class T>
    const T* getData(const std::string& name) const
    {
        return const_cast<Variant*>(this)->getData<T>(name);
    }

    Variant(const Variant& rhs);
    const Variant& operator =(const Variant& rhs);
    Variant(Variant&& rhs);
    Variant& operator =(Variant&& rhs);

    void visit(IVariantVisitor& visitor, int index = 0, int level = 0, int size = 0, const std::string& name = "");

    int getType() const;
//    std::shared_ptr<IVariantValue> getValue(const std::string& name = "");
    Variant* getVariant(const std::string& name);
    const Variant* getVariant(const std::string& name) const;

    bool add(const std::string& name, const Variant& variant);
    bool add(const std::string& name, Variant&& variant);
    bool add(const Variant& variant);
    bool add(Variant&& variant);
    int size() const;

    Variant* getVariant(const std::string& name, std::string& nameRemaining);
    const Variant* getVariant(const std::string& name, std::string& nameRemaining) const;

private:

    std::shared_ptr<IVariantValue> m_value;
};



