#include "NativeCodeOutliner.h"


NativeCodeMapper::NativeCodeMapper(void)
{
	for (int i = 0; i < HashSize; i++)
		mHash[i] = nullptr;
}

NativeCodeMapper::~NativeCodeMapper(void)
{
	for (int i = 0; i < HashSize; i++)
	{
		InsNode* n = mHash[i];
		while (n)
		{
			InsNode* m = n;
			n = n->mNext;
			delete m;
		}
		mHash[i] = nullptr;
	}

}

void NativeCodeMapper::Reset(void)
{
	mBlocks.SetSize(0);
}

int NativeCodeMapper::MapBasicBlock(NativeCodeBasicBlock* block)
{
	mBlocks.Push(block);
	return -mBlocks.Size();
}

int NativeCodeMapper::MapInstruction(const NativeCodeInstruction& ins, LinkerSection* ls)
{
	int hash = ins.CodeHash() % HashSize;
	InsNode* n = mHash[hash];
	while (n)
	{
		if (mIns[n->mIndex].CodeSame(ins) && n->mSection == ls)
			return n->mIndex;
		n = n->mNext;
	}
	n = new InsNode();
	n->mIndex = mIns.Size();
	n->mSection = ls;
	mIns.Push(ins);
	n->mNext = mHash[hash];
	mHash[hash] = n;
	return n->mIndex;
}

SuffixTree::SuffixTree(const int* str, int s, SuffixTree* n)
{
	mSeg = str;
	mSize = s;
	mNext = n;
	mParent = nullptr;
	mFirst = nullptr;
}

SuffixTree::~SuffixTree(void)
{
	delete[] mFirst;
}

void SuffixTree::AddParents(SuffixTree* parent)
{
	mParent = parent;
	if (mFirst)
	{
		for (int i = 0; i < HashSize; i++)
		{
			SuffixTree* n = mFirst[i];
			while (n)
			{
				n->AddParents(this);
				n = n->mNext;
			}
		}
	}
}

void SuffixTree::AddSuffix(const int* str, int s)
{
	int hi = str[0] & (HashSize - 1);
	SuffixTree* c = mFirst ? mFirst[hi] : nullptr;
	while (c && c->mSeg[0] != str[0])
		c = c->mNext;

	if (c)
	{
		int k = 1;
		while (k < c->mSize && str[k] == c->mSeg[k])
			k++;
		if (k == c->mSize)
			c->AddSuffix(str + k, s - k);
		else
		{
			SuffixTree * n = new SuffixTree(c->mSeg + k, c->mSize - k, nullptr);
			if (c->mFirst)
			{
				n->mFirst = new SuffixTree * [HashSize];
				for (int i = 0; i < HashSize; i++)
				{
					n->mFirst[i] = c->mFirst[i];
					c->mFirst[i] = nullptr;
				}
			}
			else
			{
				c->mFirst = new SuffixTree * [HashSize];
				for (int i = 0; i < HashSize; i++)
					c->mFirst[i] = nullptr;
			}
			c->mFirst[c->mSeg[k] & (HashSize - 1)] = n;
			int hk = str[k] & (HashSize - 1);
			c->mFirst[hk] = new SuffixTree(str + k, s - k, c->mFirst[hk]);
			c->mSize = k;
		}
	}
	else
	{
		if (!mFirst)
		{
			mFirst = new SuffixTree * [HashSize];
			for (int i = 0; i < HashSize; i++)
				mFirst[i] = nullptr;
		}

		mFirst[hi] = new SuffixTree(str, s, mFirst[hi]);
	}
}

void SuffixTree::AddString(const int* str)
{
	int s = 0;
	while(str[s] >= 0)
		s++;
	s++;

	int i = 0;
	while (str[i] >= 0)
	{
		AddSuffix(str + i, s - i);
		i++;
	}
	AddSuffix(str + i, 1);
}

void SuffixTree::CollectSuffix(NativeCodeMapper& map, int offset, ExpandingArray<SuffixSegment>& segs)
{
	offset += mSize;
	if (mFirst)
	{
		for (int i = 0; i < HashSize; i++)
		{
			SuffixTree* t = mFirst[i];

			while (t)
			{
				t->CollectSuffix(map, offset, segs);
				t = t->mNext;
			}
		}
	}
	else
	{
		NativeCodeBasicBlock* block = map.mBlocks[-(mSeg[mSize - 1] + 1)];

		SuffixSegment	seg;
		seg.mBlock = block;
		seg.mStart = offset;
		seg.mEnd = block->mIns.Size() + 1;
		segs.Push(seg);
	}
}

