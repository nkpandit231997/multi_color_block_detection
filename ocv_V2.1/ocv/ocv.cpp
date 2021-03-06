#include "stdafx.h"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <sstream>
#include <string>
#include <iostream>
#include <vector>
#include "Block.h"

using namespace std;
using namespace cv;

Mat img1;
Mat threshold;
Mat hsv;

int cameravalue = 0;

int iLowH = 0;
int iHighH = 255;
int iLowS = 0;
int iHighS = 255;
int iLowV = 0;
int iHighV = 255;

bool trackObjects = false;
bool useMorphOps = true;

int FRAME_WIDTH = 640;
int FRAME_HEIGHT = 480;
int MIN_OBJECT_AREA = 10 * 10;
int MAX_OBJECT_AREA = 304800;
int MAX_NUM_OBJECTS = 28;

void createTrackbars() {
	createTrackbar("LowH", "Control", &iLowH, 255); //Hue (0 - 179)
	createTrackbar("HighH", "Control", &iHighH, 255);

	createTrackbar("LowS", "Control", &iLowS, 255); //Saturation (0 - 255)
	createTrackbar("HighS", "Control", &iHighS, 255);

	createTrackbar("LowV", "Control", &iLowV, 255);//Value (0 - 255)
	createTrackbar("HighV", "Control", &iHighV, 255);
}

int mysortfunction(Block i, Block j) {
	return (i.getyPos()<j.getyPos());
}


void drawObject(vector <Block> Blocks, Mat &frame) {
	//int i = 0;
	putText(frame, "Traking" , Point(15, 15), 1, 1, Scalar(255,0,0), 2);
	for (int i = 0; i < Blocks.size(); i++) {
		circle(frame, Point(Blocks.at(i).getxPos(), Blocks.at(i).getyPos()), 5, Blocks.at(i).getcolor(), 2);
		//putText(frame, "Angle=" + doubleToString(Blocks.at(i).getangle()), Point(Blocks.at(i).getxPos(), Blocks.at(i).getyPos() + 15), 1, 1, Blocks.at(i).getcolor(), 2);
		//putText(frame, intToString(Blocks.at(i).getxPos()) + "," + intToString(Blocks.at(i).getyPos()), Point(Blocks.at(i).getxPos(), Blocks.at(i).getyPos() + 30), 1, 1, Blocks.at(i).getcolor(), 2);
		putText(frame, Blocks.at(i).getname(), Point(Blocks.at(i).getxPos(), Blocks.at(i).getyPos() - 20), 1, 1, Blocks.at(i).getcolor(), 2);
	}
}
void morphOps(Mat &thresh) {

	//create structuring element that will be used to "dilate" and "erode" image.
	//the element chosen here is a 3px by 3px rectangle

	Mat erodeElement = getStructuringElement(MORPH_RECT, Size(3, 3));
	//dilate with larger element so make sure object is nicely visible
	Mat dilateElement = getStructuringElement(MORPH_RECT, Size(8, 8));

	erode(thresh, thresh, erodeElement);
	erode(thresh, thresh, erodeElement);

	dilate(thresh, thresh, dilateElement);
	dilate(thresh, thresh, dilateElement);

}

double getOrientation(vector<Point> &pts, Mat &img,Scalar color)
{
	//Construct a buffer used by the pca analysis
	Mat data_pts = Mat(pts.size(), 2, CV_64FC1);
	for (int i = 0; i < data_pts.rows; ++i)
	{
		data_pts.at<double>(i, 0) = pts[i].x;
		data_pts.at<double>(i, 1) = pts[i].y;
	}

	//Perform PCA analysis
	PCA pca_analysis(data_pts, Mat(), CV_PCA_DATA_AS_ROW);

	//Store the position of the object
	
	//Store the eigenvalues and eigenvectors
	vector<Point2d> eigen_vecs(2);
	vector<double> eigen_val(2);
	for (int i = 0; i < 2; ++i)
	{
		eigen_vecs[i] = Point2d(pca_analysis.eigenvectors.at<double>(i, 0),
			pca_analysis.eigenvectors.at<double>(i, 1));

		eigen_val[i] = pca_analysis.eigenvalues.at<double>(i);
	}

	double x1 = pca_analysis.mean.at<double>(0, 0);
	double y1 = pca_analysis.mean.at<double>(0, 1);

	double x2 = pca_analysis.mean.at<double>(0, 0)+0.02*eigen_vecs[0].x * eigen_val[0];
	double y2 = pca_analysis.mean.at<double>(0, 1)+0.02*eigen_vecs[0].y * eigen_val[0];

	double p = (y1 - y2);
	double b = (x1 - x2);
	double t;
	t = atan2(p, b) * 180 / 3.14;
	//t = abs(t);
	if (t >= 0)
	{
		t = 180 - t;
	}
	else
	{
		t = abs(t);
		t = (180 - t);
		t = -t;
	}
	//putText(img, "Angle="+doubleToString(t), pos, 3, 1, color, 2);
	return t;
}

