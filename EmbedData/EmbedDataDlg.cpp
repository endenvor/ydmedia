
// EmbedDataDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "EmbedData.h"
#include "EmbedDataDlg.h"
#include "afxdialogex.h"
#include "ydmedia.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


DWORD WINAPI THreadEmbedFile(LPVOID lpParam)
{
	CEmbedDataDlg* _this = (CEmbedDataDlg*)lpParam;
	_this->EmbedFileThread();
	return 0;
}

BOOL CEmbedDataDlg::EmbedFileThread()
{
	/*
	1. 对视频文件进行编码分析
	2. 根据需要进行转码
	3. 使用加密算法生成加密视频文件
	4. 嵌入加密视频数据及其附加数据
	*/
	SetTimer(1, 500, NULL);
	m_bEmbeding = TRUE;
	if (YDMEDIA::EmbedFile(L".\\YDPlayer.exe", m_szFilePath, L".\\ydsecaid.exe", m_szPwd, m_nPlayTimes, m_bExclusive))
	{
		//MessageBox(L"生成外发视频成功");
		int nIndex = m_szFilePath.ReverseFind(L'\\');
		CString szMediaDir = m_szFilePath.Left(nIndex);
		ShellExecuteW(NULL, L"explore", szMediaDir, NULL, NULL, SW_SHOWNORMAL);
	}
	else {
		MessageBox(L"生成外发视频失败");
	}
	(CButton*)(GetDlgItem(IDC_BUTTON_OK))->EnableWindow(TRUE);
	SetDlgItemText(IDC_BUTTON_OK, L"确定");
	m_bEmbeding = FALSE;
	KillTimer(1);
	return 0;
}

// CEmbedDataDlg 对话框

CEmbedDataDlg::CEmbedDataDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_EMBEDDATA_DIALOG, pParent)
	, m_nPlayTimes(0)
	, m_szFilePath(_T(""))
	, m_szPwd(_T(""))
	, m_bExclusive(FALSE)
	, m_bEmbeding(FALSE)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CEmbedDataDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_PLAY_TIMES, m_nPlayTimes);
	DDV_MinMaxInt(pDX, m_nPlayTimes, 0, 65535);
	DDX_Text(pDX, IDC_EDIT_FILE_SELECT, m_szFilePath);
	DDX_Text(pDX, IDC_EDIT_PASSWD, m_szPwd);
	DDX_Check(pDX, IDC_CHECK_EXCLUSIVE, m_bExclusive);
}

BEGIN_MESSAGE_MAP(CEmbedDataDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_OK, &CEmbedDataDlg::OnBnClickedButtonOk)
	ON_BN_CLICKED(IDC_BTN_FILE_SELECT, &CEmbedDataDlg::OnBnClickedBtnFileSelect)
	ON_WM_DROPFILES()
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CEmbedDataDlg 消息处理程序

BOOL CEmbedDataDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CEmbedDataDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	if (1 == nIDEvent)
	{
		CString szProgressBar;
		szProgressBar.Format(L"%d%%", YDMEDIA::g_progressBar);
		SetDlgItemTextW(IDC_BUTTON_OK, szProgressBar);
	}
	CDialogEx::OnTimer(nIDEvent);
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CEmbedDataDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CEmbedDataDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CEmbedDataDlg::OnBnClickedButtonOk()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
	if (m_nPlayTimes < 0 || m_nPlayTimes > 65535)
		return;
	CString csPwd = m_szPwd;
	(CButton*)(GetDlgItem(IDC_BUTTON_OK))->EnableWindow(FALSE);
	CreateThread(NULL, 0, THreadEmbedFile, this, 0, NULL);
}



void CEmbedDataDlg::OnBnClickedBtnFileSelect()
{
	// TODO: 在此添加控件通知处理程序代码
	CFileDialog dlgFile(TRUE);
	if (dlgFile.DoModal() == IDOK)
	{
		m_szFilePath = dlgFile.GetPathName();
		UpdateData(FALSE);
	}
}


void CEmbedDataDlg::OnDropFiles(HDROP hDropInfo)
{
	UINT dragCount = 0;
	UINT copyCount = 0;
	DWORD fileAttributes = 0;
	WCHAR lpszDropFileName[MAX_PATH] = { 0 };

	dragCount = DragQueryFileW(hDropInfo, 0xFFFFFFFF, nullptr, 0);
	if (dragCount > 1)
	{
		MessageBox(L"只接收一个文件");
		return;
	}

	copyCount = DragQueryFileW(hDropInfo, 0, lpszDropFileName, MAX_PATH);
	lpszDropFileName[copyCount] = 0;

	fileAttributes = GetFileAttributes(lpszDropFileName);
	if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		MessageBox(L"文件夹无效");
		return;
	}
	SetDlgItemText(IDC_EDIT_FILE_SELECT, lpszDropFileName);
	m_szFilePath = lpszDropFileName;
	DragFinish(hDropInfo);
	CDialogEx::OnDropFiles(hDropInfo);
}



void CEmbedDataDlg::OnCancel()
{
	// TODO: 在此添加专用代码和/或调用基类
	if (m_bEmbeding)
	{
		::DeleteFile(m_szFilePath);
	}
	CDialogEx::OnCancel();
}