int SuffixTree::LongestMatch(NativeCodeMapper& map, int size, int isize, int& msize, SuffixTree*& mtree)
{
	if (mFirst)
	{
		isize += mSize;

		for (int i = 0; i < mSize; i++)
		{
			if (mSeg[i] >= 0)
				size += AsmInsModeSize[map.mIns[mSeg[i]].mMode];
		}

		assert(size < 10000);

		int cnt = 0;
		for (int i = 0; i < HashSize; i++)
		{
			SuffixTree* t = mFirst[i];
			while (t)
			{
				cnt += t->LongestMatch(map, size, isize, msize, mtree);
				t = t->mNext;
			}
		}

		if (size >= 6 && (size - 3) * (cnt - 1) > msize)
		{
			// Second run to cross check for overlaps
			ExpandingArray<SuffixSegment>	segs;
			for (int i = 0; i < HashSize; i++)
			{
				SuffixTree* t = mFirst[i];
				while (t)
				{
					t->CollectSuffix(map, 0, segs);
					t = t->mNext;
				}
			}
			segs.Sort([](const SuffixSegment& l, const SuffixSegment& r)->bool {
				return l.mBlock == r.mBlock ? l.mStart < r.mStart : ptrdiff_t(l.mBlock) < ptrdiff_t(r.mBlock);
			});

			for (int i = 0; i + 1 < segs.Size(); i++)
			{
				if (segs[i].mBlock == segs[i + 1].mBlock && segs[i].mStart + isize > segs[i + 1].mStart)
					cnt--;
			}

			if (cnt > 1 && (size - 3) * (cnt - 1) > msize)
			{
				msize = (size - 3) * (cnt - 1);
				mtree = this;
			}
		}

		return cnt;
	}
	else
		return 1;
}

void SuffixTree::Print(FILE * file, NativeCodeMapper& map, int depth)
{
	for (int i = 0; i < depth; i++)
		fprintf(file, ".");

	for (int i = 0; i < mSize; i++)
	{
		fprintf(file, "[");

		if (mSeg[i] >= 0)
			map.mIns[mSeg[i]].Disassemble(file);
		else
		{
			NativeCodeBasicBlock* block = map.mBlocks[- (mSeg[i] + 1)];
			fprintf(file, "%s,%d", block->mProc->mInterProc->mIdent->mString, block->mIndex);
		}
		fprintf(file, "]");
	}
	fprintf(file, "\n");

	if (mFirst)
	{
		for (int i = 0; i < HashSize; i++)
		{
			SuffixTree* n = mFirst[i];
			while (n)
			{
				n->Print(file, map, depth + 1);
				n = n->mNext;
			}
		}
	}
}

void SuffixTree::ParentPrint(FILE* file, NativeCodeMapper& map)
{
	if (mParent)
		mParent->ParentPrint(file, map);
	for (int i = 0; i < mSize; i++)
	{
		if (mSeg[i] >= 0)
			map.mIns[mSeg[i]].Disassemble(file);
		else
		{
			NativeCodeBasicBlock* block = map.mBlocks[-(mSeg[i] + 1)];
			fprintf(file, "%s,%d", block->mProc->mInterProc->mIdent->mString, block->mIndex);
		}
		fprintf(file, "\n");
	}
}

int SuffixTree::ParentCodeSize(NativeCodeMapper& map) const
{
	int size = 0;
	for (int i = 0; i < mSize; i++)
	{
		if (mSeg[i] >= 0)
			size += AsmInsModeSize[map.mIns[mSeg[i]].mMode];
	}

	if (mParent)
		size += mParent->ParentCodeSize(map);

	return size;
}

void SuffixTree::ParentCollect(NativeCodeMapper& map, NativeCodeBasicBlock* block)
{
	if (mParent)
		mParent->ParentCollect(map, block);
	for (int i = 0; i < mSize; i++)
		block->mIns.Push(map.mIns[mSeg[i]]);
}

void SuffixTree::ReplaceCalls(NativeCodeMapper& map, ExpandingArray<SuffixSegment>& segs)
{
	if (mFirst)
	{
		for (int i = 0; i < HashSize; i++)
		{
			SuffixTree* n = mFirst[i];
			while (n)
			{
				n->ChildReplaceCalls(map, this, 0, segs);
				n = n->mNext;
			}
		}
	}
}

void SuffixTree::ChildReplaceCalls(NativeCodeMapper& map, SuffixTree* tree, int offset, ExpandingArray<SuffixSegment>& segs)
{
	for (int i = 0; i < mSize; i++)
	{
		if (mSeg[i] >= 0)
			offset ++;
	}

	if (mFirst)
	{
		for (int i = 0; i < HashSize; i++)
		{
			SuffixTree* n = mFirst[i];
			while (n)
			{
				n->ChildReplaceCalls(map, tree, offset, segs);
				n = n->mNext;
			}
		}
	}
	else
	{
		NativeCodeBasicBlock* block = map.mBlocks[-(mSeg[mSize - 1] + 1)];
		tree->ParentReplaceCalls(map, block, offset, 0, segs);
	}
}

void SuffixTree::ParentReplaceCalls(NativeCodeMapper& map, NativeCodeBasicBlock* block, int offset, int size, ExpandingArray<SuffixSegment>& segs)
{
	for (int i = 0; i < mSize; i++)
	{
		if (mSeg[i] >= 0)
			size ++;
	}
	if (mParent)
		mParent->ParentReplaceCalls(map, block, offset, size, segs);
	else
	{
		int at = block->mIns.Size() - offset - size;

		SuffixSegment	seg;
		seg.mBlock = block;
		seg.mStart = at;
		seg.mEnd = at + size;
		segs.Push(seg);
	}
}
