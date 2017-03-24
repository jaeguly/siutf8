#include "winapihook.h"
#include "hook/hookapi.h"
#include "utils.h"
#include "utf8.h"
#include "sifilemgr.h"
#include "sihandlemgr.h"
#include "md5.h"
#include "TCHAR.h"


typedef HANDLE (WINAPI *CreateFileFn)(
	LPCTSTR lpFileName,
	DWORD dwDesiredAccess,
	DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile
);

typedef BOOL (WINAPI *CloseHandleFn)(
	HANDLE hObject
);

typedef BOOL (WINAPI *SetEndOfFileFn)(
	HANDLE hFile
);


CreateFileFn OrgCreateFile = NULL;
CloseHandleFn OrgCloseHandle = NULL;
SetEndOfFileFn OrgSetEndOfFile = NULL;


static BOOL IsSourceInsightFile(LPCTSTR lpFileName)
{
	return _tcsstr(lpFileName, _T("Source Insight")) ? TRUE : FALSE;
}

HANDLE WINAPI HookCreateFile(
	LPCTSTR lpFileName,
	DWORD dwDesiredAccess,
	DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile
)
{
	HANDLE handle;
	int u8flag = 0;
	char hookfilename[512];
	unsigned char fmd5[16];
	struct SiFileInfo* si_file_info = NULL;
	unsigned long hash;

    if (IsSourceInsightFile(lpFileName))
    {
        OutputDebugStringEx("Function:%s %s skip!",__FUNCTION__,lpFileName);
        goto RECOVER;
    }

	hash = HashString(lpFileName);

	memset(hookfilename,0,sizeof(hookfilename));
	strcpy(hookfilename,lpFileName);
	si_file_info = FindSiFileFromLink(hash);
	if(si_file_info == NULL)
	{
		HANDLE hFile = OrgCreateFile(lpFileName,
								GENERIC_READ,
    							FILE_SHARE_READ,
    							NULL,
    							OPEN_EXISTING,
    							FILE_ATTRIBUTE_NORMAL,
    							NULL);
    	if(hFile == INVALID_HANDLE_VALUE)
    	{
    		OutputDebugStringEx("Function :%s OrgCreateFile1 %s Failed[%d]",__FUNCTION__,lpFileName,GetLastError());
    		goto RECOVER;
    	}
    	DWORD fread;
    	DWORD fsize = GetFileSize(hFile,NULL);
    	char* buffer = (char*)malloc(fsize+1);
    	memset(buffer,0,fsize+1);
    	ReadFile(hFile,buffer,fsize,&fread,NULL);
    	OrgCloseHandle(hFile);
    	u8flag = IsUtf8(buffer,fsize);

    	//pure ascii
    	if(u8flag == 3)
    	{
            u8flag = 1;
    	}

    	//convert
    	if(u8flag != 0)
    	{
    		//OutputDebugStringEx("[%d]%s",u8flag,lpFileName);
    		DWORD mbsize = 0;
    		DWORD mbwriten;
    		char* mb = (char *)malloc(fsize+1);
			if(u8flag == 1)
    			utf8_to_mb(buffer,mb,&mbsize);
    		else if(u8flag == 2)
    			utf8_to_mb(buffer+3,mb,&mbsize);
    		//sprintf(hookfilename,"%s.mb",lpFileName);
    		GetTmpFilename(hash,hookfilename);
    		HANDLE hMb = OrgCreateFile(hookfilename,
								GENERIC_WRITE,
    							0,
    							NULL,
    							CREATE_ALWAYS,
    							FILE_ATTRIBUTE_NORMAL,
    							NULL);
    		if(hMb != INVALID_HANDLE_VALUE)
    		{
    			WriteFile(hMb,mb,mbsize-1,&mbwriten,NULL);
    			OrgCloseHandle(hMb);
    		}
    		else
    		{
    			OutputDebugStringEx("CreateFile %s Failed![Error=%ld]",hookfilename,GetLastError());
    		}
    		free(mb);
    	}
    	//calc md5sum only u8
    	if(u8flag != 0)
    	{
    		memset(fmd5,0,sizeof(fmd5));
    		Md5Sum((unsigned char *)buffer,fsize,fmd5);
    	}

    	free(buffer);
		SiFile_Add(hash,u8flag,fmd5,(char *)lpFileName,hookfilename);
	}
	else
	{
		u8flag = si_file_info->u8flag;
		//judge outside change
		if(u8flag != 0)
		{
			if(dwDesiredAccess == GENERIC_READ)
			{
				//read file
				HANDLE hFile = OrgCreateFile(lpFileName,
								GENERIC_READ,
    							FILE_SHARE_READ,
    							NULL,
    							OPEN_EXISTING,
    							FILE_ATTRIBUTE_NORMAL,
    							NULL);
		    	if(hFile == INVALID_HANDLE_VALUE)
		    	{
		    		OutputDebugStringEx("Function :%s OrgCreateFile2 %s Failed[%d]",__FUNCTION__,lpFileName,GetLastError());
		    		goto RECOVER;
		    	}
		    	DWORD fread;
		    	DWORD fsize = GetFileSize(hFile,NULL);
		    	char* buffer = (char*)malloc(fsize+1);
		    	memset(buffer,0,fsize+1);
		    	ReadFile(hFile,buffer,fsize,&fread,NULL);
		    	OrgCloseHandle(hFile);

		    	//calc md5sum
		    	memset(fmd5,0,sizeof(fmd5));
				Md5Sum((unsigned char *)buffer,fsize,fmd5);
				if(memcmp(fmd5,si_file_info->orgmd5,16) != 0)
				{
					OutputDebugStringEx("u8[%s] Changed outside!",lpFileName);
					//convert
					DWORD mbsize = 0;
		    		DWORD mbwriten;
		    		char* mb = (char *)malloc(fsize+1);
					if(u8flag == 1)
		    			utf8_to_mb(buffer,mb,&mbsize);
		    		else if(u8flag == 2)
		    			utf8_to_mb(buffer+3,mb,&mbsize);
		    		//sprintf(hookfilename,"%s.mb",lpFileName);
		    		GetTmpFilename(hash,hookfilename);
		    		HANDLE hMb = OrgCreateFile(hookfilename,
										GENERIC_WRITE,
		    							0,
		    							NULL,
		    							CREATE_ALWAYS,
		    							FILE_ATTRIBUTE_NORMAL,
		    							NULL);
		    		if(hMb != INVALID_HANDLE_VALUE)
		    		{
		    			WriteFile(hMb,mb,mbsize-1,&mbwriten,NULL);
		    			OrgCloseHandle(hMb);
		    		}
		    		else
		    		{
		    			OutputDebugStringEx("CreateFile %s Failed![Error=%ld]",hookfilename,GetLastError());
		    		}
		    		free(mb);

		    		//update hash
		    		memcpy(si_file_info->orgmd5,fmd5,16);
				}

				free(buffer);
			}
		}
		strcpy(hookfilename,si_file_info->mbfile);
	}

RECOVER:
	handle = OrgCreateFile(hookfilename,dwDesiredAccess,dwShareMode,lpSecurityAttributes,
									dwCreationDisposition,dwFlagsAndAttributes,hTemplateFile);
	if(u8flag != 0)
	{
		SiHandle_Add(handle,u8flag,(char *)lpFileName,hookfilename);
	}

	return handle;
}



