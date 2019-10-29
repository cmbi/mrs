//   Copyright Maarten L. Hekkelman, Radboud University 2012.
//  Distributed under the Boost Software License, Version 1.0.
//     (See accompanying file LICENSE_1_0.txt or copy at
//           http://www.boost.org/LICENSE_1_0.txt)

//	Document store. Store documents in pages and keep an index
//	to the start of each document.
//	The total size of a doc store is limited to 32 tera bytes.

#include "M6Lib.h"

#include <cassert>
#include <vector>
#include <iostream>
#include <atomic>

#include <boost/iostreams/categories.hpp>
#include <boost/thread.hpp>

#include "M6DocStore.h"
#include "M6Error.h"

using namespace std;
namespace io = boost::iostreams;
namespace fs = boost::filesystem;

#define swap_bytes(x) (x)

// --------------------------------------------------------------------

const uint32
	kM6DocStoreSignature	= 'm6ds';

const int64
	kM6DataPageSize			= 16384,
//	kM6DataPageSize			= 256,
	kM6DataPageTextSize		= kM6DataPageSize - 8,
	kM6DataPageIndexCount	= kM6DataPageTextSize / (3 * sizeof(uint32)),
	kM6DataPageTextCutOff	= 64;	// start a new data page if 'free' is less than this

enum M6DataPageType
{
	eM6DocStoreEmptyPage,
	eM6DocStoreDataPage,
	eM6DocStoreIndexLeafPage,
	eM6DocStoreIndexBranchPage,
};

struct M6DocStoreIndexEntry
{
	uint32	mDocNr;
	uint32	mDocPage;
	uint32	mDocSize;
};

BOOST_STATIC_ASSERT(sizeof(M6DocStoreIndexEntry) == 12);
BOOST_STATIC_ASSERT(sizeof(M6DocStoreIndexEntry[kM6DataPageIndexCount]) < kM6DataPageTextSize);

struct M6DocStorePageData
{
	uint8			mType;
	uint8			mFlags;
	uint16			mN;
	uint32			mLink;
	union
	{
		M6DocStoreIndexEntry
					mData[kM6DataPageIndexCount];
		uint8		mText[kM6DataPageTextSize];
	};
};

BOOST_STATIC_ASSERT(sizeof(M6DocStorePageData) == kM6DataPageSize);

struct M6DocStoreHdr
{
	uint32			mSignature;
	uint32			mDocCount;
	uint32			mIndexRoot;
	uint32			mNextDocNumber;
	uint32			mFirstDataPage;
	uint32			mLastDataPage;
	uint32			mFirstFreeDataPage;
	uint32			mAttributeOffset;
	int64			mRawTextSize;

	static const uint32
		kHeaderSize = 8 * sizeof(uint32) + 1 * sizeof(int64),
		kTextSize = kM6DataPageSize - kHeaderSize;

	uint8			mText[kTextSize];
};

BOOST_STATIC_ASSERT(sizeof(M6DocStoreHdr) == kM6DataPageSize);

// --------------------------------------------------------------------

class M6DocStorePage
{
  public:
					M6DocStorePage(M6DocStoreImpl& inStore, M6DocStorePageData* inData, uint32 inPageNr)
						: mStore(inStore), mData(inData), mPageNr(inPageNr), mDirty(false) {}
	virtual			~M6DocStorePage();

	void			SetPageType(uint8 inType)		{ mData->mType = inType; }
	uint8			GetPageType() const				{ return mData->mType; }

	void			SetLink(uint32 inPageNr)		{ mData->mLink = inPageNr; }
	uint32			GetLink() const					{ return mData->mLink; }

	void			SetDirty(bool inDirty)			{ mDirty = inDirty; }
	bool			IsDirty() const					{ return mDirty; }

	uint32			GetPageNr() const				{ return mPageNr; }

	void			Flush(M6File& inFile);

  protected:

	M6DocStoreImpl&			mStore;
	M6DocStorePageData*		mData;
	uint32					mPageNr;
	bool					mDirty;

  private:
					M6DocStorePage(const M6DocStorePage&);
	M6DocStorePage&	operator=(const M6DocStorePage&);
};

// --------------------------------------------------------------------

class M6DocStoreDataPage : public M6DocStorePage
{
  public:
					M6DocStoreDataPage(M6DocStoreImpl& inStore, M6DocStorePageData* inData, uint32 inPageNr);
	virtual			~M6DocStoreDataPage();

	uint32			Store(uint32 inDocNr, const uint8* inData, uint32 inSize);
	uint32			Load(uint32 inDocNr, uint8* outData, uint32 inSize);
	
  private:
	
	void			Write16(uint8*& ioPtr, uint16 inValue)
					{
						assert(ioPtr < mData->mText + kM6DataPageTextSize - 2);
						*ioPtr++ = static_cast<uint8>(inValue >>  8);
						*ioPtr++ = static_cast<uint8>(inValue >>  0);
					}

	void			Write32(uint8*& ioPtr, uint32 inValue)
					{
						assert(ioPtr < mData->mText + kM6DataPageTextSize - 4);
						*ioPtr++ = static_cast<uint8>(inValue >> 24);
						*ioPtr++ = static_cast<uint8>(inValue >> 16);
						*ioPtr++ = static_cast<uint8>(inValue >>  8);
						*ioPtr++ = static_cast<uint8>(inValue >>  0);
					}

	uint32			Read16(uint8*& ioPtr)
					{
						assert(ioPtr <= mData->mText + mData->mN - 2);

						uint32 result = *ioPtr++;
						result = result << 8 | *ioPtr++;
						
						return result;
					}

	uint32			Read32(uint8*& ioPtr)
					{
						assert(ioPtr <= mData->mText + mData->mN - 4);

						uint32 result = *ioPtr++;
						result = result << 8 | *ioPtr++;
						result = result << 8 | *ioPtr++;
						result = result << 8 | *ioPtr++;
						
						return result;
					}
};

