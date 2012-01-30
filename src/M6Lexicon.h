#pragma once

#include <string>
#include <vector>

#include <boost/thread.hpp>

// M6Lexicon stores text strings in an ordered way using the least
// possible amount of memory. Each text string gets a unique number.
// Strings can be accessed by this number and the number for a string
// can be found very quickly since the storage is based on a Patricia tree.

class M6Lexicon
{
	friend struct CLexPage;
	
  public:

	// since we now have several locations where a M6Lexicon class
	// is used, we have the option to create a lightweight version.
					M6Lexicon();
	virtual			~M6Lexicon();
	
	// M6Lexicon is one of the most heavily used classes.
	// To optimize, we now use separate threads but there
	// can only be one lexicon. So we need locking.
	// Profiling has shown that locking is taking a
	// disproportionate amount of time, so we split the
	// code into a part that looks up words first in a
	// shared lock mode and then stores those unknown
	// in a unique lock mode.
	
	void			LockShared()				{ mMutex.lock_shared(); }
	void			UnlockShared()				{ mMutex.unlock_shared(); }
	
	void			LockUnique()				{ mMutex.lock(); }
	void			UnlockUnique()				{ mMutex.unlock(); }
	
	uint32			Lookup(const std::string& inWord) const;
	uint32			Lookup(const char* inWord, size_t inWordLength) const;

	uint32			Store(const std::string& inWord);
	uint32			Store(const char* inWord, size_t inWordLength);

	std::string		GetString(uint32 inNr) const;
	void			GetString(uint32 inNr, const char*& outWord, size_t& outWordLength) const;

	int				Compare(uint32 inA, uint32 inB) const;

	uint32			Count() const;
	
  private:
					M6Lexicon(const M6Lexicon&);
	M6Lexicon&		operator=(const M6Lexicon&);
	
	struct M6LexiconImpl*	mImpl;
	boost::shared_mutex		mMutex;
};

// --------------------------------------------------------------------

inline uint32 M6Lexicon::Lookup(const std::string& inWord) const
{
	return Lookup(inWord.c_str(), inWord.length());
}

inline uint32 M6Lexicon::Store(const std::string& inWord)
{
	return Store(inWord.c_str(), inWord.length());
}

inline std::string M6Lexicon::GetString(uint32 inNr) const
{
	const char* w;
	size_t l;

	GetString(inNr, w, l);

	return std::string(w, l);
}
