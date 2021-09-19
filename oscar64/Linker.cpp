#include "Linker.h"

Linker::Linker(Errors* errors)
	: mErrors(errors), mSections(nullptr), mReferences(nullptr), mObjects(nullptr)
{

}

Linker::~Linker(void)
{

}

int Linker::AddSection(const Ident* section, int start, int size)
{
	return 0;
}

void Linker::AddSectionData(const Ident* section, int id, const uint8* data, int size)
{

}

uint8* Linker::AddSectionSpace(const Ident* section, int id, int size)
{
	return nullptr;
}

void Linker::AddReference(const LinkerReference& ref)
{

}