// --------------------------------------------------------------------

class M6DocStoreIndexPage : public M6DocStorePage
{
  public:
					M6DocStoreIndexPage(M6DocStoreImpl& inStore, M6DocStorePageData* inData, uint32 inPageNr);
	virtual			~M6DocStoreIndexPage();

	uint32			GetKey(uint32 inIndex) const		{ return swap_bytes(mData->mData[inIndex].mDocNr); }
	uint32			GetDocPage(uint32 inIndex) const	{ return swap_bytes(mData->mData[inIndex].mDocPage); }
	uint32			GetDocSize(uint32 inIndex) const	{ return swap_bytes(mData->mData[inIndex].mDocSize); }

	void			SetKey(uint32 inIndex, uint32 inValue)		{ mData->mData[inIndex].mDocNr = swap_bytes(inValue); }
	void			SetDocPage(uint32 inIndex, uint32 inValue)	{ mData->mData[inIndex].mDocPage = swap_bytes(inValue); }
	void			SetDocSize(uint32 inIndex, uint32 inValue)	{ mData->mData[inIndex].mDocSize = swap_bytes(inValue); }

	bool			Insert(uint32& ioDocNr, uint32& ioPageNr, uint32 inDocSize);
	void			Erase(uint32 inDocNr);
	bool			Find(uint32 inDocNr, uint32& outPageNr, uint32& outDocSize);

	void			InsertValues(uint32 inDocNr, uint32 inPageNr, uint32 inDocSize, uint32 inIndex);
	static void		Move(M6DocStoreIndexPage& inSrc, M6DocStoreIndexPage& inDst,
						uint32 inSrcIndex, uint32 inDstIndex, uint32 inCount);

	void			Validate(uint32 inKey, M6DocStoreIndexPage* inParent);
	void			Dump(int inLevel = 0);

//	void			Underflow();
};

// --------------------------------------------------------------------

template<class T>
class M6DocStorePagePtr
{
  public:
	typedef T			M6DocStorePage;

						M6DocStorePagePtr();
						M6DocStorePagePtr(M6DocStoreImpl& inStoreImpl,
							M6DocStorePage* inPage);
						M6DocStorePagePtr(const M6DocStorePagePtr& inPtr);
						~M6DocStorePagePtr();

	M6DocStorePagePtr&	operator=(const M6DocStorePagePtr& inPtr);

//	M6DocStorePage*		release();
//	void				reset(M6DocStorePage* inPage);

	M6DocStorePage&		operator*() const				{ return *mPage; }
	M6DocStorePage*		operator->() const				{ return mPage; }
						operator bool() const			{ return mPage != nullptr; }

	void				swap(M6DocStorePagePtr& ptr)	{ std::swap(mPage, ptr.mPage); }

  private:
	M6DocStoreImpl*		mImpl;
	M6DocStorePage*		mPage;
};

typedef M6DocStorePagePtr<M6DocStoreDataPage>	M6DocStoreDataPagePtr;
typedef M6DocStorePagePtr<M6DocStoreIndexPage>	M6DocStoreIndexPagePtr;

// --------------------------------------------------------------------

class M6DocStoreImpl
{
  public:
					M6DocStoreImpl(const fs::path& inPath, MOpenMode inMode);
					~M6DocStoreImpl();

	uint32			Size() const					{ return mHeader.mDocCount; }
	uint32			NextDocumentNumber()			{ return mNextDocNumber++; }
	uint32			MaxDocumentNumber() const		{ return mNextDocNumber; }

	int64			GetRawSize() const				{ return mHeader.mRawTextSize; }
	int64			GetFileSize() const				{ return mFile.Size(); }

	void			StoreDocument(uint32 inDocNr, const char* inData, size_t inSize, size_t inRawSize);
	void			EraseDocument(uint32 inDocNr);
	bool			FetchDocument(uint32 inDocNr, uint32& outPageNr, uint32& outDocSize);
	void			OpenDataStream(uint32 inDocNr, uint32 inPageNr, uint32 inDocSize,
						io::filtering_stream<io::input>& ioStream);

	uint8			RegisterAttribute(const string& inName);
	string			GetAttributeName(uint8 inAttrNr) const;

	template<class T>
	M6DocStorePagePtr<T>	Allocate();

	template<class T>
	M6DocStorePagePtr<T>	Load(uint32 inPageNr);

	void			Reference(M6DocStorePage* inPage);
	void			Release(M6DocStorePage* inPage);

	void			Commit();
	void			Rollback();
	void			SetAutoCommit(bool inAutoCommit);

	void			Validate();
	void			Dump();

	friend struct Lock;
	struct Lock : public boost::unique_lock<boost::mutex>
	{
		Lock(M6DocStoreImpl* inImpl) : boost::unique_lock<boost::mutex>(inImpl->mMutex) {}
	};

  private:

	struct M6CachedPage;
	typedef M6CachedPage*	M6CachedPagePtr;
	
	struct M6CachedPage
	{
		uint32				mPageNr;
		uint32				mRefCount;
		M6DocStorePage*		mPage;
		M6CachedPagePtr		mNext;
		M6CachedPagePtr		mPrev;
	};

	M6File					mFile;
	MOpenMode				mMode;
	boost::mutex			mMutex;
	M6DocStoreHdr			mHeader;
	atomic<uint32>			mNextDocNumber;
	M6DocStoreIndexPagePtr	mRoot;
	bool					mDirty;
	bool					mAutoCommit;

	void			InitCache();
	M6CachedPagePtr	GetCachePage(uint32 inPageNr);
	
	M6CachedPagePtr	mCache,	mLRUHead, mLRUTail;
	uint32			mCacheCount;
};

