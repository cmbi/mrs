//   Copyright Maarten L. Hekkelman, Radboud University 2012.
//  Distributed under the Boost Software License, Version 1.0.
//     (See accompanying file LICENSE_1_0.txt or copy at
//           http://www.boost.org/LICENSE_1_0.txt)
//  Also distributed under the Lesser General Public License, Version 2.1.
//     (See accompanying file lgpl-2.1.txt or copy at
//           https://www.gnu.org/licenses/lgpl-2.1.txt)

#pragma once

#include <deque>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>

template<class T, uint32 N = 100>
class M6Queue
{
  public:
						M6Queue();
						~M6Queue();

	void				Put(T inValue);
//	void				Put(T inValue[], uint32 inCount);
	T					Get();

	// flags to help debug performance issues
	bool				WasFull() const		{ return mWasFull; }
	bool				WasEmpty() const	{ return mWasEmpty; }

  private:
						M6Queue(const M6Queue&);
	M6Queue&			operator=(const M6Queue&);

	std::deque<T>		mQueue;
	boost::mutex		mMutex;
	std::unique_ptr<boost::condition>
						mEmptyCondition, mFullCondition;
	bool				mWasFull, mWasEmpty;
};

template<class T, uint32 N>
M6Queue<T,N>::M6Queue()
	: mEmptyCondition(new boost::condition), mFullCondition(new boost::condition)
	, mWasFull(false), mWasEmpty(true)
{
}

template<class T, uint32 N>
M6Queue<T,N>::~M6Queue()
{
}

template<class T, uint32 N>
void M6Queue<T,N>::Put(T inValue)
{
	boost::mutex::scoped_lock lock(mMutex);

	mWasFull = false;
	while (mQueue.size() >= N)
	{
		mFullCondition->wait(lock);
		mWasFull = true;
	}
	
	mQueue.push_back(inValue);

	mEmptyCondition->notify_one();
}

//template<class T, uint32 N>
//void M6Queue<T,N>::Put(T inValue[], uint32 inCount)
//{
//	boost::mutex::scoped_lock lock(mMutex);
//
//	mWasFull = false;
//	while (mQueue.size() >= N)
//	{
//		mFullCondition->wait(lock);
//		mWasFull = true;
//	}
//	
//	mQueue.append(inValue, inValue + inCount);
//
//	mEmptyCondition->notify_one();
//}

template<class T, uint32 N>
T M6Queue<T,N>::Get()
{
	boost::mutex::scoped_lock lock(mMutex);

	mWasEmpty = false;
	while (mQueue.empty())
	{
		mEmptyCondition->wait(lock);
		mWasEmpty = true;
	}
	
	T result = mQueue.front();
	mQueue.pop_front();

	mFullCondition->notify_one();
	
	return result;
}
