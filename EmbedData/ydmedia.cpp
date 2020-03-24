
#include "pch.h"
#include "ydmedia.h"


namespace YDMEDIA {

	volatile int g_progressBar = 0;

	//��ȡʣ�ಥ�Ŵ���
	WORD GetPlayTimes(APPEND_INFO* pAi)
	{
		WORD retVal = 0;
		unsigned int pos = pAi->nFileLength % CHAOS_SIZE;
		unsigned char nBytes = pAi->chaos[pos];
		switch (nBytes)
		{
		case 0: break;
		case 1:
		{
			retVal = pAi->chaos[(pos + 16) % CHAOS_SIZE];
			break;
		}
		case 2:
		{
			retVal = (pAi->chaos[(pos + 16) % CHAOS_SIZE] << 8) + pAi->chaos[(pos + 17) % CHAOS_SIZE];
			break;
		}
		default: break;
		}
		return retVal;
	}

	//���ļ���С����chaos�����С���࣬�������㲥�Ŵ�����ռ�õ�λ����֮��ƫ��16λ�������ֵ
	//���Ŵ������ռ2���ֽڣ���PlayTimes < 65535
	void SetPlayTimes(APPEND_INFO* pAi, WORD nTimes)
	{
		unsigned int pos = pAi->nFileLength % CHAOS_SIZE;
		unsigned char val = pAi->chaos[pos];
		if (0 == nTimes)
		{
			pAi->chaos[pos] = 0;
		}
		else if (nTimes < 256)
		{
			pAi->chaos[pos] = 1;
			pAi->chaos[(pos + 16) % CHAOS_SIZE] = (UCHAR)nTimes;
		}
		else
		{
			pAi->chaos[pos] = 2;
			pAi->chaos[(pos + 16) % CHAOS_SIZE] = nTimes >> 8;
			pAi->chaos[(pos + 17) % CHAOS_SIZE] = (UCHAR)nTimes;
		}
	}


	UCHAR GetAccessMode(APPEND_INFO* pAi)
	{
		unsigned int pos = pAi->nFileLength % CHAOS_SIZE;
		return pAi->chaos[(pos + 1) % CHAOS_SIZE];
	}


	void  SetAccessMode(APPEND_INFO* pAi, UCHAR bExclusive)
	{
		unsigned int pos = pAi->nFileLength % CHAOS_SIZE;
		pAi->chaos[(pos + 1) % CHAOS_SIZE] = bExclusive;
	}


	bool GetAppendInfo(const CString& szFileName, APPEND_INFO* pAi)
	{
		CFile file;
		int nAiSize = sizeof(APPEND_INFO);
		ULONGLONG nFileSize = 0;
		if (!file.Open(szFileName, CFile::modeRead | CFile::typeBinary))
		{
			OutputDebugString(L"Open file failed");
			return false;
		}
		nFileSize = file.GetLength();
		if (nFileSize <= sizeof(APPEND_INFO))
		{
			OutputDebugString(L"Invalid file length");
			file.Close();
			return false;
		}
		ULONGLONG pos = file.Seek(-1 * (LONGLONG)nAiSize, CFile::end);

		if (file.Read(pAi, nAiSize) != nAiSize)
		{
			OutputDebugString(L"Get APPEND_INFO failed");
			file.Close();
			return false;
		}

		CString csMark;
		std::string szMark = MARK;
		GetMD5((PBYTE)szMark.c_str(), szMark.size(), csMark);
		CStringToString(csMark, szMark);
		if (::memcmp(szMark.c_str(), pAi->szMark, 32))
		{
			OutputDebugString(L"Get mark failed");
			file.Close();
			return false;
		}
		file.Close();
		return true;
	}


	//��szPasswd, TEXT(MARK), (UINT)nMediaFileLengthƴ�Ӻ�, md5�Ľ�����ڼ���key
	std::string GetCryptKey(const CString& szPasswd, APPEND_INFO* ai)
	{
		CString csEncryptKey;
		CString csEncryptMd5;
		std::string szEncryptMd5;
		csEncryptKey.Format(L"%s%s%ul", szPasswd.GetString(), TEXT(MARK), ai->nFileLength);
		OutputDebugString(csEncryptKey);
		CStringToString(csEncryptKey, szEncryptMd5);
		GetMD5((PBYTE)szEncryptMd5.c_str(), szEncryptMd5.size(), csEncryptMd5);
		CStringToString(csEncryptMd5, szEncryptMd5);
		return szEncryptMd5;
	}