// --------------------------------------------------------------------

template<class T>
M6DocStorePagePtr<T>::M6DocStorePagePtr()
	: mImpl(nullptr)
	, mPage(nullptr)
{
}

template<class T>
M6DocStorePagePtr<T>::M6DocStorePagePtr(
		M6DocStoreImpl& inStoreImpl, M6DocStorePage* inPage)
	: mImpl(&inStoreImpl)
	, mPage(inPage)
{
	if (mPage != nullptr)
		mImpl->Reference(mPage);
}

template<class T>
M6DocStorePagePtr<T>::M6DocStorePagePtr(const M6DocStorePagePtr& inPtr)
	: mImpl(inPtr.mImpl)
	, mPage(inPtr.mPage)
{
	if (mPage != nullptr)
		mImpl->Reference(mPage);
}

template<class T>
M6DocStorePagePtr<T>::~M6DocStorePagePtr()
{
	if (mPage != nullptr)
		mImpl->Release(mPage);	
}

template<class T>
M6DocStorePagePtr<T>& M6DocStorePagePtr<T>::operator=(const M6DocStorePagePtr& inPtr)
{
	if (this != &inPtr and mPage != inPtr.mPage)
	{
		if (mPage != nullptr)
			mImpl->Release(mPage);
		mImpl = inPtr.mImpl;
		mPage = inPtr.mPage;
		if (mPage != nullptr)
			mImpl->Reference(mPage);
	}
	
	return *this;
}

// --------------------------------------------------------------------

M6DocStorePage::~M6DocStorePage()
{
	delete mData;
}

void M6DocStorePage::Flush(M6File& inFile)
{
	if (mDirty)
	{
		inFile.PWrite(*mData, mPageNr * kM6DataPageSize);
		mDirty = false;
	}
}

// --------------------------------------------------------------------

M6DocStoreDataPage::M6DocStoreDataPage(M6DocStoreImpl& inStore, M6DocStorePageData* inData, uint32 inPageNr)
	: M6DocStorePage(inStore, inData, inPageNr)
{
}

M6DocStoreDataPage::~M6DocStoreDataPage()
{
}

uint32 M6DocStoreDataPage::Store(uint32 inDocNr, const uint8* inData, uint32 inSize)
{
	uint32 result = 0;
	uint32 free = kM6DataPageTextSize - mData->mN;
	
	if (free > kM6DataPageTextCutOff)
	{
		uint8* dst = mData->mText + mData->mN;
		Write32(dst, inDocNr);
		
		result = inSize;
		if (result > free - sizeof(uint32) - sizeof(uint16))
			result = free - sizeof(uint32) - sizeof(uint16);
		
		Write16(dst, static_cast<uint16>(result));
		
		memcpy(dst, inData, result);

		mData->mN += static_cast<uint16>(result + sizeof(uint32) + sizeof(uint16));
	}
	
	mDirty = true;
	
	return result;
}

uint32 M6DocStoreDataPage::Load(uint32 inDocNr, uint8* outData, uint32 inSize)
{
	// first search the document in mText
	uint32 docNr = 0;
	uint16 size = 0;
	uint8* src = mData->mText;

	while (src < mData->mText + mData->mN)
	{
		docNr = Read32(src);
		size = Read16(src);

		if (docNr == inDocNr)
			break;

		src += size;
	}
	
	if (docNr != inDocNr)
		THROW(("Document not found!"));
	
	memcpy(outData, src, size);
	
	return size;
}

// --------------------------------------------------------------------

M6DocStoreIndexPage::M6DocStoreIndexPage(M6DocStoreImpl& inStore, M6DocStorePageData* inData, uint32 inPageNr)
	: M6DocStorePage(inStore, inData, inPageNr)
{
}

M6DocStoreIndexPage::~M6DocStoreIndexPage()
{
}

void M6DocStoreIndexPage::InsertValues(uint32 inDocNr, uint32 inPageNr, uint32 inDocSize, uint32 inIndex)
{
	memmove(mData->mData + inIndex + 1, mData->mData + inIndex, sizeof(M6DocStoreIndexEntry) * (mData->mN - inIndex));
	SetKey(inIndex, inDocNr);
	SetDocPage(inIndex, inPageNr);
	SetDocSize(inIndex, inDocSize);
	++mData->mN;
	mDirty = true;
}

void M6DocStoreIndexPage::Move(M6DocStoreIndexPage& inSrc, M6DocStoreIndexPage& inDst,
	uint32 inSrcIndex, uint32 inDstIndex, uint32 inCount)
{
	M6DocStorePageData& src = *inSrc.mData;
	M6DocStorePageData& dst = *inDst.mData;

	if (inDstIndex < dst.mN)
		memmove(dst.mData + inDstIndex + inCount, dst.mData + inDstIndex, (dst.mN - inDstIndex) * sizeof(M6DocStoreIndexEntry));
	
	memcpy(dst.mData + inDstIndex, src.mData + inSrcIndex, inCount * sizeof(M6DocStoreIndexEntry));
	src.mN -= inCount;
	dst.mN += inCount;
	
	if (inSrcIndex + inCount < src.mN)
		memmove(src.mData + inSrcIndex, src.mData + inSrcIndex + inCount, (src.mN - inSrcIndex) * sizeof(M6DocStoreIndexEntry));

	inSrc.mDirty = true;
	inDst.mDirty = true;
}

