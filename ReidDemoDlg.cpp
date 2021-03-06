
// ReidDemoDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "ReidDemo.h"
#include "ReidDemoDlg.h"
#include "afxdialogex.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

///变量
static Point origin;
static Mat srcImageA, srcImageB, srcImageXR;
static CRect rectA, rectB, rectXR, rectHist;
static Mat dstImageA, dstImageB;
static std::vector<cv::Rect> regionsA;
static std::vector<cv::Rect> regionsB;
static MatND HistXR;
static std::vector<cv::MatND> srcHistB;
static vector<double> compareResult;
// 视频
static VideoCapture captureA, captureB;
static VideoCapture vcapA, vcapB;
static const std::string videoStreamAddressA = "http://192.168.1.143:81/videostream.cgi?user=admin&pwd=888888&.mjpg";
static const std::string videoStreamAddressB = "http://192.168.1.142:81/videostream.cgi?user=admin&pwd=888888&.mjpg";
// HOG对象
static cv::HOGDescriptor hog
(
	cv::Size(64, 128), cv::Size(16, 16),
	cv::Size(8, 8), cv::Size(8, 8), 9, 1, -1,
	cv::HOGDescriptor::L2Hys, 0.2, true, cv::HOGDescriptor::DEFAULT_NLEVELS
);
// 直方图参数
static int hbins = 9, sbins = 16;
static int histSize[] = { hbins,sbins };
static int channels[] = { 0,1 };
static float hRange[] = { 0,180 };
static float sRange[] = { 0,256 };
const float* ranges[] = { hRange,sRange };
// 标志位
bool offvideoFlagA = false;
bool offvideoFlagB = false;
bool OLvideoFalgA = false;
bool OLvideoFalgB = false;

///函数
/* @brief 得到一副图像的直方图数据
@param srcImage 输入图像
@param srcHist 输出直方图
@return void 返回值为空
*/
void getHist(const Mat srcImage, MatND &srcHist) {
	Mat srcHsvImage;

	cvtColor(srcImage, srcHsvImage, CV_RGB2HSV);

	calcHist(&srcHsvImage, 1, channels, Mat(), srcHist, 2, histSize, ranges, true, false);
	normalize(srcHist, srcHist, 0, 1, NORM_MINMAX, -1, Mat());
}

/* @brief 得到H分量的直方图图像
@param src 输入图像
@param histimg 输出颜色直方图
@return void 返回值为空
*/
void getHistImg(const Mat src, Mat &histimg) {
	Mat hue, hist;

	int hsize = 18;
	float hranges[] = { 0,180 };
	const float* phranges = hranges;

	int ch[] = { 0, 0 };
	hue.create(src.size(), src.depth());
	mixChannels(&src, 1, &hue, 1, ch, 1);

	calcHist(&hue, 1, 0, Mat(), hist, 1, &hsize, &phranges);

	normalize(hist, hist, 0, 255, NORM_MINMAX);

	histimg = Scalar::all(0);
	int binW = histimg.cols / hsize;
	Mat buf(1, hsize, CV_8UC3);
	for (int i = 0; i < hsize; i++)
		buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180. / hsize), 255, 255);
	cvtColor(buf, buf, COLOR_HSV2BGR);

	for (int i = 0; i < hsize; i++)
	{
		int val = saturate_cast<int>(hist.at<float>(i)*histimg.rows / 255);
		rectangle(histimg, Point(i*binW, histimg.rows),
			Point((i + 1)*binW, histimg.rows - val),
			Scalar(buf.at<Vec3b>(i)), -1, 8);
	}
}