	//�����������
	void FillJunkData(void* pData, int nBytes)
	{
		srand((unsigned int)time(NULL));
		PBYTE bytes = (PBYTE)pData;
		for (int i = 0; i < nBytes; i++)
		{
			bytes[i] = rand() % 256;
		}
	}


	/**@brief ��һ���ļ�����Ƕ����һ���ļ�ĩβ, ����Ӹ���AppendInfo����
	* @param[in]  szDstFileName             Ƕ�����ݵ�Ŀ���ļ�����
	* @param[in]  szSrcFileName             Ƕ�����ݵ�Դ�ļ�
	* @param[in]  pAppendInfo               Ƕ��ĸ���AppendInfo����
	* @return  ����ִ�н��
	* - true			�ɹ�
	* - false        ʧ��
	*/
	bool EmbedFile(const CString& szDstFileName, const CString& szMediaFileName,
		const CString& szSubProcName, const CString& szPasswd, WORD nPlayTimes, BOOL bExclusive)
	{
		CFile fileDst, fileMedia, fileSubProc, fileOut;
		ULONGLONG nMediaFileLength = 0;
		ULONGLONG nSubProcLength = 0;
		PBYTE lpBuf = NULL;
		UINT nReadLength = 0;
		CString szOutputFile = L"";
		APPEND_INFO ai = { 0 };
		std::string szPwd;

		szOutputFile = GetNewFileSuffix(szMediaFileName, L"exe");
		if (szOutputFile.IsEmpty())	return false;

		if (!fileOut.Open(szOutputFile, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary | CFile::shareDenyWrite))
		{
			return false;
		}

		if (!fileDst.Open(szDstFileName, CFile::modeRead | CFile::typeBinary | CFile::shareDenyWrite))
		{
			return false;
		}

		if (!fileMedia.Open(szMediaFileName, CFile::modeRead | CFile::typeBinary | CFile::shareDenyWrite))
		{
			return false;
		}

		if (!fileSubProc.Open(szSubProcName, CFile::modeRead | CFile::typeBinary | CFile::shareDenyWrite))
		{
			return false;
		}

		if (fileDst.GetLength() == 0)
		{
			return false;
		}

		if ((nMediaFileLength = fileMedia.GetLength()) == 0)
		{
			return false;
		}

		if ((nSubProcLength = fileSubProc.GetLength()) == 0)
		{
			return false;
		}

		lpBuf = (PBYTE)HeapAlloc(GetProcessHeap(), 0, BUFSIZE);
		if (!lpBuf)
		{
			return false;
		}

		//����dst�ļ�
		for (nReadLength = fileDst.Read(lpBuf, BUFSIZE);
			nReadLength > 0;
			nReadLength = fileDst.Read(lpBuf, BUFSIZE))
		{
			fileOut.Write(lpBuf, nReadLength);
		}

		//���ai
		//���Mark MD5
		CString csMark;
		std::string szMark = MARK;
		GetMD5((PBYTE)szMark.c_str(), szMark.size(), csMark);
		CStringToString(csMark, szMark);
		memcpy_s(ai.szMark, 32, szMark.c_str(), 32);

		//�������MD5
		CStringToString(szPasswd, szPwd);
		CString csTemp;
		GetMD5((BYTE*)szPwd.c_str(), szPwd.size(), csTemp);
		CStringToString(csTemp, szPwd);
		memcpy_s(ai.szPwd, 32, szPwd.c_str(), 32);

		ai.nFileLength = (UINT)nMediaFileLength;
		ai.nSubProcLength = (UINT)nSubProcLength;
		FillJunkData(ai.szPhySn, 32);
		FillJunkData(ai.chaos, CHAOS_SIZE);
		SetPlayTimes(&ai, nPlayTimes);
		SetAccessMode(&ai, (UCHAR)bExclusive | ACCESS_MODE_NEW);
		std::string szEncryptKey = GetCryptKey(szPasswd, &ai);

		//��media�ļ����ܺ�д������ļ�
		UINT nReadSizeSofar = 0;
		g_progressBar = 0;
		rc4_init(szEncryptKey.c_str(), szEncryptKey.size());
		for (nReadLength = fileMedia.Read(lpBuf, BUFSIZE);
			nReadLength > 0;
			nReadLength = fileMedia.Read(lpBuf, BUFSIZE))
		{
			rc4_crypt((PUCHAR)lpBuf, nReadLength);
			fileOut.Write(lpBuf, nReadLength);
			nReadSizeSofar += nReadLength;
			g_progressBar = (int)(100.0 * nReadSizeSofar / nMediaFileLength);
		}

		//����subproc�ļ�
		for (nReadLength = fileSubProc.Read(lpBuf, BUFSIZE);
			nReadLength > 0;
			nReadLength = fileSubProc.Read(lpBuf, BUFSIZE))
		{
			fileOut.Write(lpBuf, nReadLength);
		}

		//д�븽������
		fileOut.Write(&ai, sizeof(APPEND_INFO));
		//�ͷ��ڴ�
		if (lpBuf)
		{
			HeapFree(GetProcessHeap(), 0, lpBuf);
		}
		fileDst.Close();
		fileMedia.Close();
		fileSubProc.Close();
		fileOut.Close();
		return true;
	}