bool M6DocStoreIndexPage::Insert(uint32& ioDocNr, uint32& ioPageNr, uint32 inDocSize)
{
	assert(mData->mType == eM6DocStoreIndexBranchPage or mData->mType == eM6DocStoreIndexLeafPage);

	bool result = false;
	
	int32 L = 0, R = mData->mN - 1;
	while (L <= R)
	{
		int32 i = (L + R) / 2;
		
		if (ioDocNr < GetKey(i))
			R = i - 1;
		else
			L = i + 1;
	}
	
	if (mData->mType == eM6DocStoreIndexLeafPage)
	{
		int32 ix = R + 1;
		
		if (GetKey(ix) == ioDocNr)
		{
			SetDocPage(ix, ioPageNr);
			SetDocSize(ix, inDocSize);
		}
		else if (mData->mN < kM6DataPageIndexCount)
			InsertValues(ioDocNr, ioPageNr, inDocSize, ix);
		else
		{
			M6DocStoreIndexPagePtr next(mStore.Allocate<M6DocStoreIndexPage>());
			next->mData->mType = eM6DocStoreIndexLeafPage;

			if (ix == mData->mN)	// since data is always added in order, optimise here
				next->InsertValues(ioDocNr, ioPageNr, inDocSize, 0);
			else
			{
				int32 split = mData->mN / 2;
			
				Move(*this, *next, split, 0, mData->mN - split);

				next->SetLink(mData->mLink);
				mData->mLink = next->GetPageNr();
			
				if (ix <= mData->mN)
					InsertValues(ioDocNr, ioPageNr, inDocSize, ix);
				else
					next->InsertValues(ioDocNr, ioPageNr, inDocSize, ix - mData->mN);
			}

			ioDocNr = next->GetKey(0);
			ioPageNr = next->GetPageNr();
			result = true;
		}

		mDirty = true;
	}
	else	// branch page
	{
		uint32 pageNr;

		if (R < 0)
			pageNr = mData->mLink;
		else
			pageNr = GetDocPage(R);
		
		M6DocStoreIndexPagePtr page(mStore.Load<M6DocStoreIndexPage>(pageNr));
		if (page->Insert(ioDocNr, ioPageNr, inDocSize))
		{
			int32 ix = R + 1;
			
			if (mData->mN + 1 < kM6DataPageIndexCount)
				InsertValues(ioDocNr, ioPageNr, inDocSize, ix);
			else
			{
				M6DocStoreIndexPagePtr next(mStore.Allocate<M6DocStoreIndexPage>());
				next->mData->mType = eM6DocStoreIndexBranchPage;

				int32 split = mData->mN / 2;
				
				uint32 upDocNr, downPageNr;
				
				if (ix == split)
				{
					upDocNr = ioDocNr;
					downPageNr = ioPageNr;

					Move(*this, *next, split, 0, mData->mN - split);
				}
				else
				{
					if (ix < split)
						--split;

					upDocNr = GetKey(split);
					downPageNr = GetDocPage(split);
					
					Move(*this, *next, split + 1, 0, mData->mN - split - 1);
					mData->mN -= 1;
					
					if (ioDocNr < next->GetKey(0))
						InsertValues(ioDocNr, ioPageNr, inDocSize, ix);
					else
						next->InsertValues(ioDocNr, ioPageNr, inDocSize, ix - split - 1);
				}
				
				next->SetLink(downPageNr);
				ioDocNr = upDocNr;
				ioPageNr = next->GetPageNr();
				
				result = true;
			}
			mDirty = true;
		}
	}
	
	return result;
}

void M6DocStoreIndexPage::Erase(uint32 inDocNr)
{
}

bool M6DocStoreIndexPage::Find(uint32 inDocNr, uint32& outPageNr, uint32& outDocSize)
{
	bool result = false;
	
	int32 L = 0, R = mData->mN - 1;
	while (L <= R)
	{
		int32 i = (L + R) / 2;
		
		if (inDocNr < GetKey(i))
			R = i - 1;
		else
			L = i + 1;
	}
	
	if (mData->mType == eM6DocStoreIndexLeafPage)
	{
		if (R >= 0 and GetKey(R) == inDocNr)
		{
			outPageNr = GetDocPage(R);
			outDocSize = GetDocSize(R);
			result = true;
		}
	}
	else	// branch page
	{
		uint32 pageNr;

		if (R < 0)
			pageNr = mData->mLink;
		else
			pageNr = GetDocPage(R);
		
		M6DocStoreIndexPagePtr page(mStore.Load<M6DocStoreIndexPage>(pageNr));
		result = page->Find(inDocNr, outPageNr, outDocSize);
	}
	
	return result;
}

void M6DocStoreIndexPage::Validate(uint32 inKey, M6DocStoreIndexPage* inParent)
{
	if (mData->mType == eM6DocStoreIndexLeafPage)
	{
//		M6VALID_ASSERT(mData->mN >= kM6MinEntriesPerPage or inParent == nullptr);
		//M6VALID_ASSERT(inParent == nullptr or not TooSmall());
		assert(inKey == 0 or GetKey(0) == inKey);
		
		for (uint32 i = 0; i < mData->mN; ++i)
		{
			if (i > 0)
				assert(GetKey(i) > GetKey(i - 1));
		}
		
		if (mData->mLink != 0)
		{
			M6DocStoreIndexPagePtr next(mStore.Load<M6DocStoreIndexPage>(mData->mLink));
			assert(GetKey(mData->mN - 1) < next->GetKey(0));
		}
	}
	else
	{
		assert(mData->mType == eM6DocStoreIndexBranchPage);
//		M6VALID_ASSERT(mData->mN >= kM6MinEntriesPerPage or inParent == nullptr);
		//M6VALID_ASSERT(inParent == nullptr or not TooSmall());
//		M6VALID_ASSERT(mData->mN <= kM6MaxEntriesPerPage);

		for (uint32 i = 0; i < mData->mN; ++i)
		{
			M6DocStoreIndexPagePtr link(mStore.Load<M6DocStoreIndexPage>(mData->mLink));
			link->Validate(inKey, this);
			
			for (uint32 i = 0; i < mData->mN; ++i)
			{
				M6DocStoreIndexPagePtr page(mStore.Load<M6DocStoreIndexPage>(GetDocPage(i)));
				page->Validate(GetKey(i), this);
				if (i > 0)
					assert(GetKey(i) > GetKey(i - 1));
			}
		}
	}
}