/* @brief 核心代码
*/
void coreFunction(CReidDemoDlg * const  obj) {
	Mat frameA, frameB;

	while (true) {

		//offVideoA
		if (offvideoFlagA) {
			captureA >> frameA;

			if (!frameA.empty()) {
				frameA.copyTo(srcImageA);

				// 在图像上检测行人区域			
				hog.detectMultiScale(frameA, regionsA, 0, cv::Size(8, 8), cv::Size(32, 32), 1.15, 1, 0);//detectMultiScale:第六个参数scale通常取值1.01-1.50
				// 显示
				for (int i = 0; i < regionsA.size(); i++)
				{
					regionsA[i].x += cvRound(regionsA[i].width*0.1);
					regionsA[i].width = cvRound(regionsA[i].width*0.8);
					regionsA[i].y += cvRound(regionsA[i].height*0.07);
					regionsA[i].height = cvRound(regionsA[i].height*0.8);

					cv::rectangle(frameA, regionsA[i], cv::Scalar(0, 0, 255), 2);
				}
				resize(frameA, dstImageA, cv::Size(rectA.Width(), rectA.Height()));
				imshow("ViewA", dstImageA);
				waitKey(1);
			}
			else {
				offvideoFlagA = false;
				continue;
			}
		}

		//offVideoB
		if(offvideoFlagB){
			captureB >> frameB;
			frameB.copyTo(srcImageB);

			///若为空帧则跳出，视频播放完成
			if (frameB.empty()) {
				offvideoFlagB = false;
				continue;
			}

			// 3. 在测试图像上检测行人区域	
			hog.detectMultiScale(frameB, regionsB, 0, cv::Size(8, 8), cv::Size(32, 32), 1.15, 1, 0);//detectMultiScale:第六个参数scale通常取值1.01-1.50
			if (regionsB.size()) {
				srcHistB.resize(regionsB.size());
				compareResult.resize(regionsB.size());
				Mat tempRoi;
				// 显示
				for (int i = 0; i < regionsB.size(); i++)
				{
					regionsB[i].x += cvRound(regionsB[i].width*0.1);
					regionsB[i].width = cvRound(regionsB[i].width*0.8);
					regionsB[i].y += cvRound(regionsB[i].height*0.07);
					regionsB[i].height = cvRound(regionsB[i].height*0.8);

					tempRoi = frameB(regionsB[i]);
					getHist(tempRoi, srcHistB[i]);
					compareResult[i] = compareHist(HistXR, srcHistB[i], 0);
					//cv::rectangle(detectImage, r, cv::Scalar(0, 0, 255), 2);
				}

				std::vector<double>::iterator biggest = std::max_element(std::begin(compareResult), std::end(compareResult));
				size_t MaxIndex = std::distance(std::begin(compareResult), biggest);

				cv::rectangle(frameB, regionsB[MaxIndex], cv::Scalar(0, 0, 255), 2);
			}
			resize(frameB, dstImageB, cv::Size(rectA.Width(), rectA.Height()));
			imshow("ViewB", dstImageB);

			waitKey(1);
		}

		//OLVideoA
		if (OLvideoFalgA) {
				vcapA >> frameA;

				if (frameA.empty()) {
					cout << "No frame" << endl;
					break;
				}

				// 测试图像上检测行人区域
				hog.detectMultiScale(frameA, regionsA, 0, cv::Size(8, 8), cv::Size(32, 32), 1.05, 1, 0);//detectMultiScale:第六个参数scale通常取值1.01-1.50

				// 显示
				for (int i = 0; i < regionsA.size(); i++)
				{
					cv::rectangle(frameA, regionsA[i], cv::Scalar(0, 0, 255), 2);
				}
				//resize(frameA, frameA, cv::Size(rectA.Width(), rectA.Height()));
				imshow("ViewA", frameA);

			//if (waitKey(1));

		}

		//OLVideoB
		if (OLvideoFalgB) {
			vcapB >> frameB;

			if (frameB.empty()) {
				cout << "No frame" << endl;
				break;
			}

			//// 测试图像上检测行人区域			
			//hog.detectMultiScale(frameB, regionsB, 0, cv::Size(8, 8), cv::Size(32, 32), 1.05, 1, 0);//detectMultiScale:第六个参数scale通常取值1.01-1.50

			//// 显示
			//for (int i = 0; i < regionsA.size(); i++)
			//{
			//	cv::rectangle(frameB, regionsA[i], cv::Scalar(0, 0, 255), 2);
			//}
			//resize(frameA, frameA, cv::Size(rectA.Width(), rectA.Height()));
			imshow("ViewB", frameB);

			if (waitKey(1));
		}

		//obj->m_edit_test.SetWindowText(_T("wwww"));
		waitKey(1);
	}

}


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框
class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CReidDemoDlg 对话框


