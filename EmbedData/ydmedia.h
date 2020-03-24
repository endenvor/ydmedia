#pragma once

#ifndef _YDMEDIA_H
#define _YDMEDIA_H

#include <string>
#include <Wincrypt.h>

namespace YDMEDIA {

#ifndef BUFSIZE
#define BUFSIZE 8192
#endif

#define ACCESS_MODE_EXCLUSIVE		0x01			//��ҪУ��Ӳ��SN, ֻ�������״����еĻ���������
#define ACCESS_MODE_NEW					0x02			//�״�����

#define MARK "YUNDUN_MEDIA_INFO"					//md5					28c768c3947bd4804660f1c0075dbeff
	//123456				e10adc3949ba59abbe56e057f20f883e
	//NS89N671610709T1G		9ed905ef3e536a328a16640ccedaaa7e
#define CHAOS_SIZE 64											

	struct APPEND_INFO {
		unsigned char szMark[32];				//�ļ�ʶ����
		unsigned char szPwd[32];				//������֤md5
		unsigned char szPhySn[32];				//Ӳ��sn md5
		unsigned long nFileLength;		//��Ƶ�ļ�����
		unsigned long nSubProcLength;		//�ӽ����ļ�����
		unsigned char chaos[CHAOS_SIZE];		//��������, ���Ŵ������Ƿ������豸Ƕ������
	};

	extern volatile int g_progressBar;

	void SetPlayTimes(APPEND_INFO* pAi, WORD nTimes);

	WORD GetPlayTimes(APPEND_INFO* pAi);

	UCHAR GetAccessMode(APPEND_INFO* pAi);

	void  SetAccessMode(APPEND_INFO* pAi, UCHAR bExclusive);

	bool GetAppendInfo(const CString& szFileName, APPEND_INFO* pAi);

	std::string GetCryptKey(const CString& szPasswd, APPEND_INFO* ai);

	//�����������
	void FillJunkData(void* pData, int nBytes);

	bool EmbedFile(const CString& szDstFileName, const CString& szMediaFileName,
		const CString& szSubProcName, const CString& szPasswd, WORD nPlayTimes, BOOL bExclusive);

	bool GetCmdEcho(const wchar_t* szCmd, std::string& szEcho, bool bCmd);

	bool ParseDiskdriveSN(const std::string& szEcho, const char* szSearch, std::string& szSn);

	bool CStringToString(const CString& cstr, std::string& str);

	bool StringToCString(const std::string& str, CString& cstr);

	bool GetMD5(const BYTE* pData, size_t length, CString& strMD5);

	CString GetNewFileSuffix(const CString& szFileName, const CString& szNewSuffix);

	bool ExtractFile(const CString& szExeFileName, const CString& szMediaFileName, const CString& szSubProcName, const CString& szPasswd);

	void SelfDestroy(LPCWSTR lpTempFile);

	void PrintError(LPCWSTR msg);

	void ShowAppendInfo(APPEND_INFO* ai);

	void rc4_init(const char* key, long Len);

	void rc4_crypt(unsigned char* Data, long Len);

}//end of namespace


#endif	//end of head define