void M6DocStoreIndexPage::Dump(int inLevel)
{
	string prefix(inLevel * 2, ' ');

	if (mData->mType == eM6DocStoreIndexLeafPage)
	{
		cout << prefix << "leaf page at " << mPageNr << "; N = " << mData->mN << ": [";
		for (int i = 0; i < mData->mN; ++i)
			cout << GetKey(i) << '(' << GetDocPage(i) << ')'
				 << (i + 1 < mData->mN ? ", " : "");
		cout << "]" << endl;

		if (mData->mLink)
		{
			M6DocStoreIndexPagePtr next(mStore.Load<M6DocStoreIndexPage>(mData->mLink));
			cout << prefix << "  " << "link: " << next->GetKey(0) << endl;
		}
	}
	else if (mData->mType == eM6DocStoreIndexBranchPage)
	{
		cout << prefix << "branch" << " page at " << mPageNr << "; N = " << mData->mN << ": {";
		for (int i = 0; i < mData->mN; ++i)
			cout << GetKey(i) << (i + 1 < mData->mN ? ", " : "");
		cout << "}" << endl;

		M6DocStoreIndexPagePtr link(mStore.Load<M6DocStoreIndexPage>(mData->mLink));
		link->Dump(inLevel + 1);
		
		for (int i = 0; i < mData->mN; ++i)
		{
			cout << prefix << inLevel << '.' << i << ") " << GetKey(i) << endl;
			
			M6DocStoreIndexPagePtr sub(mStore.Load<M6DocStoreIndexPage>(GetDocPage(i)));
			sub->Dump(inLevel + 1);
		}
	}
	else
		cout << "Incorrect page type" << endl;
}

// --------------------------------------------------------------------

struct M6DocSource : public io::source
{
	typedef char			char_type;
	typedef io::source_tag	category;

					M6DocSource(M6DocStoreImpl& inStore, uint32 inDocNr,
						uint32 inPageNr, uint32 inDocSize);
					M6DocSource(const M6DocSource& inSource);
	M6DocSource&	operator=(const M6DocSource& inSource);

	streamsize		read(char* s, streamsize n);

	M6DocStoreImpl*	mStore;
	uint32			mDocNr, mPageNr, mDocSize;
	char			mBuffer[kM6DataPageTextSize];
	char*			mBufferStart;
	char*			mBufferEnd;
};

M6DocSource::M6DocSource(M6DocStoreImpl& inStore, 
	uint32 inDocNr, uint32 inPageNr, uint32 inDocSize)
	: mStore(&inStore)
	, mDocNr(inDocNr)
	, mPageNr(inPageNr)
	, mDocSize(inDocSize)
	, mBufferStart(mBuffer)
	, mBufferEnd(mBuffer)
{
}

M6DocSource::M6DocSource(const M6DocSource& inSource)
	: mStore(inSource.mStore)
	, mDocNr(inSource.mDocNr)
	, mPageNr(inSource.mPageNr)
	, mDocSize(inSource.mDocSize)
	, mBufferStart(mBuffer)
	, mBufferEnd(mBuffer)
{
	size_t n = inSource.mBufferEnd - inSource.mBufferStart;
	memcpy(mBuffer, inSource.mBufferStart, n);
	mBufferEnd = mBuffer + n;
}

M6DocSource& M6DocSource::operator=(const M6DocSource& inSource)
{
	if (this != &inSource)
	{
		mStore = inSource.mStore;
		mDocNr = inSource.mDocNr;
		mPageNr = inSource.mPageNr;
		mDocSize = inSource.mDocSize;
		
		size_t n = inSource.mBufferEnd - inSource.mBufferStart;
		memcpy(mBuffer, inSource.mBufferStart, n);

		mBufferStart = mBuffer;
		mBufferEnd = mBuffer + n;
	}
	
	return *this;
}

streamsize M6DocSource::read(char* s, streamsize n)
{
	streamsize result = 0;

	while (n > 0)
	{
		if (mBufferStart == mBufferEnd)
		{
			if (mDocSize == 0)
				break;
			
			M6DocStoreImpl::Lock lock(mStore);
			M6DocStoreDataPagePtr page(mStore->Load<M6DocStoreDataPage>(mPageNr));
			
			uint32 n = page->Load(mDocNr, reinterpret_cast<uint8*>(mBuffer), sizeof(mBuffer));
			mPageNr = page->GetLink();
			mDocSize -= n;
	
			mBufferStart = mBuffer;
			mBufferEnd = mBuffer + n;
		}
		
		streamsize k = mBufferEnd - mBufferStart;
		if (k > n)
			k = n;
		memcpy(s, mBufferStart, k);
		
		mBufferStart += k;
		s += k;
		n -= k;
		result += k;
	}
	
	if (result == 0 and mDocSize == 0)
		result = -1;

	return result;
}

// --------------------------------------------------------------------

M6DocStoreImpl::M6DocStoreImpl(const fs::path& inPath, MOpenMode inMode)
	: mFile(inPath, inMode)
	, mMode(inMode)
	, mNextDocNumber(1)
	, mDirty(false)
	, mAutoCommit(true)
{
	InitCache();

	if (inMode == eReadWrite and mFile.Size() == 0)
	{
		memset(&mHeader, 0, sizeof(mHeader));

		mHeader.mSignature = kM6DocStoreSignature;
		mHeader.mNextDocNumber = 1;
		mHeader.mAttributeOffset = M6DocStoreHdr::kTextSize;
		
		mFile.PWrite(mHeader, 0);
	}
	else
	{
		mFile.PRead(mHeader, 0);
		mRoot = Load<M6DocStoreIndexPage>(mHeader.mIndexRoot);
		
		mNextDocNumber = mHeader.mNextDocNumber;
	}

	assert(mHeader.mSignature == kM6DocStoreSignature);
//	assert(mHeader.mHeaderSize == sizeof(mHeader));
}