	bool GetCmdEcho(const wchar_t* szCmd, std::string& szEcho, bool bCmd)
	{
		szEcho = "";
		wchar_t lpCmd[BUFSIZ] = { 0 };
		char* lpBuffer = NULL;
		//char lpBuffer[BUFSIZE+1] = {0};
		DWORD dwReadByte = 0;
		DWORD dwTotalBytesAvail = 0;
		HANDLE hReadPipe = NULL;
		HANDLE hWritePipe = NULL;
		SECURITY_ATTRIBUTES sa = { 0 };
		PROCESS_INFORMATION pi = { 0 };
		STARTUPINFO si = { 0 };
		bool ret = false;
		__try {
			//���SEURITY_ATTRIBUTES, ���������ܵ�
			sa.nLength = sizeof(SECURITY_ATTRIBUTES);
			sa.lpSecurityDescriptor = NULL;
			sa.bInheritHandle = TRUE;
			if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
			{
				__leave;
			}
			//���STARTUPINFO, ��������
			si.cb = sizeof(STARTUPINFO);
			si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
			si.wShowWindow = SW_HIDE;
			si.hStdOutput = hWritePipe;
			si.hStdError = hWritePipe;
			if (bCmd)
			{
				wsprintf(lpCmd, L"cmd.exe /c %s", szCmd);
			}
			else {
				wsprintf(lpCmd, L"%s", szCmd);
			}
			if (!CreateProcessW(NULL, lpCmd, NULL, NULL, true, 0, NULL, NULL, &si, &pi))
			{
				printf("CreateProcess failed with error code = %d\n", GetLastError());
				__leave;
			}
			CloseHandle(hWritePipe);
			hWritePipe = NULL;
			//WaitForSingleObject(pi.hProcess, -1);

			while (true)
			{
				// �鿴�ܵ����Ƿ�������
				DWORD dwTotalBytesAvail = 0;
				//PeekNamedPipe(hReadPipe, NULL, NULL, &dwReadByte, &dwTotalBytesAvail, NULL);
				if (!PeekNamedPipe(hReadPipe, NULL, NULL, &dwReadByte, &dwTotalBytesAvail, NULL))//PeekNamePipe����Ԥ��һ���ܵ��е����ݣ������жϹܵ����Ƿ�Ϊ��
				{
					break;
				}
				if (dwTotalBytesAvail > 0)
				{
					printf("dwTotalBytesAvail = %d\n", dwTotalBytesAvail);
					lpBuffer = (char*)calloc(1, dwTotalBytesAvail + 1);
					if (!lpBuffer)	__leave;
					// ��ȡ�ܵ�����
					if (!ReadFile(hReadPipe, lpBuffer, dwTotalBytesAvail, &dwReadByte, NULL))
					{
						free(lpBuffer);
						break;
					}
					printf("dwReadByte = %d\n", dwReadByte);
					szEcho += lpBuffer;
					free(lpBuffer);
				}
				else
				{
					DWORD dwExit = 0;
					GetExitCodeProcess(pi.hProcess, &dwExit);
					// ��������Ѿ��˳�,���ܵ��������ݵ����
					if (dwExit != STILL_ACTIVE)
					{
						break;
					}
				}
				//һ��Ҫ�Ӹ���ʱ,����������ظ�����
				Sleep(1);
			}
			ret = true;
		}
		__finally
		{
			if (hReadPipe) CloseHandle(hReadPipe);
			if (hWritePipe)	CloseHandle(hWritePipe);
			if (pi.hProcess)	CloseHandle(pi.hProcess);
			if (pi.hThread)	CloseHandle(pi.hThread);
		}

		return ret;
	}