void trackFilteredObject(Block Bl, Mat threshold, Mat &cameraFeed, vector <Block> & Blocks) {

	Mat temp;

	threshold.copyTo(temp);
	//these two vectors needed for output of findContours
	vector< vector<Point> > contours;
	vector<Vec4i> hierarchy;
	//find contours of filtered image using openCV findContours function
	findContours(temp, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);
	//use moments method to find our filtered object
	double refArea = 0;
	bool objectFound = false;
	if (hierarchy.size() > 0) {

		int numObjects = hierarchy.size();

		if (numObjects<MAX_NUM_OBJECTS) {

			for (int index = 0; index >= 0; index = hierarchy[index][0]) {

				Moments moment = moments((cv::Mat)contours[index]);
				double area = contourArea(contours[index]);
				if (area>MIN_OBJECT_AREA && area<MAX_OBJECT_AREA && area>refArea) {
					Block B;
					B.setxPos(moment.m10 / area);
					B.setyPos(moment.m01 / area);
					B.setname(Bl.getname());
					B.setcolor(Bl.getcolor());
					B.setangle(getOrientation(contours[index], cameraFeed, Bl.getcolor()));
					Blocks.push_back(B);
				}
			}
		}
		else putText(cameraFeed, "TOO MUCH NOISE! ADJUST FILTER", Point(0, 50), 1, 2, Scalar(0, 0, 255), 2);
	}
}

void detectBlock(Mat img,Block b1, vector <Block> & Blocks)
{
	cvtColor(img1, hsv, COLOR_BGR2HSV);
	inRange(hsv, b1.getHSVmin(), b1.getHSVmax(), hsv);
	morphOps(hsv);
	trackFilteredObject(b1, hsv, img1, Blocks);
}

