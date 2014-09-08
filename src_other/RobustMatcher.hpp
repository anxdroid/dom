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
#include "opencv2/nonfree/features2d.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace cv;
using namespace std;

class RobustMatcher {

	cv::SurfFeatureDetector* detector;
	cv::SurfDescriptorExtractor* extractor;
	cv::BFMatcher* matcher;
	double ratio;
	bool refineF;
	double confidence;
	double distance;

public:
	RobustMatcher();
	int ratioTest(std::vector<std::vector<cv::DMatch> > &matches);
	void symmetryTest(
	        const std::vector<std::vector<cv::DMatch> >& matches1,
	        const std::vector<std::vector<cv::DMatch> >& matches2,
	        std::vector<cv::DMatch>& symMatches);
	cv::Mat ransacTest(const std::vector<cv::DMatch>& matches,const std::vector<cv::KeyPoint>& keypoints1,
	            const std::vector<cv::KeyPoint>& keypoints2,
	            std::vector<cv::DMatch>& outMatches);
	cv::Mat match(cv::Mat& image1,
	        cv::Mat& image2, // input images
	       // output matches and keypoints
	       std::vector<cv::DMatch>& matches,
	       std::vector<cv::KeyPoint>& keypoints1,
	       std::vector<cv::KeyPoint>& keypoints2);
};
