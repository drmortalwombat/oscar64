#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include "Compiler.h"
#include "DiskImage.h"

#ifdef _WIN32
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
#endif

int main2(int argc, const char** argv)
{
	InitDeclarations();
	InitAssembler();

	if (argc > 1)
	{
		char	basePath[200], crtPath[200], includePath[200], targetPath[200], diskPath[200];
		char	strProductName[100], strProductVersion[200];

#ifdef _WIN32
			GetProductAndVersion(strProductName, strProductVersion);

			DWORD length = ::GetModuleFileNameA(NULL, basePath, sizeof(basePath));

#else
		strcpy(strProductName, "oscar64");
		strcpy(strProductVersion, "1.7.143");

#ifdef __APPLE__
		uint32_t length = sizeof(basePath);

		_NSGetExecutablePath(basePath, &length);
		length = strlen(basePath);
#else
		int length = readlink("/proc/self/exe", basePath, sizeof(basePath));

		//		strcpy(basePath, argv[0]);
		//		int length = strlen(basePath);
#endif
#endif
		while (length > 0 && basePath[length - 1] != '/' && basePath[length - 1] != '\\')
			length--;

		if (length > 0)
		{
			length--;
			while (length > 0 && basePath[length - 1] != '/' && basePath[length - 1] != '\\')
				length--;
		}

		basePath[length] = 0;

		Compiler* compiler = new Compiler();

		Location	loc;

		GrowingArray<const char*>	dataFiles(nullptr);
		GrowingArray<bool>			dataFileCompressed(false);

		compiler->mPreprocessor->AddPath(basePath);
		strcpy_s(includePath, basePath);
		strcat_s(includePath, "include/");
		compiler->mPreprocessor->AddPath(includePath);
		strcpy_s(crtPath, includePath);
		strcat_s(crtPath, "crt.c");

		bool		emulate = false, profile = false;

		targetPath[0] = 0;
		diskPath[0] = 0;

		char	targetFormat[20];
		strcpy_s(targetFormat, "prg");

		compiler->AddDefine(Ident::Unique("__OSCAR64C__"), "1");
		compiler->AddDefine(Ident::Unique("__STDC__"), "1");
		compiler->AddDefine(Ident::Unique("__STDC_VERSION__"), "199901L");

		for (int i = 1; i < argc; i++)
		{
			const char* arg = argv[i];
			if (arg[0] == '-')
			{
				if (arg[1] == 'i' && arg[2] == '=')
				{
					compiler->mPreprocessor->AddPath(arg + 3);
				}
				else if (arg[1] == 'f' && arg[2] == '=')
				{
					dataFiles.Push(arg + 3);
					dataFileCompressed.Push(false);
				}
				else if (arg[1] == 'f' && arg[2] == 'z' && arg[3] == '=')
				{
					dataFiles.Push(arg + 4);
					dataFileCompressed.Push(true);
				}
				else if (arg[1] == 'o' && arg[2] == '=')
				{
					strcpy_s(targetPath, arg + 3);
				}
				else if (arg[1] == 'r' && arg[2] == 't' && arg[3] == '=')
				{
					strcpy_s(crtPath, arg + 4);
				}
				else if (arg[1] == 'd' && arg[2] == '6' && arg[3] == '4' && arg[4] == '=')
				{
					strcpy_s(diskPath, arg + 5);
				}
				else if (arg[1] == 't' && arg[2] == 'f' && arg[3] == '=')
				{
					strcpy_s(targetFormat, arg + 4);
				}
				else if (arg[1] == 'n')
				{
					compiler->mCompilerOptions |= COPT_NATIVE;
					compiler->AddDefine(Ident::Unique("OSCAR_NATIVE_ALL"), "1");
				}
				else if (arg[1] == 'O')
				{
					if (arg[2] == '0')
						compiler->mCompilerOptions &= ~(COPT_OPTIMIZE_ALL);
					else if (arg[1] == '1' || arg[1] == 0)
						compiler->mCompilerOptions |= COPT_OPTIMIZE_DEFAULT;
					else if (arg[2] == '2')
						compiler->mCompilerOptions |= COPT_OPTIMIZE_SPEED;
					else if (arg[2] == '3')
						compiler->mCompilerOptions |= COPT_OPTIMIZE_ALL;
					else if (arg[2] == 's')
						compiler->mCompilerOptions |= COPT_OPTIMIZE_SIZE;
				}
				else if (arg[1] == 'e')
				{
					emulate = true;
					if (arg[2] == 'p')
						profile = true;
				}
				else if (arg[1] == 'd')
				{
					char	def[100];
					int i = 2;
					while (arg[i] && arg[i] != '=')
					{
						def[i - 2] = arg[i];
						i++;
					}
					def[i - 2] = 0;
					if (arg[i] == '=')
						compiler->AddDefine(Ident::Unique(def), _strdup(arg + i + 1));
					else
						compiler->AddDefine(Ident::Unique(def), "");
				}
				else if (arg[1] == 'v')
				{
					compiler->mCompilerOptions |= COPT_VERBOSE;
					if (arg[2] == '2')
						compiler->mCompilerOptions |= COPT_VERBOSE2;
				}
				else
					compiler->mErrors->Error(loc, EERR_COMMAND_LINE, "Invalid command line argument", arg);
			}
			else
			{
				if (!targetPath[0])
					strcpy_s(targetPath, argv[i]);
				compiler->mCompilationUnits->AddUnit(loc, argv[i], nullptr);
			}
		}

		if (!strcmp(targetFormat, "prg"))
		{
			compiler->mCompilerOptions |= COPT_TARGET_PRG;
			compiler->AddDefine(Ident::Unique("OSCAR_TARGET_PRG"), "1");
		}
		else if (!strcmp(targetFormat, "crt"))
		{
			compiler->mCompilerOptions |= COPT_TARGET_CRT16;
			compiler->AddDefine(Ident::Unique("OSCAR_TARGET_CRT16"), "1");
		}
		else if (!strcmp(targetFormat, "bin"))
		{
			compiler->mCompilerOptions |= COPT_TARGET_BIN;
			compiler->AddDefine(Ident::Unique("OSCAR_TARGET_BIN"), "1");
		}
		else if (!strcmp(targetFormat, "lzo"))
		{
			compiler->mCompilerOptions |= COPT_TARGET_LZO;
			compiler->AddDefine(Ident::Unique("OSCAR_TARGET_LZO"), "1");
		}
		else
			compiler->mErrors->Error(loc, EERR_COMMAND_LINE, "Invalid target format option", targetFormat);

		if (compiler->mErrors->mErrorCount == 0)
		{
			if (compiler->mCompilerOptions & COPT_VERBOSE)
			{
				printf("Starting %s %s\n", strProductName, strProductVersion);
			}

			// Add runtime module

			if (crtPath[0])
				compiler->mCompilationUnits->AddUnit(loc, crtPath, nullptr);

			if (compiler->mCompilerOptions & COPT_TARGET_LZO)
			{
				compiler->BuildLZO(targetPath);
			}
			else if (compiler->ParseSource() && compiler->GenerateCode())
			{
				DiskImage* d64 = nullptr;

				if (diskPath[0])
					d64 = new DiskImage(diskPath);

				compiler->WriteOutputFile(targetPath, d64);

				if (d64)
				{
					for (int i = 0; i < dataFiles.Size(); i++)
					{
						if (!d64->WriteFile(dataFiles[i], dataFileCompressed[i]))
						{
							printf("Could not embedd disk file %s\n", dataFiles[i]);
							return 20;
						}
					}
					if (!d64->WriteImage(diskPath))
					{
						printf("Could not write disk image %s\n", diskPath);
						return 20;
					}
				}

				if (emulate)
					compiler->ExecuteCode(profile);
			}
		}

		if (compiler->mErrors->mErrorCount != 0)
			return 20;
	}
	else
	{
		printf("oscar64 {-i=includePath} [-o=output.prg] [-rt=runtime.c] [-tf=target] [-e] [-n] {-dSYMBOL[=value]} [-v] [-d64=diskname] {-f[z]=file.xxx} {source.c}\n");

		return 0;
	}

	return 0;
}


int main(int argc, const char** argv)
{
#ifdef _WIN32
#ifndef _DEBUG
	__try 
	{
#endif
#endif

		return main2(argc, argv);

#ifdef _WIN32
#ifndef _DEBUG
}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		printf("oscar64 crashed.");
		return 30;
	}
#endif
#endif
}