int main(int argc, char* argv[])
{
	cameravalue = 0;
	//cin >> cameravalue;
	VideoCapture capture0(0);
	VideoCapture capture1(1);
	//capture1.read(img1);
	int mode = 1;//1- room 2- lab
	int lastmode = 1;
	namedWindow("Control");
	//createTrackbar("cam", "Control", &cameravalue, 1);
	createTrackbar("Mode", "Control", &mode, 4);
	int x = 0, y = 0;
	Mat t;
	Block block[7];

	//creating object properties
	if (mode == 1) {
		//room Blue
		block[0].setname("Blue");
		block[0].setHSVmin(Scalar(82, 99, 126));
		block[0].setHSVmax(Scalar(255, 255, 255));
		block[0].setcolor(Scalar(255, 0, 0));

		//red
		block[1].setname("Red");
		block[1].setHSVmin(Scalar(0, 75, 91));
		block[1].setHSVmax(Scalar(14, 255, 255));
		block[1].setcolor(Scalar(0, 0, 255));
		//yellow
		block[2].setname("Yellow");
		block[2].setHSVmin(Scalar(23, 123, 149));
		block[2].setHSVmax(Scalar(46, 254, 252));
		block[2].setcolor(Scalar(0, 255, 255));

	}
	else
	{

		//lab Blue
		block[0].setname("Blue");
		block[0].setHSVmin(Scalar(84, 45, 114));
		block[0].setHSVmax(Scalar(124, 134, 171));
		block[0].setcolor(Scalar(255, 0, 0));
		//red
		block[1].setname("Red");
		block[1].setHSVmin(Scalar(0, 127, 105));
		block[1].setHSVmax(Scalar(11, 156, 146));
		block[1].setcolor(Scalar(0, 0, 255));
		//yello
		block[2].setname("Yellow");
		block[2].setHSVmin(Scalar(21, 162, 137));
		block[2].setHSVmax(Scalar(40, 199, 200));
		block[2].setcolor(Scalar(0, 255, 255));
		//green
		block[3].setname("Green");
		block[3].setHSVmin(Scalar(41, 108, 137));
		block[3].setHSVmax(Scalar(59, 199, 200));
		block[3].setcolor(Scalar(0,255,0));
		//orange
		block[4].setname("Orange");
		block[4].setHSVmin(Scalar(5, 135, 153));
		block[4].setHSVmax(Scalar(23, 172, 200));
		block[4].setcolor(Scalar(0, 165, 255));
		//darkblue
		block[5].setname("DarkBlue");
		block[5].setHSVmin(Scalar(103, 92, 91));
		block[5].setHSVmax(Scalar(121, 138, 138));
		block[5].setcolor(Scalar(139, 0, 0));
		//pink
		block[6].setname("Pink");
		block[6].setHSVmin(Scalar(125, 58, 140));
		block[6].setHSVmax(Scalar(179, 93, 178));
		block[6].setcolor(Scalar(203 , 192 , 255));

	}

	while (1) {
		if (cameravalue == 0)
			capture0.read(img1);
		else
			capture1.read(img1);
		flip(img1, img1, 1);
		
		if (mode != lastmode)
		{
			if (mode == 0)
			{
				cout << "MOde is Changed To : Start\n";
				block[0].setname("Blue");
				block[0].setHSVmin(Scalar(89, 153, 117));
				block[0].setHSVmax(Scalar(137, 228, 212));
				block[0].setcolor(Scalar(255, 0, 0));
				//red
				block[1].setname("Red");
				block[1].setHSVmin(Scalar(0, 142, 171));
				block[1].setHSVmax(Scalar(7, 244, 214));
				block[1].setcolor(Scalar(0, 0, 255));
				//yello
				block[2].setname("Yellow");
				block[2].setHSVmin(Scalar(22, 172, 212));
				block[2].setHSVmax(Scalar(40, 255, 255));
				block[2].setcolor(Scalar(0, 255, 255));
				//green
				block[3].setname("Green");
				block[3].setHSVmin(Scalar(32, 165, 172));
				block[3].setHSVmax(Scalar(84, 255, 255));
				block[3].setcolor(Scalar(0, 255, 0));
				//orange
				block[4].setname("Orange");
				block[4].setHSVmin(Scalar(0, 159, 210));
				block[4].setHSVmax(Scalar(14, 244, 255));
				block[4].setcolor(Scalar(0, 165, 255));
				//darkblue
				block[5].setname("DarkBlue");
				block[5].setHSVmin(Scalar(91, 131, 99));
				block[5].setHSVmax(Scalar(122, 211, 160));
				block[5].setcolor(Scalar(139, 0, 0));
				//pink
				block[6].setname("Pink");
				block[6].setHSVmin(Scalar(110, 75, 149));
				block[6].setHSVmax(Scalar(176, 187, 220));
				block[6].setcolor(Scalar(203, 192, 255));
				
			}
			if (mode == 1)
			{
				cout << "MOde is Changed To : Middle\n";
				block[0].setname("Blue");
				block[0].setHSVmin(Scalar(89, 153, 117));
				block[0].setHSVmax(Scalar(137, 228, 212));
				block[0].setcolor(Scalar(255, 0, 0));
				//red
				block[1].setname("Red");
				block[1].setHSVmin(Scalar(0, 142, 171));
				block[1].setHSVmax(Scalar(7, 244, 214));
				block[1].setcolor(Scalar(0, 0, 255));
				//yello
				block[2].setname("Yellow");
				block[2].setHSVmin(Scalar(22, 172, 212));
				block[2].setHSVmax(Scalar(40, 255, 255));
				block[2].setcolor(Scalar(0, 255, 255));
				//green
				block[3].setname("Green");
				block[3].setHSVmin(Scalar(32, 165, 172));
				block[3].setHSVmax(Scalar(84, 255, 255));
				block[3].setcolor(Scalar(0, 255, 0));
				//orange
				block[4].setname("Orange");
				block[4].setHSVmin(Scalar(0, 159, 210));
				block[4].setHSVmax(Scalar(14, 244, 255));
				block[4].setcolor(Scalar(0, 165, 255));
				//darkblue
				block[5].setname("DarkBlue");
				block[5].setHSVmin(Scalar(91, 131, 99));
				block[5].setHSVmax(Scalar(122, 211, 160));
				block[5].setcolor(Scalar(139, 0, 0));
				//pink
				block[6].setname("Pink");
				block[6].setHSVmin(Scalar(110, 75, 149));
				block[6].setHSVmax(Scalar(176, 187, 220));
				block[6].setcolor(Scalar(203, 192, 255));
			}
			if (mode == 2)
			{
				cout << "MOde is Changed To : Final\n";
				block[0].setname("Blue");
				block[0].setHSVmin(Scalar(89, 153, 117));
				block[0].setHSVmax(Scalar(137, 228, 212));
				block[0].setcolor(Scalar(255, 0, 0));
				//red
				block[1].setname("Red");
				block[1].setHSVmin(Scalar(0, 142, 171));
				block[1].setHSVmax(Scalar(7, 244, 214));
				block[1].setcolor(Scalar(0, 0, 255));
				//yello
				block[2].setname("Yellow");
				block[2].setHSVmin(Scalar(22, 172, 212));
				block[2].setHSVmax(Scalar(40, 255, 255));
				block[2].setcolor(Scalar(0, 255, 255));
				//green
				block[3].setname("Green");
				block[3].setHSVmin(Scalar(32, 165, 172));
				block[3].setHSVmax(Scalar(84, 255, 255));
				block[3].setcolor(Scalar(0, 255, 0));
				//orange
				block[4].setname("Orange");
				block[4].setHSVmin(Scalar(0, 159, 210));
				block[4].setHSVmax(Scalar(14, 244, 255));
				block[4].setcolor(Scalar(0, 165, 255));
				//darkblue
				block[5].setname("DarkBlue");
				block[5].setHSVmin(Scalar(91, 131, 99));
				block[5].setHSVmax(Scalar(122, 211, 160));
				block[5].setcolor(Scalar(139, 0, 0));
				//pink
				block[6].setname("Pink");
				block[6].setHSVmin(Scalar(110, 75, 149));
				block[6].setHSVmax(Scalar(176, 187, 220));
				block[6].setcolor(Scalar(203, 192, 255));
			}
			if (mode == 3)
			{
				cout << "MOde is Changed To : Stack\n";
				block[0].setname("Blue");
				block[0].setHSVmin(Scalar(89, 153, 117));
				block[0].setHSVmax(Scalar(137, 228, 212));
				block[0].setcolor(Scalar(255, 0, 0));
				//red
				block[1].setname("Red");
				block[1].setHSVmin(Scalar(0, 142, 171));
				block[1].setHSVmax(Scalar(7, 244, 214));
				block[1].setcolor(Scalar(0, 0, 255));
				//yello
				block[2].setname("Yellow");
				block[2].setHSVmin(Scalar(22, 172, 212));
				block[2].setHSVmax(Scalar(40, 255, 255));
				block[2].setcolor(Scalar(0, 255, 255));
				//green
				block[3].setname("Green");
				block[3].setHSVmin(Scalar(32, 165, 172));
				block[3].setHSVmax(Scalar(84, 255, 255));
				block[3].setcolor(Scalar(0, 255, 0));
				//orange
				block[4].setname("Orange");
				block[4].setHSVmin(Scalar(0, 159, 210));
				block[4].setHSVmax(Scalar(14, 244, 255));
				block[4].setcolor(Scalar(0, 165, 255));
				//darkblue
				block[5].setname("DarkBlue");
				block[5].setHSVmin(Scalar(91, 131, 99));
				block[5].setHSVmax(Scalar(122, 211, 160));
				block[5].setcolor(Scalar(139, 0, 0));
				//pink
				block[6].setname("Pink");
				block[6].setHSVmin(Scalar(110, 75, 149));
				block[6].setHSVmax(Scalar(176, 187, 220));
				block[6].setcolor(Scalar(203, 192, 255));
			}
			if (mode == 4)
			{
				cout << "MOde is Changed To : Tracking_Color\n";
			}
			lastmode = mode;
		}
		if (mode == 4)
		{
			createTrackbars();
			cvtColor(img1, hsv, COLOR_BGR2HSV); //Convert the captured frame from BGR to HSV
			inRange(hsv, Scalar(iLowH, iLowS, iLowV), Scalar(iHighH, iHighS, iHighV), hsv); //Threshold the image
			imshow("image", img1);
			imshow("hsv", hsv);
		}
		else
		{
			if (mode == 0)
			{
				Rect cropped = Rect(160, 120, 320, 240);
				img1 = img1(cropped);
				pyrUp(img1, img1, Size(img1.cols * 2, img1.rows * 2));
			}

			medianBlur(img1, img1, 5);
			
			vector <Block> Blocks;
			
			for(int i=0;i<7;i++)
				detectBlock(img1, block[i], Blocks);

			drawObject(Blocks, img1);
			sort(Blocks.begin(), Blocks.end(), mysortfunction);

			if (Blocks.size())
				cout << Blocks.at(0).getname() << " x = " << Blocks.at(0).getxPos() << " y = " << Blocks.at(0).getyPos()<<"\n";

			imshow("image", img1);
		}
		if (waitKey(30) == 27) //wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop
		{
			cout << "esc key is pressed by user" << endl;
			break;
		}
	}
	return 0;
}

