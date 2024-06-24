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
#include <time.h>

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
		int		dataFileInterleave = 10;

#ifdef _WIN32
			GetProductAndVersion(strProductName, strProductVersion);

			DWORD length = ::GetModuleFileNameA(NULL, basePath, sizeof(basePath));

#else
		strcpy(strProductName, "oscar64");
		strcpy(strProductVersion, "1.29.247");

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

		compiler->mCompilerOptions |= COPT_NATIVE;

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
		int			trace = 0;

		targetPath[0] = 0;
		diskPath[0] = 0;

		char	targetFormat[20];
		strcpy_s(targetFormat, "prg");

		char	targetMachine[20];
		strcpy_s(targetMachine, "c64");

		compiler->AddDefine(Ident::Unique("__OSCAR64C__"), "1");
		compiler->AddDefine(Ident::Unique("__STDC__"), "1");
		compiler->AddDefine(Ident::Unique("__STDC_VERSION__"), "199901L");

		bool	defining = false;

		for (int i = 1; i < argc; i++)
		{
			const char* arg = argv[i];
			if (defining)
			{
				defining = false;

				char	def[100];
				int i = 0;
				while (arg[i] && arg[i] != '=')
				{
					def[i] = arg[i];
					i++;
				}
				def[i] = 0;
				if (arg[i] == '=')
					compiler->AddDefine(Ident::Unique(def), _strdup(arg + i + 1));
				else
					compiler->AddDefine(Ident::Unique(def), "");
			}
			else if (arg[0] == '-')
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
				else if (arg[1] == 'f' && arg[2] == 'i' && arg[3] == '=')
				{
					dataFileInterleave = atoi(arg + 4);
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
				else if (arg[1] == 't' && arg[2] == 'm' && arg[3] == '=')
				{
					strcpy_s(targetMachine, arg + 4);
				}
				else if (arg[1] == 'c' && arg[2] == 'i' && arg[3] == 'd' && arg[4] == '=')
				{
					char	cid[10];
					strcpy_s(cid, arg + 5);
					compiler->mCartridgeID = atoi(cid);
				}
				else if (arg[1] == 'n')
				{
					compiler->mCompilerOptions |= COPT_NATIVE;
				}
				else if (arg[1] == 'b' && arg[2] == 'c')
				{
					compiler->mCompilerOptions &= ~COPT_NATIVE;
				}
				else if (arg[1] == 'p' && arg[2] == 's' && arg[3] == 'c' && arg[4] == 'i')
				{
					compiler->mCompilerOptions |= COPT_PETSCII;
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
					else if (arg[2] == 'a')
						compiler->mCompilerOptions |= COPT_OPTIMIZE_ASSEMBLER;
					else if (arg[2] == 'i')
						compiler->mCompilerOptions |= COPT_OPTIMIZE_AUTO_INLINE;
					else if (arg[2] == 'z')
						compiler->mCompilerOptions |= COPT_OPTIMIZE_AUTO_ZEROPAGE;
					else if (arg[2] == 'p')
						compiler->mCompilerOptions |= COPT_OPTIMIZE_CONST_PARAMS;
					else if (arg[2] == 'g')
						compiler->mCompilerOptions |= COPT_OPTIMIZE_GLOBAL;
					else if (arg[2] == 'm')
						compiler->mCompilerOptions |= COPT_OPTIMIZE_MERGE_CALLS;
				}
				else if (arg[1] == 'e')
				{
					emulate = true;
					if (arg[2] == 'p')
						profile = true;
					else if (arg[2] == 't')
						trace = 2;
					else if (arg[2] == 'b')
						trace = 1;
				}
				else if (arg[1] == 'D' && !arg[2])
				{
					defining = true;
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
				else if (arg[1] == 'g')
				{
					compiler->mCompilerOptions |= COPT_DEBUGINFO;
				}
				else if (arg[1] == 'v')
				{
					compiler->mCompilerOptions |= COPT_VERBOSE;
					if (arg[2] == '2')
						compiler->mCompilerOptions |= COPT_VERBOSE2;
					else if (arg[2] == '3')
						compiler->mCompilerOptions |= COPT_VERBOSE2 | COPT_VERBOSE3;
				}
				else if (arg[1] == 'x' && arg[2] == 'z')
				{
					compiler->mCompilerOptions |= COPT_EXTENDED_ZERO_PAGE;
				}
				else if (arg[1] == 'p' && arg[2] == 'p')
				{
					compiler->mCompilerOptions |= COPT_CPLUSPLUS;
					compiler->AddDefine(Ident::Unique("__cplusplus"), "1");
				}
				else
					compiler->mErrors->Error(loc, EERR_COMMAND_LINE, "Invalid command line argument", arg);
			}
			else
			{
				if (!targetPath[0])
					strcpy_s(targetPath, argv[i]);

				ptrdiff_t n = strlen(argv[i]);
				if (n > 4 && argv[i][n - 4] == '.' && argv[i][n - 3] == 'c' && argv[i][n - 2] == 'p' && argv[i][n - 1] == 'p')
				{
					compiler->mCompilerOptions |= COPT_CPLUSPLUS;
					compiler->AddDefine(Ident::Unique("__cplusplus"), "1");
				}

				compiler->mCompilationUnits->AddUnit(loc, argv[i], nullptr);
			}
		}

		if (compiler->mCompilerOptions & COPT_NATIVE)
		{
			compiler->AddDefine(Ident::Unique("OSCAR_NATIVE_ALL"), "1");
		}


		// REMOVE ME
		// compiler->mCompilerOptions |= COPT_OPTIMIZE_GLOBAL;
		// REMOVE ME

		char	basicStart[10];
		strcpy_s(basicStart, "0x0801");

		if (!strcmp(targetMachine, "c64"))
		{
			compiler->mTargetMachine = TMACH_C64;
			compiler->AddDefine(Ident::Unique("__C64__"), "1");
		}
		else if (!strcmp(targetMachine, "c128"))
		{
			strcpy_s(basicStart, "0x1c01");
			compiler->mTargetMachine = TMACH_C128;
			compiler->AddDefine(Ident::Unique("__C128__"), "1");
		}
		else if (!strcmp(targetMachine, "c128b"))
		{
			strcpy_s(basicStart, "0x1c01");
			compiler->mTargetMachine = TMACH_C128B;
			compiler->AddDefine(Ident::Unique("__C128B__"), "1");
		}
		else if (!strcmp(targetMachine, "c128e"))
		{
			strcpy_s(basicStart, "0x1c01");
			compiler->mTargetMachine = TMACH_C128E;
			compiler->AddDefine(Ident::Unique("__C128E__"), "1");
		}
		else if (!strcmp(targetMachine, "vic20"))
		{
			strcpy_s(basicStart, "0x1001");
			compiler->mTargetMachine = TMACH_VIC20;
			compiler->AddDefine(Ident::Unique("__VIC20__"), "1");
		}
		else if (!strcmp(targetMachine, "vic20+3"))
		{
			strcpy_s(basicStart, "0x0401");
			compiler->mTargetMachine = TMACH_VIC20_3K;
			compiler->AddDefine(Ident::Unique("__VIC20__"), "1");
		}
		else if (!strcmp(targetMachine, "vic20+8"))
		{
			strcpy_s(basicStart, "0x1201");
			compiler->mTargetMachine = TMACH_VIC20_8K;
			compiler->AddDefine(Ident::Unique("__VIC20__"), "1");
		}
		else if (!strcmp(targetMachine, "vic20+16"))
		{
			strcpy_s(basicStart, "0x1201");
			compiler->mTargetMachine = TMACH_VIC20_16K;
			compiler->AddDefine(Ident::Unique("__VIC20__"), "1");
		}
		else if (!strcmp(targetMachine, "vic20+24"))
		{
			strcpy_s(basicStart, "0x1201");
			compiler->mTargetMachine = TMACH_VIC20_24K;
			compiler->AddDefine(Ident::Unique("__VIC20__"), "1");
		}
		else if (!strcmp(targetMachine, "pet"))
		{
			strcpy_s(basicStart, "0x0401");
			compiler->mTargetMachine = TMACH_PET_8K;
			compiler->AddDefine(Ident::Unique("__CBMPET__"), "1");
		}
		else if (!strcmp(targetMachine, "pet16"))
		{
			strcpy_s(basicStart, "0x0401");
			compiler->mTargetMachine = TMACH_PET_16K;
			compiler->AddDefine(Ident::Unique("__CBMPET__"), "1");
		}
		else if (!strcmp(targetMachine, "pet32"))
		{
			strcpy_s(basicStart, "0x0401");
			compiler->mTargetMachine = TMACH_PET_32K;
			compiler->AddDefine(Ident::Unique("__CBMPET__"), "1");
		}
		else if (!strcmp(targetMachine, "plus4"))
		{
			strcpy_s(basicStart, "0x1001");
			compiler->mTargetMachine = TMACH_PLUS4;
			compiler->AddDefine(Ident::Unique("__PLUS4__"), "1");
		}
		else if (!strcmp(targetMachine, "x16"))
		{
			strcpy_s(basicStart, "0x0801");
			compiler->mTargetMachine = TMACH_X16;
			compiler->AddDefine(Ident::Unique("__X16__"), "1");
		}
		else if (!strcmp(targetMachine, "nes"))
		{
			compiler->mTargetMachine = TMACH_NES;
		}
		else if (!strcmp(targetMachine, "nes_nrom_h"))
		{
			compiler->mTargetMachine = TMACH_NES_NROM_H;
		}
		else if (!strcmp(targetMachine, "nes_nrom_v"))
		{
			compiler->mTargetMachine = TMACH_NES_NROM_V;
		}
		else if (!strcmp(targetMachine, "nes_mmc1"))
		{
			compiler->mTargetMachine = TMACH_NES_MMC1;
		}
		else if (!strcmp(targetMachine, "nes_mmc3"))
		{
			compiler->mTargetMachine = TMACH_NES_MMC3;
		}
		else if (!strcmp(targetMachine, "atari"))
		{
			compiler->mTargetMachine = TMACH_ATARI;
			compiler->AddDefine(Ident::Unique("__ATARI__"), "1");
		}
		else
			compiler->mErrors->Error(loc, EERR_COMMAND_LINE, "Invalid target machine option", targetMachine);


		if (compiler->mTargetMachine >= TMACH_NES && compiler->mTargetMachine <= TMACH_NES_MMC3)
		{
			compiler->mCompilerOptions |= COPT_TARGET_NES;
			compiler->mCompilerOptions |= COPT_EXTENDED_ZERO_PAGE;
			compiler->mCompilerOptions |= COPT_NATIVE;
			compiler->AddDefine(Ident::Unique("OSCAR_TARGET_NES"), "1");
			switch (compiler->mTargetMachine)
			{
			default:
			case TMACH_NES:
			case TMACH_NES_NROM_H:
			case TMACH_NES_NROM_V:
				break;
			case TMACH_NES_MMC1:
				compiler->AddDefine(Ident::Unique("__NES_MMC1__"), "1");
				break;
			case TMACH_NES_MMC3:
				compiler->AddDefine(Ident::Unique("__NES_MMC3__"), "1");
				break;
			}
			compiler->AddDefine(Ident::Unique("__NES__"), "1");
		}
		else if (!strcmp(targetFormat, "prg"))
		{
			compiler->mCompilerOptions |= COPT_TARGET_PRG;
			compiler->AddDefine(Ident::Unique("OSCAR_TARGET_PRG"), "1");
			compiler->AddDefine(Ident::Unique("OSCAR_BASIC_START"), basicStart);
		}
		else if (!strcmp(targetFormat, "crt"))
		{
			compiler->mCompilerOptions |= COPT_TARGET_CRT_EASYFLASH;
			compiler->mCartridgeID = 0x0020;

			compiler->AddDefine(Ident::Unique("OSCAR_TARGET_CRT_EASYFLASH"), "1");
		}
		else if (!strcmp(targetFormat, "crt16"))
		{
			compiler->mCompilerOptions |= COPT_TARGET_CRT16;
			compiler->AddDefine(Ident::Unique("OSCAR_TARGET_CRT16"), "1");
		}
		else if (!strcmp(targetFormat, "crt8"))
		{
			compiler->mCompilerOptions |= COPT_TARGET_CRT8;
			compiler->AddDefine(Ident::Unique("OSCAR_TARGET_CRT8"), "1");
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
			compiler->AddDefine(Ident::Unique("OSCAR_BASIC_START"), basicStart);
		}
		else
			compiler->mErrors->Error(loc, EERR_COMMAND_LINE, "Invalid target format option", targetFormat);

		if (compiler->mErrors->mErrorCount == 0)
		{
			strcpy_s(compiler->mVersion, strProductVersion);

			if (compiler->mCompilerOptions & COPT_VERBOSE)
			{
				printf("Starting %s %s\n", strProductName, strProductVersion);
			}

			{
				char dstring[100], tstring[100];
				time_t now = time(NULL);
				struct tm t;
#ifdef _WIN32
				localtime_s(&t, &now);
#else
				localtime_r(&now, &t);
#endif

				strftime(dstring, sizeof(tstring) - 1, "\"%b %d %Y\"", &t);
				strftime(tstring, sizeof(dstring) - 1, "\"%H:%M:%S\"", &t);

				compiler->AddDefine(Ident::Unique("__DATE__"), dstring);
				compiler->AddDefine(Ident::Unique("__TIME__"), tstring);
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
						if (!d64->WriteFile(dataFiles[i], dataFileCompressed[i], dataFileInterleave))
						{
							printf("Could not embed disk file %s\n", dataFiles[i]);
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
					compiler->ExecuteCode(profile, trace);
			}
		}

		if (compiler->mErrors->mErrorCount != 0)
			return 20;
	}
	else
	{
		printf("oscar64 {-i=includePath} [-o=output.prg] [-rt=runtime.c] [-tf=target] [-tm=machine] [-e] [-n] [-g] [-O(0|1|2|3)] [-pp] {-dSYMBOL[=value]} [-v] [-d64=diskname] {-f[z]=file.xxx} {source.c}\n");

		return 0;
	}

	return 0;
}


#ifdef WIN32
#ifndef _DEBUG
int seh_filter(unsigned int code, struct _EXCEPTION_POINTERS* info)
{
#ifdef _WIN64
	printf("oscar64 crashed. %08x %08llx", info->ExceptionRecord->ExceptionCode, (uint64)(info->ExceptionRecord->ExceptionAddress));
#else
	printf("oscar64 crashed. %08x %08x", info->ExceptionRecord->ExceptionCode, (uint32)(info->ExceptionRecord->ExceptionAddress));
#endif
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif
#endif

int main(int argc, const char** argv)
{
#if 1
#ifdef _WIN32
#ifndef __GNUC__
#ifndef _DEBUG
	__try 
	{
#endif
#endif
#endif
#endif
		return main2(argc, argv);

#if 1
#ifdef _WIN32
#ifndef __GNUC__
#ifndef _DEBUG
}
	__except (seh_filter(GetExceptionCode(), GetExceptionInformation()))
	{
		return 30;
	}
#endif
#endif
#endif
#endif
}