CReidDemoDlg::CReidDemoDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_REIDDEMO_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CReidDemoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	//  DDX_Control(pDX, IDC_EDIT_Test, m_edit_test);
	DDX_Control(pDX, IDC_EDIT_Test, m_edit_test);
}

BEGIN_MESSAGE_MAP(CReidDemoDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_ShowVideoA, &CReidDemoDlg::OnBnClickedShowvideoa)
	ON_BN_CLICKED(IDC_Test, &CReidDemoDlg::OnBnClickedTest)
	ON_BN_CLICKED(IDC_DetectA, &CReidDemoDlg::OnBnClickedDetecta)
	ON_BN_CLICKED(IDC_BtnShotA, &CReidDemoDlg::OnBnClickedBtnshota)
	ON_BN_CLICKED(IDC_Test2, &CReidDemoDlg::OnBnClickedTest2)
	ON_BN_CLICKED(IDC_DetectB, &CReidDemoDlg::OnBnClickedDetectb)
	ON_BN_CLICKED(IDC_BtnVideoTestA, &CReidDemoDlg::OnBnClickedBtnvideotesta)
	ON_BN_CLICKED(IDC_BtnVideoTestB, &CReidDemoDlg::OnBnClickedBtnvideotestb)
	ON_BN_CLICKED(IDC_BtnStart, &CReidDemoDlg::OnBnClickedBtnstart)
	ON_BN_CLICKED(IDC_ShowVideoB, &CReidDemoDlg::OnBnClickedShowvideob)
END_MESSAGE_MAP()


// CReidDemoDlg 消息处理程序

BOOL CReidDemoDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	Mat temp;
	//picture control
	///PicA
	namedWindow("ViewA", WINDOW_AUTOSIZE);
	HWND hWndA = (HWND)cvGetWindowHandle("ViewA");
	HWND hParentA = ::GetParent(hWndA);
	::SetParent(hWndA, GetDlgItem(IDC_PicA)->m_hWnd);
	::ShowWindow(hParentA, SW_HIDE);
	///PicXR
	namedWindow("ViewXR", WINDOW_AUTOSIZE);
	HWND hWndXR = (HWND)cvGetWindowHandle("ViewXR");
	HWND hParentXR = ::GetParent(hWndXR);
	::SetParent(hWndXR, GetDlgItem(IDC_PicXR)->m_hWnd);
	::ShowWindow(hParentXR, SW_HIDE);
	///picHist
	namedWindow("ViewHist", WINDOW_AUTOSIZE);
	HWND hWndHist = (HWND)cvGetWindowHandle("ViewHist");
	HWND hParentHist = ::GetParent(hWndHist);
	::SetParent(hWndHist, GetDlgItem(IDC_PicHist)->m_hWnd);
	::ShowWindow(hParentHist, SW_HIDE);	
	///PicB
	namedWindow("ViewB", WINDOW_AUTOSIZE);
	HWND hWndB = (HWND)cvGetWindowHandle("ViewB");
	HWND hParentB = ::GetParent(hWndB);
	::SetParent(hWndB, GetDlgItem(IDC_PicB)->m_hWnd);
	::ShowWindow(hParentB, SW_HIDE);
	///resize
	GetDlgItem(IDC_PicA)->GetClientRect(&rectA);
	GetDlgItem(IDC_PicB)->GetClientRect(&rectB);
	GetDlgItem(IDC_PicXR)->GetClientRect(&rectXR);
	GetDlgItem(IDC_PicHist)->GetClientRect(&rectHist);
	//初始显示
	///PicA
	temp = imread("./res/cameraA.jpg");
	resize(temp, temp, cv::Size(rectA.Width(), rectA.Height()));
	imshow("ViewA", temp);
	///PicB
	temp = imread("./res/cameraB.jpg");
	resize(temp, temp, cv::Size(rectB.Width(), rectB.Height()));
	imshow("ViewB", temp);
	///PicXR
	temp = imread("./res/xrinit.jpg");
	resize(temp, temp, cv::Size(rectXR.Width(), rectXR.Height()));
	imshow("ViewXR", temp);
	///picHist
	temp = imread("./res/hist.jpg");
	resize(temp, temp, cv::Size(rectHist.Width(), rectHist.Height()));
	imshow("ViewHist", temp);
	//Hog
	///采用已经训练好的行人检测分类器，另有getDaimlerPeopleDetector()
	hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector()); 
	

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CReidDemoDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码来绘制该图标。
// 对于使用文档/视图模型的 MFC 应用程序，这将由框架自动完成。


