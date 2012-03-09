//	Basic index class using B+ tree data structure
//	
//	

#pragma once

#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem/path.hpp>

#include "M6File.h"
#include "M6BitStream.h"

struct M6IndexImpl;
class M6CompressedArray;
union M6BitVector;
class M6Progress;
class M6Iterator;

extern const uint32 kM6MaxKeyLength;

class M6BasicIndex
{
  public:
	virtual			~M6BasicIndex();

	static M6BasicIndex*
					Load(const boost::filesystem::path& inPath);

	virtual M6IndexType
					GetIndexType() const;

	void			Commit();
	void			Rollback();
	void			SetAutoCommit(bool inAutoCommit);
	void			SetBatchMode(bool inBatchMode);
	
	void			Vacuum(M6Progress& inProgress);
	
	virtual int		CompareKeys(const char* inKeyA, size_t inKeyLengthA,
						const char* inKeyB, size_t inKeyLengthB) const = 0;

	// iterator is used to iterate over the keys in an index.
	// To access the accompanying data, use the M6Iterator producing
	// method.

	class iterator : public std::iterator<std::forward_iterator_tag,const std::string>
	{
	  public:
		typedef std::iterator<std::forward_iterator_tag, const std::string>	base_type;
		typedef base_type::reference										reference;
		typedef base_type::pointer											pointer;
		
						iterator();
						iterator(const iterator& iter);
		iterator&		operator=(const iterator& iter);
	
		reference		operator*() const							{ return mCurrent; }
		pointer			operator->() const							{ return &mCurrent; }
	
		iterator&		operator++();
		iterator		operator++(int)								{ iterator iter(*this); operator++(); return iter; }
	
		bool			operator==(const iterator& iter) const		{ return mIndex == iter.mIndex and mPage == iter.mPage and mKeyNr == iter.mKeyNr; }
		bool			operator!=(const iterator& iter) const		{ return not operator==(iter); }

		M6Iterator*		GetDocuments() const;
		uint32			GetCount() const;
	
	  private:
		friend struct M6IndexImpl;
	
						iterator(M6IndexImpl* inIndex, uint32 inPageNr, uint32 inKeyNr);
	
		M6IndexImpl*	mIndex;
		uint32			mPage;
		uint32			mKeyNr;
		std::string		mCurrent;
	};
	
	// lame, but works for now (needed for boost::range)
	typedef iterator const_iterator;
	
	iterator		begin() const;
	iterator		end() const;
	
	iterator		lower_bound(const std::string& inKey) const;
	iterator		upper_bound(const std::string& inKey) const;
	
	void			Insert(const std::string& inKey, uint32 inValue);
	void			Erase(const std::string& inKey);
	bool			Find(const std::string& inKey, uint32& outValue);
	
	M6Iterator*		Find(const std::string& inKey);
	M6Iterator*		FindString(const std::string& inString);

	uint32			size() const;
	uint32			depth() const;

	void			Dump() const;
	void			Validate() const;

  protected:
					M6BasicIndex(const boost::filesystem::path& inPath,
						M6IndexType inIndexType, MOpenMode inMode);
					M6BasicIndex(M6IndexImpl* inImpl);

	M6IndexImpl*	mImpl;
};

template<class INDEX, class COMPARATOR, M6IndexType TYPE> class M6Index : public INDEX
{
  public:
	typedef COMPARATOR			M6Comparator;

					M6Index(const boost::filesystem::path& inPath, MOpenMode inMode)
						: INDEX(inPath, TYPE, inMode) {}

	virtual int		CompareKeys(const char* inKeyA, size_t inKeyLengthA,
								const char* inKeyB, size_t inKeyLengthB) const
						{ return mComparator(inKeyA, inKeyLengthA, inKeyB, inKeyLengthB); }

  protected:
	M6Comparator	mComparator;
};

