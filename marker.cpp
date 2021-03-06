﻿
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/aruco.hpp>
#include <iostream>

using namespace std;
using namespace cv;

namespace {
	const char* about = "Basic marker detection";
	const char* keys =
		"{d        |  8     | dictionary: DICT_4X4_50=0, DICT_4X4_100=1, DICT_4X4_250=2,"
		"DICT_4X4_1000=3, DICT_5X5_50=4, DICT_5X5_100=5, DICT_5X5_250=6, DICT_5X5_1000=7, "
		"DICT_6X6_50=8, DICT_6X6_100=9, DICT_6X6_250=10, DICT_6X6_1000=11, DICT_7X7_50=12,"
		"DICT_7X7_100=13, DICT_7X7_250=14, DICT_7X7_1000=15, DICT_ARUCO_ORIGINAL = 16}"
		"{v        |       | Input from video file, if ommited, input comes from camera }"
		"{ci       | 0     | Camera id if input doesnt come from video (-v) }"
		"{c        | calib.yml      | Camera intrinsic parameters. Needed for camera pose }"
		"{l        | 0.1   | Marker side lenght (in meters). Needed for correct scale in camera pose }"
		"{dp       |       | File of marker detector parameters }"
		"{r        |       | show rejected candidates too }";
}

/**
*/
static bool readCameraParameters(string filename, Mat &camMatrix, Mat &distCoeffs) {
	FileStorage fs(filename, FileStorage::READ);
	if (!fs.isOpened())
		return false;
	fs["camera_matrix"] >> camMatrix;
	fs["distortion_coefficients"] >> distCoeffs;
	return true;
}



/**
*/
static bool readDetectorParameters(string filename, Ptr<aruco::DetectorParameters> &params) {
	FileStorage fs(filename, FileStorage::READ);
	if (!fs.isOpened())
		return false;
	fs["adaptiveThreshWinSizeMin"] >> params->adaptiveThreshWinSizeMin;
	fs["adaptiveThreshWinSizeMax"] >> params->adaptiveThreshWinSizeMax;
	fs["adaptiveThreshWinSizeStep"] >> params->adaptiveThreshWinSizeStep;
	fs["adaptiveThreshConstant"] >> params->adaptiveThreshConstant;
	fs["minMarkerPerimeterRate"] >> params->minMarkerPerimeterRate;
	fs["maxMarkerPerimeterRate"] >> params->maxMarkerPerimeterRate;
	fs["polygonalApproxAccuracyRate"] >> params->polygonalApproxAccuracyRate;
	fs["minCornerDistanceRate"] >> params->minCornerDistanceRate;
	fs["minDistanceToBorder"] >> params->minDistanceToBorder;
	fs["minMarkerDistanceRate"] >> params->minMarkerDistanceRate;
	fs["cornerRefinementMethod"] >> params->cornerRefinementMethod;
	fs["cornerRefinementWinSize"] >> params->cornerRefinementWinSize;
	fs["cornerRefinementMaxIterations"] >> params->cornerRefinementMaxIterations;
	fs["cornerRefinementMinAccuracy"] >> params->cornerRefinementMinAccuracy;
	fs["markerBorderBits"] >> params->markerBorderBits;
	fs["perspectiveRemovePixelPerCell"] >> params->perspectiveRemovePixelPerCell;
	fs["perspectiveRemoveIgnoredMarginPerCell"] >> params->perspectiveRemoveIgnoredMarginPerCell;
	fs["maxErroneousBitsInBorderRate"] >> params->maxErroneousBitsInBorderRate;
	fs["minOtsuStdDev"] >> params->minOtsuStdDev;
	fs["errorCorrectionRate"] >> params->errorCorrectionRate;
	return true;
}

Mat drawWorldImg(bool flag, int i, vector< vector< Point2f > > corners, Mat image) {
	Mat src;
	src = cv::imread("zhaohuan.jpg",1);
	if (src.empty()) {
		cout << "图像加载失败！" << endl;
		return image;
	}
	cv::Point2f srcQuad[] = {
		cv::Point2f(0,0),
		cv::Point2f(src.cols - 1,0),
		cv::Point2f(src.cols - 1,src.rows - 1),
		cv::Point2f(0,src.rows - 1)
	};

	cv::Point2f dstQuad[] = {
		cv::Point2f(corners[i][0].x,corners[i][0].y),
		cv::Point2f(corners[i][1].x,corners[i][1].y),
		cv::Point2f(corners[i][2].x,corners[i][2].y),
		cv::Point2f(corners[i][3].x,corners[i][3].y)
	};

	cv::Mat warp_mat = cv::getPerspectiveTransform(srcQuad, dstQuad);
	Mat dst;
	//cv::Mat dst(image.rows, image.cols, CV_8UC3, Scalar(0, 0, 0));;
	cv::warpPerspective(src, dst, warp_mat, src.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar());
	
	Rect ROI_orect = boundingRect(corners[i]);
	dst(ROI_orect).copyTo(image(ROI_orect));
	//for (int i = 0; i < 4; i++)
		//cv::circle(dst, dstQuad[i], 5, cv::Scalar(255, 0, 255), -1, cv::LINE_AA);
	
	//imshow("Perspective_Warp", image);
	//cv::waitKey();

	return image;

}