M6DocStoreImpl::~M6DocStoreImpl()
{
	if (mDirty)
	{
		mHeader.mNextDocNumber = mNextDocNumber;
		mFile.PWrite(mHeader, 0);
	}
	
	mRoot = M6DocStoreIndexPagePtr();

	for (uint32 ix = 0; ix < mCacheCount; ++ix)
		delete mCache[ix].mPage;
	delete[] mCache;
}

uint8 M6DocStoreImpl::RegisterAttribute(const string& inName)
{
	uint8 result = 0, n = 1;
	
	uint32 l = static_cast<uint32>(inName.length());
	if (l > 255)
		THROW(("Attribute name too long (%s)", inName.c_str()));
	
	uint8* attr = mHeader.mText + mHeader.mAttributeOffset;
	while (attr < mHeader.mText + M6DocStoreHdr::kTextSize)
	{
		if (l == *attr and memcmp(attr + 1, inName.c_str(), l) == 0)
		{
			result = n;
			break;
		}
		
		attr += *attr + 1;
		++n;
	}
	
	if (result == 0 and mMode == eReadWrite)
	{
		if (l + 1 > mHeader.mAttributeOffset)
			THROW(("No space left for new attribute (%s)", inName.c_str()));
		
		result = n;

		uint32 o = mHeader.mAttributeOffset;
		memmove(mHeader.mText + o - l - 1, mHeader.mText + o, M6DocStoreHdr::kTextSize - o);
		mHeader.mAttributeOffset -= l + 1;

		uint8* attr = mHeader.mText + M6DocStoreHdr::kTextSize - l - 1;
		*attr = static_cast<uint8>(inName.length());
		memcpy(attr + 1, inName.c_str(), inName.length());
	
		mDirty = true;
	}
	
	return result;
}

string M6DocStoreImpl::GetAttributeName(uint8 inAttrNr) const
{
	uint8 n = 1;
	string result;
	
	const uint8* attr = mHeader.mText + mHeader.mAttributeOffset;
	while (n < inAttrNr and attr < mHeader.mText + M6DocStoreHdr::kTextSize)
	{
		attr += *attr + 1;
		++n;
	}
	
	if (n == inAttrNr and attr < mHeader.mText + M6DocStoreHdr::kTextSize)
		result.assign(reinterpret_cast<const char*>(attr + 1), *attr);
	else
		THROW(("Attribute not registered: %d", inAttrNr));

	return result;
}

void M6DocStoreImpl::StoreDocument(uint32 inDocNr, const char* inData, size_t inSize, size_t inRawSize)
{
	if (inSize == 0 or inData == nullptr)
		THROW(("Empty document"));
	
	if (inSize > numeric_limits<uint32>::max())
		THROW(("Document too large"));

	if (inDocNr == 0 or inDocNr > mNextDocNumber)
		THROW(("Invalid document number"));

//cout << "store doc" << endl;

	const uint8* ptr = reinterpret_cast<const uint8*>(inData);
	uint32 size = static_cast<uint32>(inSize);
	uint32 pageNr = mHeader.mLastDataPage;

	M6DocStoreDataPagePtr dataPage;
	if (pageNr == 0)
	{
		dataPage = Allocate<M6DocStoreDataPage>();
		dataPage->SetPageType(eM6DocStoreDataPage);
		pageNr = dataPage->GetPageNr();
		mHeader.mLastDataPage = pageNr;
	}
	else
		dataPage = Load<M6DocStoreDataPage>(pageNr);

	uint32 docPageNr = pageNr, docSize = size;
	
	while (size > 0)
	{
		uint32 k = dataPage->Store(inDocNr, ptr, size);
		
		ptr += k;
		size -= k;
		
		if (size > 0)
		{
			M6DocStoreDataPagePtr next(Allocate<M6DocStoreDataPage>());
			next->SetPageType(eM6DocStoreDataPage);
			pageNr = next->GetPageNr();
			dataPage->SetLink(pageNr);
			mHeader.mLastDataPage = pageNr;
			dataPage = next;
			
			if (k == 0)	// can happen if last page was too full to start writing data
				docPageNr = pageNr;
		}
	}
	
	if (not mRoot)
	{
		if (mHeader.mIndexRoot == 0)
		{
			mRoot = Allocate<M6DocStoreIndexPage>();
			mRoot->SetPageType(eM6DocStoreIndexLeafPage);
			mHeader.mIndexRoot = mRoot->GetPageNr();
		}
		else
			mRoot = Load<M6DocStoreIndexPage>(mHeader.mIndexRoot);
	}
	
	if (mRoot->Insert(inDocNr, docPageNr, docSize))
	{
		M6DocStoreIndexPagePtr newRoot(Allocate<M6DocStoreIndexPage>());
		newRoot->SetPageType(eM6DocStoreIndexBranchPage);
		newRoot->SetLink(mHeader.mIndexRoot);
		newRoot->InsertValues(inDocNr, docPageNr, 0, 0);
		mHeader.mIndexRoot = newRoot->GetPageNr();
		
		mRoot = newRoot;
	}
	
	++mHeader.mDocCount;
	mHeader.mRawTextSize += inRawSize;

	mDirty = true;
}

void M6DocStoreImpl::EraseDocument(uint32 inDocNr)
{
	THROW(("unimplemented"));
}