// simplistic comparator, based on strncmp
struct M6BasicComparator
{
	int operator()(const char* inKeyA, size_t inKeyLengthA, const char* inKeyB, size_t inKeyLengthB) const
	{
		size_t l = inKeyLengthA;
		if (l > inKeyLengthB)
			l = inKeyLengthB;
		int d = strncmp(inKeyA, inKeyB, l);
		if (d == 0)
			d = static_cast<int>(inKeyLengthA - inKeyLengthB);
		return d;
	}
};

struct M6NumericComparator
{
	int operator()(const char* inKeyA, size_t inKeyLengthA, const char* inKeyB, size_t inKeyLengthB) const
	{
#pragma message("TODO improve numeric comparison")
		int d = 0;
		const char* ai = inKeyA + inKeyLengthA;
		const char* bi = inKeyB + inKeyLengthB;
		while (ai > inKeyA and bi > inKeyB)
		{
			--ai; --bi;
			if (*ai != *bi)
				d = *ai - *bi;
		}

		while (ai > inKeyA)
		{
			--ai;
			if (*ai != '0')
			{
				d = 1;
				break;
			}
		}

		while (bi > inKeyB)
		{
			--bi;
			if (*bi != '0')
			{
				d = -1;
				break;
			}
		}

		return d;
	}
};

typedef M6Index<M6BasicIndex, M6BasicComparator, eM6CharIndex>	M6SimpleIndex;
typedef M6Index<M6BasicIndex, M6NumericComparator, eM6NumberIndex>	M6NumberIndex;

// --------------------------------------------------------------------

class M6MultiBasicIndex : public M6BasicIndex
{
  public:
					M6MultiBasicIndex(const boost::filesystem::path& inPath, M6IndexType inIndexType, MOpenMode inMode);

	void			Insert(const std::string& inKey, const std::vector<uint32>& inDocuments);
	bool			Find(const std::string& inKey, M6CompressedArray& outDocuments);
};

typedef M6Index<M6MultiBasicIndex, M6BasicComparator, eM6CharMultiIndex> M6SimpleMultiIndex;
typedef M6Index<M6MultiBasicIndex, M6NumericComparator, eM6NumberMultiIndex> M6NumberMultiIndex;

// --------------------------------------------------------------------

class M6MultiIDLBasicIndex : public M6BasicIndex
{
  public:
					M6MultiIDLBasicIndex(const boost::filesystem::path& inPath, M6IndexType inIndexType, MOpenMode inMode);

	void			Insert(const std::string& inKey, int64 inIDLOffset, const std::vector<uint32>& inDocuments);
	bool			Find(const std::string& inKey, M6CompressedArray& outDocuments, int64& outIDLOffset);
};

typedef M6Index<M6MultiIDLBasicIndex, M6BasicComparator, eM6CharMultiIDLIndex> M6SimpleIDLMultiIndex;

// --------------------------------------------------------------------

class M6WeightedBasicIndex : public M6BasicIndex
{
  public:
					M6WeightedBasicIndex(const boost::filesystem::path& inPath, M6IndexType inIndexType, MOpenMode inMode);

	class M6WeightedIterator
	{
	  public:
						M6WeightedIterator();
						M6WeightedIterator(M6IndexImpl& inIndex, const M6BitVector& inBitVector, uint32 inCount);
						M6WeightedIterator(const M6WeightedIterator&);
		M6WeightedIterator&
						operator=(const M6WeightedIterator&);

		uint32			Size() const								{ return mSize; }
		bool			Next(uint32& outDocNr, uint8& outWeight);

	  private:
		M6IBitStream	mBits;
		std::vector<uint32>
						mDocs;
		uint32			mSize;
		uint8			mWeight;
	};
	
	void			Insert(const std::string& inKey, std::vector<std::pair<uint32,uint8>>& inDocuments);
	bool			Find(const std::string& inKey, M6WeightedIterator& outIterator);
	void			CalculateDocumentWeights(uint32 inDocCount, std::vector<float>& outWeights,
						M6Progress& inProgress);
};

typedef M6Index<M6WeightedBasicIndex, M6BasicComparator, eM6CharWeightedIndex>	M6SimpleWeightedIndex;