void CReidDemoDlg::OnPaint()
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
		//CDialogEx::OnPaint();
		CRect   rect;
		CPaintDC   dc(this);
		GetClientRect(rect);
		dc.FillSolidRect(rect, RGB(255, 255, 255));
		CDialog::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CReidDemoDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CReidDemoDlg::OnBnClickedShowvideoa()
{
	// TODO: 在此添加控件通知处理程序代码
	OLvideoFalgA = true;
	// open the video stream and make sure it's opened
	if (!vcapA.open(videoStreamAddressA)) {
		AfxMessageBox(_T("打开视频流A错误，请检查IP地址！"));
	}
}



void CReidDemoDlg::OnBnClickedShowvideob()
{
	// TODO: 在此添加控件通知处理程序代码
	OLvideoFalgB = true;
	// open the video stream and make sure it's opened
	if (!vcapB.open(videoStreamAddressB)) {
		AfxMessageBox(_T("打开视频流B错误，请检查IP地址！"));
	}

}



void CReidDemoDlg::OnBnClickedTest()
{
	// TODO: 在此添加控件通知处理程序代码
	//读入图片
	srcImageA = imread("./res/01/0102.jpg");

	//改变图片尺寸
	resize(srcImageA, dstImageA, cv::Size(rectA.Width(), rectA.Height()));
	
	imshow("ViewA", dstImageA);
}

/* @brief 供setMouseCallback调用。
*/
static void onMouse(int event, int x, int y, int, void*)
{
	Mat dstXR, histimgXR = Mat::zeros(rectHist.Height(), rectHist.Width(), CV_8UC3);
	switch (event)
	{
	case EVENT_LBUTTONDOWN:
		origin = Point(x, y);
		for (int i = 0; i < regionsA.size(); i++) {
			if (origin.inside(regionsA[i]))
			{				
				//以下操作获取图形控件尺寸并以此改变图片尺寸  
				srcImageXR = srcImageA(regionsA[i]);
				resize(srcImageXR, dstXR, cv::Size(rectXR.Width(), rectXR.Height()));
				imshow("ViewXR", dstXR);
				//直方图
				getHistImg(srcImageXR, histimgXR);
				imshow("ViewHist", histimgXR);
				break;
			}
		}
		break;
	}
}

void CReidDemoDlg::OnBnClickedDetecta()
{
	// TODO: 在此添加控件通知处理程序代码
	Mat detectImage;
	dstImageA.copyTo(detectImage);
	
	// 在测试图像上检测行人区域	
	///detectMultiScale:第六个参数scale通常取值1.01-1.50
	hog.detectMultiScale(detectImage, regionsA, 0, cv::Size(8, 8), cv::Size(32, 32), 1.05, 1, 0);
	
	// 显示
	for (size_t i = 0; i < regionsA.size(); i++)
	{
		regionsA[i].x += cvRound(regionsA[i].width*0.1);
		regionsA[i].width = cvRound(regionsA[i].width*0.8);
		regionsA[i].y += cvRound(regionsA[i].height*0.07);
		regionsA[i].height = cvRound(regionsA[i].height*0.8);

		cv::rectangle(detectImage, regionsA[i], cv::Scalar(0, 0, 255), 2);
	}
	imshow("ViewA", detectImage);

	setMouseCallback("ViewA", onMouse, 0);
}