bool M6DocStoreImpl::FetchDocument(uint32 inDocNr, uint32& outPageNr, uint32& outDocSize)
{
	if (not mRoot)
	{
		if (mHeader.mIndexRoot == 0)
		{
			mRoot = Allocate<M6DocStoreIndexPage>();
			mRoot->SetPageType(eM6DocStoreIndexLeafPage);
			mHeader.mIndexRoot = mRoot->GetPageNr();
		}
		else
			mRoot = Load<M6DocStoreIndexPage>(mHeader.mIndexRoot);
	}

	return mRoot->Find(inDocNr, outPageNr, outDocSize);
}

void M6DocStoreImpl::OpenDataStream(uint32 inDocNr,
	uint32 inPageNr, uint32 inDocSize, io::filtering_stream<io::input>& ioStream)
{
	ioStream.push(M6DocSource(*this, inDocNr, inPageNr, inDocSize));
}

template<class T>
M6DocStorePagePtr<T> M6DocStoreImpl::Allocate()
{
	int64 fileSize = mFile.Size();
	uint32 pageNr = static_cast<uint32>((fileSize - 1) / kM6DataPageSize + 1ULL);
	int64 offset = pageNr * kM6DataPageSize;
	mFile.Truncate(offset + kM6DataPageSize);
	
	M6DocStorePageData* data = new M6DocStorePageData;
	memset(data, 0, kM6DataPageSize);
	
	M6CachedPagePtr cp = GetCachePage(pageNr);
	cp->mPage = new T(*this, data, pageNr);
	
	return M6DocStorePagePtr<T>(*this, static_cast<T*>(cp->mPage));
}

template<class T>
M6DocStorePagePtr<T> M6DocStoreImpl::Load(uint32 inPageNr)
{
	if (inPageNr == 0)
		THROW(("Invalid page number"));

	M6CachedPagePtr cp = mLRUHead;
	while (cp != nullptr and cp->mPageNr != inPageNr)
		cp = cp->mNext; 

	if (cp == nullptr)
	{
		cp = GetCachePage(inPageNr);
		
		M6DocStorePageData* data = new M6DocStorePageData;
		mFile.PRead(*data, inPageNr * kM6DataPageSize);

		switch (data->mType)
		{
			case eM6DocStoreDataPage:
				cp->mPage = new M6DocStoreDataPage(*this, data, inPageNr);
				break;
			
			case eM6DocStoreIndexLeafPage:
			case eM6DocStoreIndexBranchPage:
				cp->mPage = new M6DocStoreIndexPage(*this, data, inPageNr);
				break;
			
			default:
                uint8 dataType = data->mType;
				delete data;
				THROW(("Invalid page type in document store (page = %d, type = %d)", inPageNr, dataType));
				break;
		}
	}
	
	return M6DocStorePagePtr<T>(*this, static_cast<T*>(cp->mPage));
}

void M6DocStoreImpl::InitCache()
{
	const uint32 kM6CacheCount = 32;
	
	mCacheCount = kM6CacheCount;
	mCache = new M6CachedPage[mCacheCount];
	
	for (uint32 ix = 0; ix < mCacheCount; ++ix)
	{
		mCache[ix].mPageNr = 0;
		mCache[ix].mRefCount = 0;
		mCache[ix].mPage = nullptr;
		mCache[ix].mNext = mCache + ix + 1;
		mCache[ix].mPrev = mCache + ix - 1;
	}
	
	mCache[0].mPrev = mCache[mCacheCount - 1].mNext = nullptr;
	mLRUHead = mCache;
	mLRUTail = mCache + mCacheCount - 1;
}

M6DocStoreImpl::M6CachedPagePtr M6DocStoreImpl::GetCachePage(uint32 inPageNr)
{
	M6CachedPagePtr result = mLRUTail;
	
	// now search backwards for a cached page that can be recycled
	uint32 n = 0;
	while (result != nullptr and result->mPage != nullptr and 
		(result->mRefCount > 0 or (result->mPage->IsDirty() and mAutoCommit == false)))
	{
		result = result->mPrev;
		++n;
	}
	
	if (result == nullptr)
		THROW(("cache full"));

//	// we could end up with a full cache, if so, double the cache
//	
//	if (result == nullptr)
//	{
//		mCacheCount *= 2;
//		M6CachedPagePtr tmp = new M6CachedPage[mCacheCount];
//		
//		for (uint32 ix = 0; ix < mCacheCount; ++ix)
//		{
//			tmp[ix].mPageNr = 0;
//			tmp[ix].mNext = tmp + ix + 1;
//			tmp[ix].mPrev = tmp + ix - 1;
//		}
//
//		tmp[0].mPrev = tmp[mCacheCount - 1].mNext = nullptr;
//
//		for (M6CachedPagePtr a = mLRUHead, b = tmp; a != nullptr; a = a->mNext, b = b->mNext)
//		{
//			b->mPageNr = a->mPageNr;
//			b->mPage = a->mPage;
//		}
//		
//		delete[] mCache;
//		mCache = tmp;
//		
//		mLRUHead = mCache;
//		mLRUTail = mCache + mCacheCount - 1;
//		
//		result = mLRUTail;
//	}

	if (result != mLRUHead)
	{
		if (result == mLRUTail)
			mLRUTail = result->mPrev;
		
		if (result->mPrev)
			result->mPrev->mNext = result->mNext;
		if (result->mNext)
			result->mNext->mPrev = result->mPrev;
		
		result->mPrev = nullptr;
		result->mNext = mLRUHead;
		mLRUHead->mPrev = result;
		mLRUHead = result;
	}
	
	if (result->mPage != nullptr and result->mPage->IsDirty())
		result->mPage->Flush(mFile);

	result->mPageNr = inPageNr;
	delete result->mPage;
	result->mPage = nullptr;
	result->mRefCount = 0;
//	result->mDirty = false;
	
	return result;
}

