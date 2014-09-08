#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include <fstream>
#include <string>
#include "opencv2/core/core.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/nonfree/nonfree.hpp"
#include "RobustMatcher.hpp"

using namespace cv;

void readme();

/** @function main */
int main( int argc, char** argv ) {
	RobustMatcher *r = new RobustMatcher();
	Mat match = imread("tests/matchh.png");
	cout << match.rows << "x" << match.cols << endl;
	Mat orig = imread("tests/orig.jpg");
	cout << orig.rows << "x" << orig.cols << endl;
	std::vector<cv::DMatch> matches;
	std::vector<cv::KeyPoint> keypoints1;
	std::vector<cv::KeyPoint> keypoints2;
	r->match(match, orig, matches, keypoints1, keypoints2);

	std::cout << "Number of good matching " << (int)matches.size() << "\n" << endl;

	if ((int)matches.size() > 5 ){
		cout << "Good matching !" << endl;
	}
	//-- Localize the object
	std::vector<Point2f> obj;
	std::vector<Point2f> scene;

	for( int i = 0; i < matches.size(); i++ )
	{
		//-- Get the keypoints from the good matches
		obj.push_back( keypoints1[ matches[i].queryIdx ].pt );
		scene.push_back( keypoints2[matches[i].trainIdx ].pt );
	}
	cv::Mat arrayRansac;
	std::vector<uchar> inliers(obj.size(),0);
	Mat H = findHomography( obj, scene, CV_RANSAC,3,inliers);


	//-- Get the corners from the image_1 ( the object to be "detected" )
	std::vector<Point2f> obj_corners(4);
	obj_corners[0] = cvPoint(0,0); obj_corners[1] = cvPoint( match.cols, 0 );
	obj_corners[2] = cvPoint( match.cols, match.rows ); obj_corners[3] = cvPoint( 0, match.rows );
	std::vector<Point2f> scene_corners(4);


	perspectiveTransform( obj_corners, scene_corners, H);


	//-- Draw lines between the corners (the mapped object in the scene - image_2 )
	line( orig, scene_corners[0] + Point2f( match.cols, 0), scene_corners[1] + Point2f( match.cols, 0), Scalar(0, 255, 0), 4 );
	line( orig, scene_corners[1] + Point2f( match.cols, 0), scene_corners[2] + Point2f( match.cols, 0), Scalar( 0, 255, 0), 4 );
	line( orig, scene_corners[2] + Point2f( match.cols, 0), scene_corners[3] + Point2f( match.cols, 0), Scalar( 0, 255, 0), 4 );
	line( orig, scene_corners[3] + Point2f( match.cols, 0), scene_corners[0] + Point2f( match.cols, 0), Scalar( 0, 255, 0), 4 );
	namedWindow( "Display window", WINDOW_AUTOSIZE );// Create a window for display.
	imshow( "Display window", orig );                   // Show our image inside it.

	waitKey(0);
	return 0;
 }

  /** @function readme */
  void readme()
  { std::cout << " Usage: ./SURF_descriptor <img1> <img2>" << std::endl; }