BOOL WINAPI HookCloseHandle(
	HANDLE hObject
)
{
	BOOL rtv;

	SiHandle_Del(hObject);
	rtv = OrgCloseHandle(hObject);

	return rtv;
}

BOOL WINAPI HookSetEndOfFile(
	HANDLE hFile
)
{
	BOOL rtv;

	rtv = OrgSetEndOfFile(hFile);

	struct SiHandleInfo* si_handle_info = NULL;
	si_handle_info = FindSiHandleFromLink(hFile);
	if(si_handle_info != NULL)
	{
		//读文件
		DWORD fread;
		DWORD fsize = SetFilePointer(hFile,0,NULL,FILE_CURRENT);
		char* mb = (char *)malloc(fsize+1);
		memset(mb,0,fsize+1);
		SetFilePointer(hFile,0,NULL,FILE_BEGIN);
		ReadFile(hFile,mb,fsize,&fread,NULL);
		SetFilePointer(hFile,fsize,NULL,FILE_BEGIN);

    	//转成utf8
    	DWORD utf8size = 0;
    	DWORD utf8writen;
    	char* utf8 = (char *)malloc(2*fsize+3);
    	memset(utf8,0,2*fsize+3);
    	if(si_handle_info->u8flag == 1)
    		mb_to_utf8(mb,utf8,&utf8size);
    	else if(si_handle_info->u8flag == 2)
    	{
    		mb_to_utf8(mb,utf8+3,&utf8size);
    		utf8[0] = 0xef;
    		utf8[1] = 0xbb;
    		utf8[2] = 0xbf;
    		utf8size += 3;
    	}
    	else
    	{
    		OutputDebugStringEx("Function :%s Error HandleInfo!",__FUNCTION__);
    	}

    	//写回utf8
    	HANDLE hUtf8 = OrgCreateFile(si_handle_info->orgfile,
								GENERIC_WRITE,
    							0,
    							NULL,
    							CREATE_ALWAYS,
    							FILE_ATTRIBUTE_NORMAL,
    							NULL);
		if(hUtf8 != INVALID_HANDLE_VALUE)
		{
			WriteFile(hUtf8,utf8,utf8size-1,&utf8writen,NULL);
			OrgCloseHandle(hUtf8);
		}
		else
		{
			OutputDebugStringEx("CreateFile %s Failed![Error=%ld]",si_handle_info->orgfile,GetLastError());
		}

		//update md5
		unsigned long hash = HashString(si_handle_info->orgfile);
		struct SiFileInfo* si_file_info = FindSiFileFromLink(hash);
		unsigned char fmd5[16];
		memset(fmd5,0,sizeof(fmd5));
		Md5Sum((unsigned char *)utf8,utf8size-1,fmd5);
		memcpy(si_file_info->orgmd5,fmd5,16);

		free(utf8);
		free(mb);
	}

	return rtv;
}


BOOL HookWinApi(void)
{
	OrgCreateFile = (CreateFileFn)HookFunction("kernel32.dll","CreateFileA",(void *)HookCreateFile);
	if(OrgCreateFile == NULL)
	{
		OutputDebugString("Hook CreateFile Failed!");
		return FALSE;
	}

	OrgCloseHandle = (CloseHandleFn)HookFunction("kernel32.dll","CloseHandle",(void *)HookCloseHandle);
	if(OrgCloseHandle == NULL)
	{
		OutputDebugString("Hook CloseHandle Failed!");
		return FALSE;
	}

	OrgSetEndOfFile = (SetEndOfFileFn)HookFunction("kernel32.dll","SetEndOfFile",(void *)HookSetEndOfFile);
	if(OrgSetEndOfFile == NULL)
	{
		OutputDebugString("Hook SetEndOfFile Failed!");
		return FALSE;
	}

	return TRUE;
}