	bool ParseDiskdriveSN(const std::string& szEcho, const char* szSearch, std::string& szSn)
	{
		//SerialNumber
		std::string::size_type loc = szEcho.find(szSearch);
		if (std::string::npos == loc)
		{
			return false;
		}
		szSn = szEcho.substr(loc + strlen(szSearch), szEcho.length());
		size_t iBegin = 0;
		size_t iEnd = 0;
		for (; iBegin < szSn.length(); iBegin++)
		{
			if (isalnum(szSn[iBegin]))
			{
				iEnd = iBegin;
				while (isalnum(szSn[iEnd++]));
				break;
			}
		}
		if (iBegin == szSn.length()) return false;
		szSn = szSn.substr(iBegin, iEnd - iBegin - 1);
		return true;
	}


	bool CStringToString(const CString& cstr, std::string& str)
	{
		int size = WideCharToMultiByte(CP_ACP, 0, cstr.GetString(), -1, 0, 0, 0, 0);
		char* pchar = (char*)malloc(size * sizeof(char));
		if (!pchar)	return false;
		memset(pchar, 0, size);
		if (!WideCharToMultiByte(CP_ACP, 0, cstr.GetString(), -1, pchar, size, 0, 0))
		{
			free(pchar);
			return false;
		}
		str = pchar;
		free(pchar);
		return true;
	}


	bool StringToCString(const std::string& str, CString& cstr)
	{
		int size = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, 0, 0);

