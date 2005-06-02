#include "Common.h"
#include "MFFileSystem_Internal.h"
#include "FileSystem/MFFileSystemNative.h"

void MFFileSystemNative_InitModule()
{
	MFFileSystemCallbacks fsCallbacks;

	fsCallbacks.RegisterFS = MFFileSystemNative_Register;
	fsCallbacks.UnregisterFS = MFFileSystemNative_Unregister;
	fsCallbacks.FSMount = MFFileSystemNative_Mount;
	fsCallbacks.FSDismount = MFFileSystemNative_Dismount;
	fsCallbacks.FSOpen = MFFileSystemNative_Open;
	fsCallbacks.Open = MFFileNative_Open;
	fsCallbacks.Close = MFFileNative_Close;
	fsCallbacks.Read = MFFileNative_Read;
	fsCallbacks.Write = MFFileNative_Write;
	fsCallbacks.Seek = MFFileNative_Seek;
	fsCallbacks.Tell = MFFileNative_Tell;
	fsCallbacks.Query = MFFileNative_Query;
	fsCallbacks.GetSize = MFFileNative_GetSize;

	hNativeFileSystem = MFFileSystem_RegisterFileSystem(&fsCallbacks);
}

void MFFileSystemNative_DeinitModule()
{
	MFFileSystem_UnregisterFileSystem(hNativeFileSystem);
}

int MFFileSystemNative_Dismount(MFMount *pMount)
{
	MFFileSystem_ReleaseToc(pMount->pEntries, pMount->numFiles);

	return 0;
}

MFFile* MFFileSystemNative_Open(MFMount *pMount, const char *pFilename, uint32 openFlags)
{
	MFFileHandle hFile = NULL;

	// recurse toc
	MFTOCEntry *pTOCEntry = MFFileSystem_GetTocEntry(pFilename, pMount->pEntries, pMount->numFiles);

	if(pTOCEntry)
	{
		MFOpenDataNative openData;

		openData.cbSize = sizeof(MFOpenDataNative);
		openData.openFlags = openFlags;
		openData.pFilename = STR("%s%s", (char*)pTOCEntry->pFilesysData, pTOCEntry->pName);

		hFile = MFFile_Open(hNativeFileSystem, &openData);
	}

	return hFile;
}