void M6DocStoreImpl::Reference(M6DocStorePage* inPage)
{
	if (inPage == nullptr)
		THROW(("Invalid page number"));

	M6CachedPagePtr cp = mLRUHead;
	while (cp != nullptr and cp->mPage != inPage)
		cp = cp->mNext; 

	if (cp == nullptr)
		THROW(("page not found in cache"));
	
	cp->mRefCount += 1;
}

void M6DocStoreImpl::Release(M6DocStorePage* inPage)
{
	if (inPage == nullptr)
		THROW(("Invalid page number"));

	M6CachedPagePtr cp = mLRUHead;
	while (cp != nullptr and cp->mPage != inPage)
		cp = cp->mNext; 

	if (cp == nullptr)
		THROW(("page not found in cache"));
	
	cp->mRefCount -= 1;
}

void M6DocStoreImpl::Commit()
{
	for (uint32 ix = 0; ix < mCacheCount; ++ix)
	{
		if (mCache[ix].mPage != nullptr and mCache[ix].mPage->IsDirty())
			mCache[ix].mPage->Flush(mFile);
	}
}

void M6DocStoreImpl::Rollback()
{
	for (uint32 ix = 0; ix < mCacheCount; ++ix)
	{
		if (mCache[ix].mPage and mCache[ix].mPage->IsDirty())
		{
			assert(mCache[ix].mRefCount == 0);

			mCache[ix].mPage->SetDirty(false);
			delete mCache[ix].mPage;
			mCache[ix].mPage = nullptr;
			mCache[ix].mPageNr = 0;
			mCache[ix].mRefCount = 0;
		}
	}
}

void M6DocStoreImpl::SetAutoCommit(bool inAutoCommit)
{
	mAutoCommit = inAutoCommit;
}

void M6DocStoreImpl::Validate()
{
	if (not mRoot)
	{
		if (mHeader.mIndexRoot == 0)
		{
			mRoot = Allocate<M6DocStoreIndexPage>();
			mRoot->SetPageType(eM6DocStoreIndexLeafPage);
			mHeader.mIndexRoot = mRoot->GetPageNr();
		}
		else
			mRoot = Load<M6DocStoreIndexPage>(mHeader.mIndexRoot);
	}

	mRoot->Validate(0, nullptr);
}

void M6DocStoreImpl::Dump()
{
	cout << endl << "Dumping tree" << endl << endl;

	if (not mRoot)
	{
		if (mHeader.mIndexRoot == 0)
		{
			mRoot = Allocate<M6DocStoreIndexPage>();
			mRoot->SetPageType(eM6DocStoreIndexLeafPage);
			mHeader.mIndexRoot = mRoot->GetPageNr();
		}
		else
			mRoot = Load<M6DocStoreIndexPage>(mHeader.mIndexRoot);
	}

	mRoot->Dump(0);

	cout << endl;
}

// --------------------------------------------------------------------

M6DocStore::M6DocStore(const fs::path& inPath, MOpenMode inMode)
	: mImpl(new M6DocStoreImpl(inPath, inMode))
{
}

M6DocStore::~M6DocStore()
{
//	M6DocStoreImpl::Lock lock(mImpl);
	delete mImpl;
}

void M6DocStore::GetInfo(uint32& outDocCount, int64& outFileSize, int64& outRawSize)
{
	M6DocStoreImpl::Lock lock(mImpl);
	outDocCount = mImpl->Size();
	outFileSize = mImpl->GetFileSize();
	outRawSize = mImpl->GetRawSize();
}	

uint8 M6DocStore::RegisterAttribute(const string& inName)
{
	M6DocStoreImpl::Lock lock(mImpl);
	return mImpl->RegisterAttribute(inName);
}

string M6DocStore::GetAttributeName(uint8 inAttrNr) const
{
	M6DocStoreImpl::Lock lock(mImpl);
	return mImpl->GetAttributeName(inAttrNr);
}

void M6DocStore::StoreDocument(uint32 inDocNr, const char* inData, size_t inSize, size_t inRawSize)
{
	M6DocStoreImpl::Lock lock(mImpl);
	mImpl->StoreDocument(inDocNr, inData, inSize, inRawSize);
}

void M6DocStore::EraseDocument(uint32 inDocNr)
{
	M6DocStoreImpl::Lock lock(mImpl);
	mImpl->EraseDocument(inDocNr);
}

bool M6DocStore::FetchDocument(uint32 inDocNr, uint32& outPageNr, uint32& outDocSize)
{
	M6DocStoreImpl::Lock lock(mImpl);
	return mImpl->FetchDocument(inDocNr, outPageNr, outDocSize);
}

void M6DocStore::OpenDataStream(uint32 inDocNr, uint32 inPageNr, uint32 inDocSize,
	io::filtering_stream<io::input>& ioStream)
{
	M6DocStoreImpl::Lock lock(mImpl);
	mImpl->OpenDataStream(inDocNr, inPageNr, inDocSize, ioStream);
}

uint32 M6DocStore::size() const
{
	M6DocStoreImpl::Lock lock(mImpl);
	return mImpl->Size();
}

void M6DocStore::Commit()
{
//	mImpl->Validate();
	M6DocStoreImpl::Lock lock(mImpl);
	mImpl->Commit();
}

uint32 M6DocStore::GetNextDocumentNumber()
{
	return mImpl->NextDocumentNumber();
}

uint32 M6DocStore::GetMaxDocNr()
{
	return mImpl->MaxDocumentNumber();
}

void M6DocStore::Validate()
{
	M6DocStoreImpl::Lock lock(mImpl);
	mImpl->Validate();
}

void M6DocStore::Dump()
{
	M6DocStoreImpl::Lock lock(mImpl);
	mImpl->Dump();
}