		wchar_t* pwchar = (wchar_t*)malloc(size * sizeof(wchar_t));
		if (!pwchar)
		{
			return false;
		}
		memset(pwchar, 0, size);
		if (!MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, pwchar, size))
		{
			free(pwchar);
			return false;
		}
		cstr = pwchar;
		free(pwchar);
		return true;
	}


	bool GetMD5(const BYTE* pData, size_t length, CString& strMD5)
	{
		strMD5 = "";
		HCRYPTPROV hProv = NULL;
		if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) == FALSE)       //���CSP��һ����Կ�����ľ��
		{
			return false;
		}
		HCRYPTPROV hHash = NULL;
		if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash) == FALSE)     //��ʼ������������hash������������һ����CSP��hash������صľ��������������������CryptHashData���á�
		{
			return false;
		}
		if (CryptHashData(hHash, pData, (DWORD)length, 0) == FALSE)      //hash�ļ�
		{
			return false;
		}
		BYTE* pbHash;
		DWORD dwHashLen = sizeof(DWORD);
		if (!CryptGetHashParam(hHash, HP_HASHVAL, NULL, &dwHashLen, 0))      //��Ҳ��֪��ΪʲôҪ����������CryptGetHashParam������ǲ��յ�msdn       
		{
			return false;
		}
		pbHash = (BYTE*)malloc(dwHashLen);
		if (!pbHash)
		{
			return false;
		}
		if (CryptGetHashParam(hHash, HP_HASHVAL, pbHash, &dwHashLen, 0))            //���md5ֵ
		{
			CString temp;
			for (DWORD i = 0; i < dwHashLen; i++)         //���md5ֵ
			{
				printf("%02x", pbHash[i]);
				temp.Format(L"%02x", pbHash[i]);
				strMD5 += temp;
			}
		}
		if (CryptDestroyHash(hHash) == FALSE)          //����hash����
		{
			return false;
		}
		if (CryptReleaseContext(hProv, 0) == FALSE)
		{
			return false;
		}
		return true;
	}


	/**@brief ����Դ�ļ�������, Ȼ���滻�ļ���׺��
	* @detail c:\file\123.mp4	 ->		c:\file\123.exe
	* @param[in]  szFileName             Դ�ļ���
	* @param[in]  szNewSuffix            �µ��ļ���׺
	* @return  ����ִ�н��
	* - �ǿ��ַ�			 �ɹ�
	* - ���ַ�			ʧ��
	*/
	CString GetNewFileSuffix(const CString& szFileName, const CString& szNewSuffix)
	{
		CString szOut = L"";
		int pos = szFileName.ReverseFind(L'.');
		if (-1 == pos)	return szOut;
		szOut = szFileName.Left(pos + 1);
		szOut.Append(szNewSuffix);
		return szOut;
	}


	bool ExtractFile(const CString& szExeFileName, const CString& szMediaFileName, const CString& szSubProcName, const CString& szPasswd)
	{
		CFile fileExe, fileMedia, fileSubProc;
		APPEND_INFO ai = { 0 };
		ULONGLONG nFileLength = 0;		//szExeFileName�ļ���С
		ULONGLONG nMediaFileLength = 0;
		ULONGLONG nSubProcLength = 0;
		LONGLONG nOffset = 0;
		PBYTE lpBuf = 0;
		ULONGLONG nReadSizeSofar = 0;

		//CString csTempFileName;
		//csTempFileName = GetNewFileSuffix(szSubProcName, L"tmp");

		if (!GetAppendInfo(szExeFileName, &ai))	return false;
		nMediaFileLength = ai.nFileLength;
		nSubProcLength = ai.nSubProcLength;

		if (!fileExe.Open(szExeFileName, CFile::modeRead | CFile::typeBinary))
		{
			MessageBox(AfxGetMainWnd()->m_hWnd, L"fileExe.Open() failed", L"", MB_ICONERROR);
			return false;
		}

		if (!fileMedia.Open(szMediaFileName, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
		{
			MessageBox(AfxGetMainWnd()->m_hWnd, L"fileMedia.Open() failed", L"", MB_ICONERROR);
			fileExe.Close();
			return false;
		}

		if (!fileSubProc.Open(szSubProcName, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary | CFile::shareDenyWrite))
		{
			MessageBox(AfxGetMainWnd()->m_hWnd, L"fileMedia.Open() failed", L"", MB_ICONERROR);
			fileExe.Close();
			fileMedia.Close();
			return false;
		}

		lpBuf = (PBYTE)HeapAlloc(GetProcessHeap(), 0, BUFSIZE);
		if (!lpBuf)
		{
			fileExe.Close();
			fileMedia.Close();
			fileSubProc.Close();
			return false;
		}

		// ��media�ļ����ܺ�д������ļ�
		std::string szCryptKey = GetCryptKey(szPasswd, &ai);
		rc4_init(szCryptKey.c_str(), szCryptKey.size());

		//��ȡ�����ļ�
		g_progressBar = 0;
		nFileLength = fileExe.GetLength();
		nOffset = (LONGLONG)(nFileLength - nMediaFileLength - nSubProcLength - sizeof(APPEND_INFO));
		fileExe.Seek(nOffset, CFile::begin);
		nReadSizeSofar = 0;
		bool bLastTime = false;
		for (UINT nReadSize = fileExe.Read(lpBuf, BUFSIZE);
			nReadSize > 0;
			nReadSize = fileExe.Read(lpBuf, BUFSIZE))
		{
			if ((nReadSizeSofar + nReadSize) > nMediaFileLength)
			{
				nReadSize = (UINT)(nMediaFileLength - nReadSizeSofar);
				bLastTime = true;
			}
			nReadSizeSofar += nReadSize;
			rc4_crypt((PUCHAR)lpBuf, nReadSize);
			fileMedia.Write(lpBuf, nReadSize);
			g_progressBar = (int)(100.0 * nReadSizeSofar / nMediaFileLength);
			if (bLastTime) break;
		}

		//��ȡ�ӽ����ļ�
		OutputDebugString(L"Begin extract SubProcFile");
		nOffset = (LONGLONG)(nFileLength - nSubProcLength - sizeof(APPEND_INFO));
		fileExe.Seek(nOffset, CFile::begin);
		nReadSizeSofar = 0;

		for (UINT nReadSize = fileExe.Read(lpBuf, BUFSIZE);
			nReadSize > 0;
			nReadSize = fileExe.Read(lpBuf, BUFSIZE))
		{
			if ((nReadSizeSofar + nReadSize) <= nSubProcLength)
			{
				nReadSizeSofar += nReadSize;
				fileSubProc.Write(lpBuf, nReadSize);
			}
			else {
				nReadSize = (UINT)(nSubProcLength - nReadSizeSofar);
				fileSubProc.Write(lpBuf, nReadSize);
				break;
			}
		}

		if (nReadSizeSofar != nSubProcLength)
		{
			OutputDebugString(L"Failed extract SubProcFile");
		}
		else {
			OutputDebugString(L"Finish extract SubProcFile");
		}

		//�ͷ��ڴ�
		if (lpBuf)	HeapFree(GetProcessHeap(), 0, lpBuf);
		fileExe.Close();
		fileMedia.Close();
		fileSubProc.Close();
		//CFile::Rename(csTempFileName, szSubProcName);
		return true;
	}


	void SelfDestroy(LPCWSTR lpTempFile)
	{
		// temporary .bat file
		static const WCHAR tempbatname[] = L"_uninsep.bat";
		CString csCmd;	//bat contents
		std::string szCmd;

		static WCHAR templ[] =
			L":Repeat1\r\n"
			L"del \"%s\"\r\n"
			L"if exist \"%s\" goto Repeat1\r\n"
			L":Repeat2\r\n"
			L"del \"%s\"\r\n"
			L"if exist \"%s\" goto Repeat2\r\n"
			L"del \"%s\"";

		WCHAR modulename[_MAX_PATH];    // absolute path of calling .exe file
		WCHAR temppath[_MAX_PATH];      // absolute path of temporary .bat file
		// absolute path of temporary .bat file
		GetTempPath(_MAX_PATH, temppath);
		wcscat_s(temppath, tempbatname);
		// absolute path of calling .exe file
		GetModuleFileName(NULL, modulename, MAX_PATH);

		HANDLE hf;

		hf = CreateFile(temppath, GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hf != INVALID_HANDLE_VALUE)
		{
			csCmd.Format(templ, lpTempFile, lpTempFile, modulename, modulename, temppath);
			CStringToString(csCmd, szCmd);
			DWORD len;
			WriteFile(hf, szCmd.c_str(), szCmd.size(), &len, NULL);
			CloseHandle(hf);
			ShellExecuteW(NULL, L"open", temppath, NULL, NULL, SW_HIDE);
		}
	}


	void PrintError(LPCWSTR msg)
	{
		CString temp;
		temp.Format(L"%s failed with error code = %d", msg, ::GetLastError());
		::OutputDebugString(temp);
	}


	void ShowAppendInfo(APPEND_INFO* ai)
	{
		CString msg;
		std::string mark, pwd, physn;
		WORD nPlayTimes = GetPlayTimes(ai);
		UCHAR nAccessMode = GetAccessMode(ai);

		char temp[33] = { 0 };

		memcpy_s(temp, 33, ai->szMark, 32);
		mark = temp;

		memcpy_s(temp, 33, ai->szPwd, 32);
		pwd = temp;

		memcpy_s(temp, 33, ai->szPhySn, 32);
		physn = temp;

		msg.Format(L"szMark = %S\nszPwd = %S\nszPhySn = %S", mark.c_str(), pwd.c_str(), physn.c_str());
		::OutputDebugString(msg);

		msg.Format(L"nFileLength = %ul, nSubProcLength = %ul, nPlayTimes = %d, nAccessMode = %d\n",
			ai->nFileLength, ai->nSubProcLength, nPlayTimes, nAccessMode);
		::OutputDebugString(msg);

	}


	static unsigned char g_sbox[256];

	/*��ʼ������*/
	void rc4_init(const char* key, long Len)
	{
		int i = 0, j = 0;
		unsigned char tmp = 0;
		for (i = 0; i < 256; i++)
		{
			g_sbox[i] = i;
		}

		for (i = 0; i < 256; i++)
		{
			j = (j + g_sbox[i] + key[i % Len]) % 256;
			tmp = g_sbox[i];
			g_sbox[i] = g_sbox[j];//����s[i]��s[j]
			g_sbox[j] = tmp;
		}
	}

	/*�ӽ���*/
	void rc4_crypt(unsigned char* Data, long Len)
	{
		int i = 0, j = 0, t = 0;
		long k = 0;
		unsigned char tmp;
		for (k = 0; k < Len; k++)
		{
			i = (i + 1) % 256;
			j = (j + g_sbox[i]) % 256;
			tmp = g_sbox[i];
			g_sbox[i] = g_sbox[j];//����s[x]��s[y]
			g_sbox[j] = tmp;
			t = (g_sbox[i] + g_sbox[j]) % 256;
			Data[k] ^= g_sbox[t];
		}
	}


}//end of namespace

