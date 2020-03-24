
#include "pch.h"
#include "ydmedia.h"


namespace YDMEDIA {

	volatile int g_progressBar = 0;

	//获取剩余播放次数
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

	//用文件大小，对chaos数组大小求余，用来计算播放次数所占用的位数，之后偏移16位存放其数值
	//播放次数最大占2个字节，即PlayTimes < 65535
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


	//对szPasswd, TEXT(MARK), (UINT)nMediaFileLength拼接后, md5的结果用于加密key
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


	//填充垃圾数据
	void FillJunkData(void* pData, int nBytes)
	{
		srand((unsigned int)time(NULL));
		PBYTE bytes = (PBYTE)pData;
		for (int i = 0; i < nBytes; i++)
		{
			bytes[i] = rand() % 256;
		}
	}


	/**@brief 将一个文件内容嵌入另一个文件末尾, 并添加附加AppendInfo数据
	* @param[in]  szDstFileName             嵌入数据的目标文件副本
	* @param[in]  szSrcFileName             嵌入数据的源文件
	* @param[in]  pAppendInfo               嵌入的附加AppendInfo数据
	* @return  函数执行结果
	* - true			成功
	* - false        失败
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

		//复制dst文件
		for (nReadLength = fileDst.Read(lpBuf, BUFSIZE);
			nReadLength > 0;
			nReadLength = fileDst.Read(lpBuf, BUFSIZE))
		{
			fileOut.Write(lpBuf, nReadLength);
		}

		//填充ai
		//填充Mark MD5
		CString csMark;
		std::string szMark = MARK;
		GetMD5((PBYTE)szMark.c_str(), szMark.size(), csMark);
		CStringToString(csMark, szMark);
		memcpy_s(ai.szMark, 32, szMark.c_str(), 32);

		//填充密码MD5
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

		//对media文件加密后写入输出文件
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

		//复制subproc文件
		for (nReadLength = fileSubProc.Read(lpBuf, BUFSIZE);
			nReadLength > 0;
			nReadLength = fileSubProc.Read(lpBuf, BUFSIZE))
		{
			fileOut.Write(lpBuf, nReadLength);
		}

		//写入附加数据
		fileOut.Write(&ai, sizeof(APPEND_INFO));
		//释放内存
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
			//填充SEURITY_ATTRIBUTES, 创建匿名管道
			sa.nLength = sizeof(SECURITY_ATTRIBUTES);
			sa.lpSecurityDescriptor = NULL;
			sa.bInheritHandle = TRUE;
			if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
			{
				__leave;
			}
			//填充STARTUPINFO, 创建进程
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
				// 查看管道中是否有数据
				DWORD dwTotalBytesAvail = 0;
				//PeekNamedPipe(hReadPipe, NULL, NULL, &dwReadByte, &dwTotalBytesAvail, NULL);
				if (!PeekNamedPipe(hReadPipe, NULL, NULL, &dwReadByte, &dwTotalBytesAvail, NULL))//PeekNamePipe用来预览一个管道中的数据，用来判断管道中是否为空
				{
					break;
				}
				if (dwTotalBytesAvail > 0)
				{
					printf("dwTotalBytesAvail = %d\n", dwTotalBytesAvail);
					lpBuffer = (char*)calloc(1, dwTotalBytesAvail + 1);
					if (!lpBuffer)	__leave;
					// 读取管道数据
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
					// 避免程序已经退出,但管道仍有数据的情况
					if (dwExit != STILL_ACTIVE)
					{
						break;
					}
				}
				//一定要加个延时,否则可能有重复数据
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
		if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) == FALSE)       //获得CSP中一个密钥容器的句柄
		{
			return false;
		}
		HCRYPTPROV hHash = NULL;
		if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash) == FALSE)     //初始化对数据流的hash，创建并返回一个与CSP的hash对象相关的句柄。这个句柄接下来将被CryptHashData调用。
		{
			return false;
		}
		if (CryptHashData(hHash, pData, (DWORD)length, 0) == FALSE)      //hash文件
		{
			return false;
		}
		BYTE* pbHash;
		DWORD dwHashLen = sizeof(DWORD);
		if (!CryptGetHashParam(hHash, HP_HASHVAL, NULL, &dwHashLen, 0))      //我也不知道为什么要先这样调用CryptGetHashParam，这块是参照的msdn       
		{
			return false;
		}
		pbHash = (BYTE*)malloc(dwHashLen);
		if (!pbHash)
		{
			return false;
		}
		if (CryptGetHashParam(hHash, HP_HASHVAL, pbHash, &dwHashLen, 0))            //获得md5值
		{
			CString temp;
			for (DWORD i = 0; i < dwHashLen; i++)         //输出md5值
			{
				printf("%02x", pbHash[i]);
				temp.Format(L"%02x", pbHash[i]);
				strMD5 += temp;
			}
		}
		if (CryptDestroyHash(hHash) == FALSE)          //销毁hash对象
		{
			return false;
		}
		if (CryptReleaseContext(hProv, 0) == FALSE)
		{
			return false;
		}
		return true;
	}


	/**@brief 生成源文件名副本, 然后替换文件后缀名
	* @detail c:\file\123.mp4	 ->		c:\file\123.exe
	* @param[in]  szFileName             源文件名
	* @param[in]  szNewSuffix            新的文件后缀
	* @return  函数执行结果
	* - 非空字符			 成功
	* - 空字符			失败
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
		ULONGLONG nFileLength = 0;		//szExeFileName文件大小
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

		// 对media文件加密后写入输出文件
		std::string szCryptKey = GetCryptKey(szPasswd, &ai);
		rc4_init(szCryptKey.c_str(), szCryptKey.size());

		//提取播放文件
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

		//提取子进程文件
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

		//释放内存
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

	/*初始化函数*/
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
			g_sbox[i] = g_sbox[j];//交换s[i]和s[j]
			g_sbox[j] = tmp;
		}
	}

	/*加解密*/
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
			g_sbox[i] = g_sbox[j];//交换s[x]和s[y]
			g_sbox[j] = tmp;
			t = (g_sbox[i] + g_sbox[j]) % 256;
			Data[k] ^= g_sbox[t];
		}
	}


}//end of namespace

