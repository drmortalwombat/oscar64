#pragma once

#include "NativeCodeGenerator.h"
#include "Array.h"

class NativeCodeMapper
{
public:
	NativeCodeMapper(void);
	~NativeCodeMapper(void);

	void Reset(void);

	int MapInstruction(const NativeCodeInstruction& ins, LinkerSection * ls);
	int MapBasicBlock(NativeCodeBasicBlock* block);

	ExpandingArray<NativeCodeInstruction>	mIns;
	ExpandingArray<NativeCodeBasicBlock*>	mBlocks;
protected:
	static const int HashSize = 256;

	struct InsNode
	{
		int						mIndex;
		LinkerSection		*	mSection;
		InsNode* mNext;
	};

	InsNode							*		mHash[HashSize];
};

struct SuffixSegment
{
	NativeCodeBasicBlock	*	mBlock;
	int							mStart, mEnd;
};

class SuffixTree
{
public:
	const int	*	mSeg;
	int				mSize;

	static const int HashSize = 32;

	SuffixTree* mNext, * mParent, ** mFirst;

	SuffixTree(const int* str, int s, SuffixTree* n);
	~SuffixTree(void);

	void AddParents(SuffixTree* parent);

	void AddSuffix(const int* str, int s);
	void AddString(const int* str);

	void Print(FILE* file, NativeCodeMapper & map, int depth);
	void ParentPrint(FILE* file, NativeCodeMapper& map);
	int LongestMatch(NativeCodeMapper& map, int size, int isize, int & msize, SuffixTree *& mtree);
	void CollectSuffix(NativeCodeMapper& map, int offset, ExpandingArray<SuffixSegment>& segs);
	int ParentCodeSize(NativeCodeMapper& map) const;
	void ParentCollect(NativeCodeMapper& map, NativeCodeBasicBlock * block);
	void ReplaceCalls(NativeCodeMapper& map, ExpandingArray<SuffixSegment> & segs);
	void ChildReplaceCalls(NativeCodeMapper& map, SuffixTree * tree, int offset, ExpandingArray<SuffixSegment>& segs);
	void ParentReplaceCalls(NativeCodeMapper& map, NativeCodeBasicBlock* block, int offset, int size, ExpandingArray<SuffixSegment>& segs);

};