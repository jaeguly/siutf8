#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include "utf8.h"

void utf8_to_mb(const char* u8,char* mb,DWORD* mbsize)
{
    int ret;
	if(u8 == NULL)
	{
		return;
	}
	//u8->unicode
	size_t size = MultiByteToWideChar(CP_UTF8,0,u8,-1,NULL,0);
    if (size == 0)
        OutputDebugStringEx("<%s>#%d Failed[%d]",__FUNCTION__,__LINE__,GetLastError());

	wchar_t* wansi = (wchar_t *)malloc((size)*sizeof(wchar_t));
	memset(wansi,0,(size)*sizeof(wchar_t));
	ret = MultiByteToWideChar(CP_UTF8,0,u8,-1,wansi,size);
    if (!ret)
        OutputDebugStringEx("<%s>#%d Failed[%d]",__FUNCTION__,__LINE__,GetLastError());
		
	//unicode->ascii
	size = WideCharToMultiByte(CP_ACP,0,wansi,-1,NULL,0,NULL,NULL);
    if (size == 0)
        OutputDebugStringEx("<%s>#%d Failed[%d]",__FUNCTION__,__LINE__,GetLastError());

	char* ansi = (char *)malloc(size);
	memset(ansi,0,size);
	ret = WideCharToMultiByte(CP_ACP,0,wansi,-1,ansi,size,NULL,NULL);
    if (!ret)
        OutputDebugStringEx("<%s>#%d Failed[%d]",__FUNCTION__,__LINE__,GetLastError());

	memcpy(mb,ansi,size);
	*mbsize = size;
	
	//clear	
	free(wansi);
	free(ansi);	
}

void mb_to_utf8(const char* mb,char* u8,DWORD* u8size)
{
    int ret;
	if(mb == NULL)
	{
		return;
	}

	//mb->unicode
	size_t size = MultiByteToWideChar(CP_ACP,0,mb,-1,NULL,0);
    if (size == 0)
        OutputDebugStringEx("<%s>#%d Failed[%d]",__FUNCTION__,__LINE__,GetLastError());

	wchar_t* wansi = (wchar_t *)malloc((size)*sizeof(wchar_t));
	memset(wansi,0,(size)*sizeof(wchar_t));
	ret = MultiByteToWideChar(CP_ACP,0,mb,-1,wansi,size);
    if (!ret)
        OutputDebugStringEx("<%s>#%d Failed[%d]",__FUNCTION__,__LINE__,GetLastError());
	
	//unicode->ascii
	size = WideCharToMultiByte(CP_UTF8,0,wansi,-1,NULL,0,NULL,NULL);
    if (size == 0)
        OutputDebugStringEx("<%s>#%d Failed[%d]",__FUNCTION__,__LINE__,GetLastError());

	char* ansi = (char *)malloc(size);
	memset(ansi,0,size);
	ret = WideCharToMultiByte(CP_UTF8,0,wansi,-1,ansi,size,NULL,NULL);
    if (!ret)
        OutputDebugStringEx("<%s>#%d Failed[%d]",__FUNCTION__,__LINE__,GetLastError());
	
	memcpy(u8,ansi,size);
	*u8size = size;

	//clear
	free(wansi);
	free(ansi);
}

/**
 * 检查bit位是否为真
 * @param  value [待检测值]
 * @param  bit   [1~8位 低~高]
 * @return       [1 真 0 假]
 */
static inline int CheckBit(unsigned char value, int bit)
{
	unsigned char bitvalue[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

	if((bit >= 1)&&(bit <= 8))
	{
		 if(value & bitvalue[bit-1])
		 	return(1);
		 else
		 	return(0);
	}
	else
	{
		printf("FILE: %s LINE: %d -- 传入的函数参数错误! bit=[%d]\n",
			__FILE__, __LINE__, bit);
		return(-1);
	}
}

//2 utf8 dom
//1 utf8
//0 not utf8
int IsUtf8(char* buf,int size)
{
	int i;
	int u8 = 0;
	int u8len = 0;
	int firstbyte = 0;
	const char BOM[3] = {0xef,0xbb,0xbf};

	if(size == 0)
	{
		return 0;
	}
	if(size >= 3 && strncmp(buf,BOM,3) == 0)
	{
		return 2;
	}

	for(i=0;i<size;i++)
	{
		//should be binrary file.
		if(buf[i] == 0)
		{
			return 0;
		}


		if(firstbyte == 0)
		{
			//pure ascii
			if((buf[i]&0xff) <= 127)
			{
				u8 = 0;
				u8len = 0;
			}
			//out of u8 range
			else if((buf[i]&0xff) > 0xef)
			{
				u8 = 0;
				u8len = 0;
			}
			else
			{
				firstbyte = 1;
				//u8 3b 1110xxxx 10xxxxxx 10xxxxxx
				if(CheckBit(buf[i],7) == 1 && CheckBit(buf[i],6) == 1 && CheckBit(buf[i],5) == 0)
				{
					u8 = 1;
					u8len++;
				}
				else
				{
					i++;
					firstbyte = 0;
					u8 = 0;
					u8len = 0;
				}
			}
		}
		else
		{
			if((buf[i]&0xff) <= 127 || (buf[i]&0xff) > 0xbf)
			{
				firstbyte = 0;
				
				if(u8 == 1 && u8len % 3 != 0)
				{
					u8 = 0;
				}
				else if(u8 == 1 && u8len % 3 == 0)
				{
					//so we mark it utf8
					break;
				}
			}
			else
			{
				u8len++;
			}
		}
	}

	//check tail loop
	if(u8 == 1 && u8len % 3 != 0)
	{
		u8 = 0;
	}

	return u8;
}

