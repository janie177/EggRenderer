#pragma once
#include <queue>

namespace egg
{
    template<typename T>
    class HandleRecycler
    {
        static_assert(std::is_integral_v<T>, "T has to be integral type.");
    public:
        HandleRecycler();

        /*
         * Get a free handle.
         */
        inline T GetHandle();

        /*
         * Recycle a handle.
         */
        inline void Recycle(const T& a_Handle);

    private:
        T m_Counter;
        std::queue<T> m_FreedHandles;
    };

    template <typename T>
    HandleRecycler<T>::HandleRecycler() : m_Counter(0)
    {
        
    }

    template <typename T>
    T HandleRecycler<T>::GetHandle()
    {
        if(m_FreedHandles.empty())
        {
            auto value = m_FreedHandles.front();
            m_FreedHandles.pop();
            return value;
        }
        return m_Counter++;
    }

    template <typename T>
    void HandleRecycler<T>::Recycle(const T& a_Handle)
    {
        m_FreedHandles.push(a_Handle);
    }
}
