#include <stdio.h>
#include <windows.h>
#include "Compiler.h"

bool GetProductAndVersion(char* strProductName, char* strProductVersion)
{
	// get the filename of the executable containing the version resource
	TCHAR szFilename[MAX_PATH + 1] = { 0 };
	if (GetModuleFileName(NULL, szFilename, MAX_PATH) == 0)
	{
		return false;
	}

	// allocate a block of memory for the version info
	DWORD dummy;
	DWORD dwSize = GetFileVersionInfoSize(szFilename, &dummy);
	if (dwSize == 0)
	{
		return false;
	}

	BYTE* data = new BYTE[dwSize];

	// load the version info
	if (!GetFileVersionInfo(szFilename, NULL, dwSize, data))
	{
		return false;
	}

	// get the name and version strings
	LPVOID pvProductName = NULL;
	unsigned int iProductNameLen = 0;
	LPVOID pvProductVersion = NULL;
	unsigned int iProductVersionLen = 0;

	// replace "040904e4" with the language ID of your resources
	if (!VerQueryValueA(&data[0], "\\StringFileInfo\\000904b0\\ProductName", &pvProductName, &iProductNameLen) ||
		!VerQueryValueA(&data[0], "\\StringFileInfo\\000904b0\\ProductVersion", &pvProductVersion, &iProductVersionLen))
	{
		return false;
	}


	strcpy_s(strProductName, 100, (LPCSTR)pvProductName);
	strcpy_s(strProductVersion, 100, (LPCSTR)pvProductVersion);

	return true;
}


int main(int argc, const char** argv)
{
	InitDeclarations();
	InitAssembler();

	if (argc > 0)
	{
		char	basePath[200], crtPath[200], includePath[200], targetPath[200];
		char	strProductName[100], strProductVersion[200];

		if (GetProductAndVersion(strProductName, strProductVersion))
		{
			printf("Starting %s %s\n", strProductName, strProductVersion);
		}

		DWORD length = ::GetModuleFileNameA(NULL, basePath, sizeof(basePath));

		while (length > 0 && basePath[length - 1] != '/' && basePath[length - 1] != '\\')
			length--;

#ifdef _DEBUG
		if (length > 0)
		{
			length--;
			while (length > 0 && basePath[length - 1] != '/' && basePath[length - 1] != '\\')
				length--;
		}
#endif

		basePath[length] = 0;

		Compiler* compiler = new Compiler();

		Location	loc;

		compiler->mPreprocessor->AddPath(basePath);
		strcpy_s(includePath, basePath);
		strcat_s(includePath, "include/");
		compiler->mPreprocessor->AddPath(includePath);
		strcpy_s(crtPath, includePath);
		strcat_s(crtPath, "crt.c");

		bool		emulate = false;

		targetPath[0] = 0;

		for (int i = 1; i < argc; i++)
		{
			const char* arg = argv[i];
			if (arg[0] == '-')
			{
				if (arg[1] == 'i' && arg[2] == '=')
				{
					compiler->mPreprocessor->AddPath(arg + 3);
				}
				else if (arg[1] == 'o' && arg[2] == '=')
				{
					strcpy_s(targetPath, arg + 3);
				}
				else if (arg[1] == 'r' && arg[2] == 't' && arg[3] == '=')
				{
					strcpy_s(crtPath, arg + 4);
				}
				else if (arg[1] == 'e')
				{
					emulate = true;
				}
				else
					compiler->mErrors->Error(loc, "Invalid command line argument", arg);
			}
			else
			{
				if (!targetPath[0])
					strcpy_s(targetPath, argv[i]);
				compiler->mCompilationUnits->AddUnit(loc, argv[i], nullptr);
			}
		}

		// Add runtime module

		compiler->mCompilationUnits->AddUnit(loc, crtPath, nullptr);

		if (compiler->ParseSource() && compiler->GenerateCode())
		{
			compiler->WriteOutputFile(targetPath);

			if (emulate)
				compiler->ExecuteCode();
		}

		if (compiler->mErrors->mErrorCount != 0)
			return 20;
	}
	else
	{
		printf("oscar64 {-i=includePath} [-o=output.prg] [-cr=runtime.c] [-e] {source.c}\n");
		return 0;
	}

	return 0;
}

