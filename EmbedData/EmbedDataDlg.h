
// EmbedDataDlg.h: 头文件
//

#pragma once


// CEmbedDataDlg 对话框
class CEmbedDataDlg : public CDialogEx
{
// 构造
public:
	CEmbedDataDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_EMBEDDATA_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonOk();
private:
	// 播放次数
	int m_nPlayTimes;
	CString m_szPwd;
	bool m_bEmbeding;

public:
	afx_msg void OnBnClickedBtnFileSelect();
	CString m_szFilePath;
	// //是否只能在一台机器上播放
	BOOL m_bExclusive;
	afx_msg void OnDropFiles(HDROP hDropInfo);
	BOOL EmbedFileThread();

	afx_msg void OnTimer(UINT_PTR nIDEvent);
	virtual void OnCancel();
};
