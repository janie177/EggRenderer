#pragma once
#include <functional>
#include <vector>
#include <memory>
#include <mutex>

/*
 * Basically a concurrent vector of shared pointers.
 */
template<typename T>
class ConcurrentRegistry
{
public:
	/*
	 * Add an item to the registry.
	 * This locks the internal mutex and is thus thread safe.
	 *
	 * Note: The same shared_ptr should only be added once, or it will become a memory leak.
	 */
	void Add(const std::shared_ptr<T>& a_Ptr);

	/*
	 * Clean up objects that are no longer referenced from the outside.
	 * The provided function is ran for all objects that are removed.
	 *
	 * The entry offset and num entries provided allow for a subset of the entries to be checked for removal.
	 * By default, all entries are checked.
	 *
	 * Entries are only erased when the provided function returns true.
	 * Entries are skipped over when the provided function returns false.
	 */
	void RemoveUnused(
		const std::function<bool(T& a_Entry)>& a_OnRemoveFunc,
		size_t a_EntryOffset = 0,
		size_t a_NumEntries = std::numeric_limits<size_t>::max()
	);

	/*
	 * Get the amount of items in this registry.
	 */
	size_t GetSize() const;

	/*
	 * Clear all entries.
	 * The function provided is ran for each entry.
	 *
	 * Note that remaining shared_ptr
	 */
	void RemoveAll(const std::function<void(T& a_Entry)>& a_OnRemoveFunc);

private:
	std::vector<std::shared_ptr<T>> m_Vector;
	std::mutex m_Mutex;
	
};

template <typename T>
void ConcurrentRegistry<T>::Add(const std::shared_ptr<T>& a_Ptr)
{
	//Don't add nullptr.
	if(a_Ptr != nullptr)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_Vector.push_back(a_Ptr);
	}
}

template <typename T>
void ConcurrentRegistry<T>::RemoveUnused(
	const std::function<bool(T& a_Entry)>& a_OnRemoveFunc,
	size_t a_EntryOffset,
	size_t a_NumEntries
)
{
	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	int counter = 0;
	auto itr = m_Vector.begin() + a_EntryOffset;
	while(itr < m_Vector.end() && counter <= a_NumEntries)
	{
		//If only one reference, and the function also returns true; remove.
		if((*itr).use_count() < 2 && a_OnRemoveFunc(*(*itr)))
		{
			itr = m_Vector.erase(itr);
		}
		else
		{
			++itr;
		}
		
		++counter;
	}
}

template <typename T>
size_t ConcurrentRegistry<T>::GetSize() const
{
	return m_Vector.size();
}

template <typename T>
void ConcurrentRegistry<T>::RemoveAll(const std::function<void(T& a_Entry)>& a_OnRemoveFunc)
{
	std::lock_guard<std::mutex> lockGuard(m_Mutex);
	for (auto& entry : m_Vector)
	{
		a_OnRemoveFunc(*entry);
	}
	m_Vector.clear();
}