/**
*/
int main(int argc, char *argv[]) {
	CommandLineParser parser(argc, argv, keys);
	parser.about(about);

	std::cout << argc << std::endl;
	for (int i = 0; i<argc; i++)
		std::cout << argv[i] << std::endl;

	if (argc < 2) {
		parser.printMessage();
		return 0;
	}

	int dictionaryId = parser.get<int>("d");
	bool showRejected = parser.has("r");
	bool estimatePose = parser.has("c");
	float markerLength = parser.get<float>("l");

	Ptr<aruco::DetectorParameters> detectorParams = aruco::DetectorParameters::create();
	if (parser.has("dp")) {
		bool readOk = readDetectorParameters(parser.get<string>("dp"), detectorParams);
		if (!readOk) {
			cerr << "Invalid detector parameters file" << endl;
			return 0;
		}
	}
	detectorParams->cornerRefinementMethod = aruco::CORNER_REFINE_SUBPIX; // do corner refinement in markers

	int camId = parser.get<int>("ci");

	String video;
	if (parser.has("v")) {
		video = parser.get<String>("v");
	}

	if (!parser.check()) {
		parser.printErrors();
		return 0;
	}

	Ptr<aruco::Dictionary> dictionary =
		aruco::getPredefinedDictionary(aruco::PREDEFINED_DICTIONARY_NAME(dictionaryId));

	Mat camMatrix, distCoeffs;
	if (estimatePose) {
		bool readOk = readCameraParameters(parser.get<string>("c"), camMatrix, distCoeffs);
		if (!readOk) {
			cerr << "Invalid camera file" << endl;
			return 0;
		}
	}

	VideoCapture inputVideo;
	int waitTime;
	if (!video.empty()) {
		inputVideo.open(video);
		waitTime = 0;
	}
	else {
		inputVideo.open(camId);
		waitTime = 10;
	}

	double totalTime = 0;
	int totalIterations = 0;

	while (inputVideo.grab()) {
		Mat image, imageCopy, imageMask;
		inputVideo.retrieve(image);

		double tick = (double)getTickCount();

		vector< int > ids;
		vector< vector< Point2f > > corners, rejected;
		vector< Vec3d > rvecs, tvecs;

		// detect markers and estimate pose
		aruco::detectMarkers(image, dictionary, corners, ids, detectorParams, rejected);
		//corners	检测到的标记角落的矢量。对于每个标记，提供了它的四个角（例如，std :: vector <std :: vector <cv :: Point2f>>）。
		//				对于N个检测到的标记，此数组的维数为N×4。角的顺序是顺时针的。
		if (estimatePose && ids.size() > 0)
			aruco::estimatePoseSingleMarkers(corners, markerLength, camMatrix, distCoeffs, rvecs,
				tvecs);//rvecs旋转矩阵；tvecs平移向量



		double currentTime = ((double)getTickCount() - tick) / getTickFrequency();
		totalTime += currentTime;
		totalIterations++;
		if (totalIterations % 30 == 0) {
			cout << "Detection Time = " << currentTime * 1000 << " ms "
				<< "(Mean = " << 1000 * totalTime / double(totalIterations) << " ms)" << endl;
		}

		// draw results
		//Mat imageMask;
		Mat mask(image.rows, image.cols, CV_8UC3, Scalar(0, 0, 0));
		image.copyTo(imageCopy); 
		image.copyTo(imageMask);

		if (ids.size() > 0) {
			aruco::drawDetectedMarkers(imageCopy, corners, ids);

			if (estimatePose) {
				for (unsigned int i = 0; i < ids.size(); i++) {

					mask = drawWorldImg(estimatePose, i, corners,mask);
					aruco::drawAxis(imageCopy, camMatrix, distCoeffs, rvecs[i], tvecs[i],
						markerLength * 0.5f);
				}
				imageCopy = mask + imageCopy;
				//imshow("mask", mask);
				
			}
		}

		

		if (showRejected && rejected.size() > 0)
			aruco::drawDetectedMarkers(imageCopy, rejected, noArray(), Scalar(100, 0, 255));

		imshow("out", imageCopy);
		char key = (char)waitKey(waitTime);
		if (key == 27) break;
	}

	return 0;
}