void CReidDemoDlg::OnBnClickedBtnshota()
{
	// TODO: 在此添加控件通知处理程序代码

}


void CReidDemoDlg::OnBnClickedTest2()
{
	// TODO: 在此添加控件通知处理程序代码
	//读入图片
	srcImageB = imread("./res/01/0104.jpg");

	//以下操作获取图形控件尺寸并以此改变图片尺寸  
	resize(srcImageB, dstImageB, cv::Size(rectB.Width(), rectB.Height()));

	imshow("ViewB", dstImageB);
}


void CReidDemoDlg::OnBnClickedDetectb()
{
	if (!srcImageXR.empty())
	{
		getHist(srcImageXR, HistXR);

		// TODO: 在此添加控件通知处理程序代码
		Mat detectImage;
		dstImageB.copyTo(detectImage);

		// 3. 在测试图像上检测行人区域	
		hog.detectMultiScale(detectImage, regionsB, 0, cv::Size(8, 8), cv::Size(32, 32), 1.05, 1, 0);//detectMultiScale:第六个参数scale通常取值1.01-1.50
		if (regionsB.size()) {
			srcHistB.resize(regionsB.size());
			Mat tempRoi;
			// 显示
			for (int i = 0; i < regionsB.size(); i++)
			{
				regionsB[i].x += cvRound(regionsB[i].width*0.1);
				regionsB[i].width = cvRound(regionsB[i].width*0.8);
				regionsB[i].y += cvRound(regionsB[i].height*0.07);
				regionsB[i].height = cvRound(regionsB[i].height*0.8);

				tempRoi = detectImage(regionsB[i]);
				getHist(tempRoi, srcHistB[i]);

				//cv::rectangle(detectImage, r, cv::Scalar(0, 0, 255), 2);
			}
			vector<double> compareResult;
			compareResult.resize(regionsB.size());
			CString str;
			for (int i = 0; i < regionsB.size(); i++) {
				compareResult[i] = compareHist(HistXR, srcHistB[i], 0);
			}
			std::vector<double>::iterator biggest = std::max_element(std::begin(compareResult), std::end(compareResult));
			size_t MaxIndex = std::distance(std::begin(compareResult), biggest);

			str.Format(_T("%.2f"), compareResult[MaxIndex]);
			SetDlgItemText(IDC_EDIT_Test, str);

			cv::rectangle(detectImage, regionsB[MaxIndex], cv::Scalar(0, 0, 255), 2);
		}
		else {
			AfxMessageBox(_T("未检测到行人！"));
		}
		imshow("ViewB", detectImage);
	}
	else AfxMessageBox(_T("未选择行人！"));
}


void CReidDemoDlg::OnBnClickedBtnvideotesta()
{
	// TODO: 在此添加控件通知处理程序代码
	setMouseCallback("ViewA", onMouse, 0);
	
	//captureA.open("./res/viptrain.avi");
	captureA.open("./res/viptrain.avi");
	

	offvideoFlagA = true;
}


void CReidDemoDlg::OnBnClickedBtnvideotestb()
{
	// TODO: 在此添加控件通知处理程序代码
	// 得到行人的颜色直方图
	getHist(srcImageXR, HistXR);

	captureB.open("./res/viptrain.avi");

	offvideoFlagB = true;
}

void CReidDemoDlg::OnBnClickedBtnstart()
{
	// TODO: 在此添加控件通知处理程序代码
	coreFunction(this);
	setMouseCallback("ViewA", onMouse, 0);
}

