//============================================================================
// Name        : Domotica.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "domotica.h"

using namespace cv;
using namespace std;

cv::Mat sourceImage, preProcImage, postProcImage, contImage;
char* fileName;
int userThreshold;
int debugLevel = 3;
RNG rng(12345);

vector<Rect> *boundRect;
vector<Rect> *suitableBoundRect;
vector<Vec4i> *middles;
vector<Vec4i> *tops;
vector<Vec4i> *heights;
vector<Vec4i> *widths;

std::map<int, vector<int> > *rectStats;
vector<int> *rectsInRect;

vector<int> *ocrRect;

void loadImage(char* fileName) {
	if (debugLevel > 0) {
		cout << "==================================================" << endl;
		cout << "Loading image " << fileName << "..." << endl;
		cout << "==================================================" << endl;
	}
	sourceImage = imread(fileName, 1 );
}

void preProcessImage() {
	if (debugLevel > 0) {
		cout << "Converting to gray and blurring..." << endl;
	}
	cvtColor( sourceImage, preProcImage, CV_BGR2GRAY );
	blur( preProcImage, preProcImage, Size(3,3) );
}

vector<Vec4i>* sortValues(vector<Vec4i> *store, int nRect) {
	vector<Vec4i> *sortedStore = new vector<Vec4i>();
	while (store->size() > 0) {
		int maxOcc = 0;
		int maxOccI = -1;
		for( int j = 0; j< store->size(); j++ ) {
			if (store->at(j)[2] >= maxOcc) {
				maxOcc = store->at(j)[2];
				maxOccI = j;
			}
		}
		if (debugLevel > 2 && sortedStore->size() == 0) {
			double perc = 100 * maxOcc / nRect;
			int gap = store->at(maxOccI)[1] - store->at(maxOccI)[0];
			cout << "Max occurrence: " << maxOcc << " " << perc << "% of all rectangles [" << store->at(maxOccI)[0] << " - " << store->at(maxOccI)[1] << " gap " << gap << "] @ " << maxOccI << endl;
		}
		int j = maxOccI;
		sortedStore->push_back(store->at(j));
		store->erase (store->begin()+j);
		if (debugLevel > 3) {
			cout << "Sorted container @ " << sortedStore <<  " [" << sortedStore->size() << " location/s]" << endl;
			cout << "Original container @ " << store <<  " [" << store->size() << " location/s]" << endl;
		}
	}
	delete store;
	return sortedStore;

}

void storeValue(vector<Vec4i> *store, double lower, double higher, int value, int maxGap, int debugLevel = 0) {
	bool found = false;
	bool outOfGap = true;
	for( int j = 0; j< store->size(); j++ ) {
		if (value > lower*store->at(j)[0] && value <= higher*store->at(j)[1]) {
			//cout << " Location " << j << ": [" << store->at(j)[0] << " - " << value << " - " <<  store->at(j)[1] << "] " << store->at(j)[2] << endl;
			if (value <= store->at(j)[0]) {
				int gap = store->at(j)[1] - value;
				if (gap <= maxGap) {
					store->at(j) = Vec4i(value, store->at(j)[1], store->at(j)[2]+1);
					outOfGap = false;
				}else{
					outOfGap = true;
					//int newLower = store->at(j)[1] - maxGap;
					//store->at(j) = Vec4i(newLower, store->at(j)[1], store->at(j)[2]+1);
				}
			}
			if (value > store->at(j)[1]) {
				int gap = value - store->at(j)[0];
				if (gap <= maxGap) {
					store->at(j) = Vec4i(store->at(j)[0], value, store->at(j)[2]+1);
					outOfGap = false;
				}else{
					outOfGap = true;
					//int newUpper = store->at(j)[0] + maxGap;
					//store->at(j) = Vec4i(store->at(j)[0], newUpper, store->at(j)[2]+1);
				}
			}
			if (!outOfGap) {
				if (debugLevel > 2) {
					cout << "Location " << j << " " << store->at(j)[0] << " < " << value << " < " <<  store->at(j)[1] << " [" << store->at(j)[2] << " rectangle/s]" << endl;
				}
				found = true;
				break;
			}
		}
	}
	if (!found) {
		store->push_back(Vec4i(value, value, 1));
		if (debugLevel > 2) {
			cout << "New value " << value << " [" << store->size() << " location/s]" << endl;
		}
	}
}

void pruneRectangles() {
	// eliminate rectangles inside rectangles
	rectsInRect = new vector<int> ;
	for (int i = 0; i < boundRect->size(); i++) {
		for (int j = 0; j < boundRect->size(); j++) {
			if (i != j) {
				Point tlIn = boundRect->at(j).tl();
				Point tlIn2 = boundRect->at(j).tl();
				Point brIn = boundRect->at(j).br();
				if (debugLevel > 3) {
					cout << i << " vs " << j << endl;
				}

				tlIn2.y = boundRect->at(j).br().y;
				if (boundRect->at(i).contains(tlIn) ||
						boundRect->at(i).contains(tlIn2) ||
						boundRect->at(i).contains(brIn)) {
					if (debugLevel > 2) {
						cout << i << " contains " << j << endl;
					}
					rectsInRect->push_back(j);
				}
			}
		}
	}

	/*
	rectsInRect = new vector<int> ;
	typedef std::map<int, vector<int> >::iterator it_type;
	for(it_type iterator = rectStats->begin(); iterator != rectStats->end(); iterator++) {
		vector<int> rects = iterator->second;
		for(int rectOut = 0; rectOut < rects.size(); rectOut++) {
			for(int rectIn = 0; rectIn < rects.size(); rectIn++) {
				int rectIndexOut = rects[rectOut];
				int rectIndexIn = rects[rectIn];
				if (rectIndexOut != rectIndexIn) {
					//cout << "Confront " << rectIndexOut << " with " << rectIndexIn << endl;
					Point tlOut = boundRect->at(rectIndexOut).tl();
					Point tlIn = boundRect->at(rectIndexIn).tl();
					Point tlIn2 = boundRect->at(rectIndexIn).tl();
					Point brIn = boundRect->at(rectIndexIn).br();

					tlIn2.y = boundRect->at(rectIndexIn).br().y;
					if (boundRect->at(rectIndexOut).contains(tlIn) ||
							boundRect->at(rectIndexOut).contains(tlIn2) ||
							boundRect->at(rectIndexOut).contains(brIn)) {
						if (debugLevel > 3) {
							cout << rectIndexOut << " contains " << rectIndexIn << endl;
						}
						rectsInRect->push_back(rectIndexIn);
					}
				}
			}
		}
	}
	*/
}

int selectNumberRectangles() {
	typedef std::map<int, vector<int> >::iterator it_type;
	int nRect = 0;
	ocrRect = new vector<int>;
	double fontScale = 0.5;
	for(it_type iterator = rectStats->begin(); iterator != rectStats->end() && nRect < 9; iterator++) {
		Scalar color = Scalar( 255, 255, 255 );
		for( int i = 0; i< iterator->second.size(); i++ ) {
			if (std::find(ocrRect->begin(), ocrRect->end(), iterator->second[i]) == ocrRect->end()) {
				nRect++;
				if (std::find(rectsInRect->begin(), rectsInRect->end(), iterator->second[i]) == rectsInRect->end()) {
					ocrRect->push_back(iterator->second[i]);
					rectangle(postProcImage, boundRect->at(iterator->second[i]).tl(), boundRect->at(iterator->second[i]).br(), Scalar(255,255,255), 2, 8, 0 );
					if (debugLevel > 3) {
						cout << iterator->second[i] << " not contained. Rectinrect [" << rectsInRect->size() << "]" << endl;
						cout << boundRect->at(iterator->second[i]).tl().x << "," << boundRect->at(iterator->second[i]).tl().y << endl;
						cout << boundRect->at(iterator->second[i]).br().x << "," << boundRect->at(iterator->second[i]).br().y << endl;
						cout << "Val " << iterator->first << " rect " << iterator->second[i] << " [" << nRect << "]" << endl;
						Point textPos = boundRect->at(iterator->second[i]).br();
						textPos.y += 12;
						textPos.x =boundRect->at(iterator->second[i]).tl().x;
						std::ostringstream stringStream;
						stringStream << iterator->second[i];
						std::string text = stringStream.str();
						int fontFace = FONT_HERSHEY_SCRIPT_SIMPLEX;
						putText(postProcImage, text, textPos, fontFace, fontScale, Scalar(255,255,255), 1, 8);
					}
				}
			}
		}
	}
	return nRect;
}

string ocrRectangles() {
	std::map<int, int> ocrFinal;
	for (int i = 0; i < ocrRect->size(); i++) {
		Rect origRect = boundRect->at(ocrRect->at(i));
		int countOcc = 0;
		std::map<int, int> ocrValues;
		for (int horizOffset = -2; horizOffset <= 2; horizOffset+=2) {
			for (int vertOffset = 0; vertOffset <= 0; vertOffset+=2) {
				countOcc++;
				int x1 = origRect.tl().x;
				int y1 = origRect.tl().y;
				int x2 = origRect.br().x;
				int y2 = origRect.br().y;
				if (horizOffset+x1 > 0) {
					x1 += horizOffset - 0;
					x2 += horizOffset + 0;
				}
				if (x1 > postProcImage.cols) {
					x1 = postProcImage.cols;
				}
				if (x2 > postProcImage.cols) {
					x2 = postProcImage.cols;
				}
				if (x1 < 0) {
					x1 = 0;
				}
				if (x2 < 0) {
					x2 = 0;
				}

				if (vertOffset+y1 > 0) {
					y1 += vertOffset + 0;
					y2 += vertOffset - 0;
				}
				if (y1 > postProcImage.rows) {
					y1 = postProcImage.rows;
				}
				if (y2 > postProcImage.rows) {
					y2 = postProcImage.rows;
				}
				if (y1 < 0) {
					y1 = 0;
				}
				if (y2 < 0) {
					y2 = 0;
				}

				Rect rectMoved(x1, y1, x2-x1, y2-y1);
				Mat imageRoi = preProcImage(rectMoved);



			    // Pass it to Tesseract API
				tesseract::TessBaseAPI tess;
				tess.Init(NULL, "eng", tesseract::OEM_DEFAULT);
				tess.SetVariable("tessedit_char_whitelist", "0123456789");
				tess.SetPageSegMode(tesseract::PSM_SINGLE_CHAR);
				//tess.SetImage((uchar*)imageRoi.data, imageRoi.cols, imageRoi.rows, 1, imageRoi.cols);
				tess.TesseractRect( imageRoi.data, 1, imageRoi.step1(), 0, 0, imageRoi.cols, imageRoi.rows);
				char* out = tess.GetUTF8Text();
				//std::cout << ocrRect->at(i) << " tesseract output: " << out << std::endl;
				if (strlen(out) > 0) {
					int num = atoi(out);
					if (ocrValues.find( num ) != ocrValues.end()) {
						ocrValues[num]++;
					}else{
						ocrValues[num] = 1;
					}
					if (debugLevel > 3) {
						cout << "Rect " << ocrRect->at(i) << " ocr " << num << " [" << ocrValues[num] << "] " << horizOffset << "x" << vertOffset << endl;
					}
				}
				delete[] out;
			}
		}
		if (debugLevel > 3) {
			cout << "\tAnalyzed " << countOcc << " rectangles" << endl;
		}

		int maxVal = -1;
		int maxValI = -1;
		typedef std::map<int, int>::iterator it_type2;
		for(it_type2 iterator2 = ocrValues.begin(); iterator2 != ocrValues.end(); iterator2++) {
			//cout << iterator->second[i] << " => " << iterator2->first << ": [" << iterator2->second << "]" << endl;
			if (iterator2->second > maxVal) {
				maxVal = iterator2->second;
				maxValI = iterator2->first;
			}
		}
		if (debugLevel > 0) {
			cout << ocrRect->at(i) << " Most frequent scan " << maxValI << " [" << maxVal << "]" << endl;
		}
		int maxSecVal = -1;
		int maxSecValI = -1;
		for(it_type2 iterator2 = ocrValues.begin(); iterator2 != ocrValues.end(); iterator2++) {
			if (iterator2->second > maxSecVal && iterator2->second < maxVal) {
				maxSecVal = iterator2->second;
				maxSecValI = iterator2->first;
			}
		}

		double perc = 100 * maxVal / countOcc;
		std::ostringstream stringStream5;
		stringStream5 << ocrRect->at(i) << " " << maxValI << " [" << perc << "%]";
		if (maxSecVal > 0) {
			perc = 100 * maxSecVal / countOcc;
			stringStream5 << " " << maxSecValI << " [" << perc << "%]";
		}
		std::string ocrOutImg = stringStream5.str();

		Point tl = boundRect->at(ocrRect->at(i)).tl();
		Point br = boundRect->at(ocrRect->at(i)).br();
		cout << "Rect " << ocrRect->at(i) << " @ " << tl.x << "," << tl.y << "-" << br.x << "," << br.y << endl;
		br.y += 18;
		br.x = tl.x;
		cout << "\t" << ocrOutImg << " @ " << br.x << "," << br.y << endl;
		int fontFace = FONT_HERSHEY_SCRIPT_SIMPLEX;
		putText(postProcImage, ocrOutImg, br, fontFace, 0.5, Scalar::all(255), 1, 8);
		ocrFinal[tl.x] = maxValI;
	}

	std::ostringstream stringStream6;

	for (std::map<int, int>::iterator i = ocrFinal.begin(); i != ocrFinal.end(); i++) {
		//cout << i->first << " " << i->second << endl;
		if (i->second < 0) {
			stringStream6 << "X";
		}else{
			stringStream6 << i->second;
		}
	}
	//cout << stringStream6.str();
	return stringStream6.str();
}

void getRectStats() {
	rectStats = new std::map<int, vector<int> >;
	vector<Rect> myBoundRect(*boundRect);

	for( int medianI = 0; medianI < middles->size(); medianI++ ) {
		for( int topI = 0; topI < tops->size(); topI++ ) {
			for( int heightI = 0; heightI < heights->size(); heightI++ ) {
				for( int widthI = 0; widthI < widths->size(); widthI++ ) {
					for( int i = 0; i< myBoundRect.size(); i++ ) {
						Point tl = myBoundRect.at(i).tl();
						Point br = myBoundRect.at(i).br();
						int val = medianI * 1000 + topI * 100 + heightI * 10 + widthI;

						if (debugLevel > 3) {
							cout << i << " vs val " << val << endl;
						}
						int median = tl.y + ((br.y - tl.y)/2);
						if (median >= middles->at(medianI)[0] && median <= middles->at(medianI)[1]) {
							int top = tl.y;
							if (top >= tops->at(topI)[0] && top <= tops->at(topI)[1]) {
								int height = br.y - tl.y;
								if (height >= heights->at(heightI)[0] && height <= heights->at(heightI)[1]) {
									int width = br.x - tl.x;
									if (width >= widths->at(widthI)[0] && width <= widths->at(widthI)[1]) {
										if (debugLevel > 3) {
											cout << i << " => " << val << endl;
										}
										if (rectStats->find(val) == rectStats->end()) {
											if (debugLevel > 3) {
												cout << "Creating " << val << endl;
											}
											vector<int> rects;
											rectStats->insert ( std::pair<int,vector<int> >(val, rects) );
										}
										rectStats->at(val).push_back(i);
										//myBoundRect.erase(myBoundRect.begin()+i);
									}
								}
							}
						}

					}
				}
			}
		}
	}
	std::map<int, vector<int> > *sortedRectStats = new std::map<int, vector<int> >;
	int pos = 0;
	while (rectStats->size() > 0) {
		int maxOcc = 0;
		int maxOccI = -1;
		typedef std::map<int, vector<int> >::iterator it_type;
		for(it_type iterator = rectStats->begin(); iterator != rectStats->end(); iterator++) {
			if ((int)iterator->second.size() >= maxOcc) {
				maxOcc = (int)iterator->second.size();
				maxOccI = (int)iterator->first;
			}
		}
		sortedRectStats->insert ( std::pair<int,vector<int> >(pos, rectStats->at(maxOccI)) );
		if (debugLevel > 3) {
			cout << "Most Frequent val: " << maxOccI << "[" << rectStats->at(maxOccI).size() << "]" << endl;
		}
		rectStats->erase(maxOccI);
		pos++;
	}
	delete rectStats;
	rectStats = sortedRectStats;
}

void analyzeRectangles() {
	if (debugLevel > 0) {
		cout << "Analyzing rectangles... [" << boundRect->size() << "]" << endl;
	}
	// removing rectangles with area lower than 1% of image area and dimensions lower than 10px
	suitableBoundRect = new vector<Rect>();
	for( int i = 0; i< boundRect->size(); i++ ) {
		Point tl = boundRect->at(i).tl();
		Point br = boundRect->at(i).br();

		int height = (br.y - tl.y);
		int width = (br.x - tl.x);
		int area = width * height;
		int imgArea = postProcImage.rows * postProcImage.cols;
		if (area < (0.01*imgArea) || area > (0.3*imgArea) || width < 10 || height < 10 || width > height) {
			//boundRect->erase(boundRect->begin()+i);
			if (debugLevel > 3) {
				cout << "Removing rect " << i << " for area = " << area << " w " << width << " h " << height << endl;
			}
		}else{
			suitableBoundRect->push_back(boundRect->at(i));
		}
	}
	if (debugLevel > 0) {
		cout << "Analyzing rectangles... [" << suitableBoundRect->size() << "]" << endl;
	}
	delete boundRect;
	boundRect = suitableBoundRect;
	if (suitableBoundRect->size() < 2) {
		return;
	}

	for( int i = 0; i< boundRect->size(); i++ ) {
		Point tl = boundRect->at(i).tl();
		Point br = boundRect->at(i).br();
		int median = tl.y + ((br.y - tl.y)/2);
		if (debugLevel > 3) {
			cout << "Rect " << i << " Middle " << median << endl;
		}
		if (median > 0) {
			if (debugLevel > 3) {
				cout << "Middle line: " << median << endl;
			}
			if (middles == NULL) {
				middles = new vector<Vec4i>;
			}else{
				middles->empty();
			}
			if (debugLevel > 3) {
				cout << "Container @ " << middles <<  " [" << middles->size() << " location/s]" << endl;
			}
			storeValue(middles, 0.9, 1.1, median, 100, 0);
			if (debugLevel > 3) {
				cout << "Container @ " << middles <<  " [" << middles->size() << " location/s]" << endl;
				cout << "==================================================" << endl;
			}

			if (tops == NULL) {
				tops = new vector<Vec4i>;
			}else{
				tops->empty();
			}
			int top = tl.y;
			if (debugLevel > 3) {
				cout << "Rect " << i << " Top " << top << endl;
			}
			storeValue(tops, 0.8, 1.2, top, 100, 0);

			if (heights == NULL) {
				heights = new vector<Vec4i>;
			}else{
				heights->empty();
			}
			int height = br.y - tl.y;
			if (debugLevel > 3) {
				cout << "Rect " << i << " Height " << height << endl;
			}
			storeValue(heights, 0.9, 1.1, height, 40, 0);

			if (widths == NULL) {
				widths = new vector<Vec4i>;
			}else{
				widths->empty();
			}
			int width = br.x - tl.x;
			if (debugLevel > 3) {
				cout << "Rect " << i << " Width " << width << endl;
			}
			storeValue(widths, 0.5, 1.5, width, 50, 0);
		}
	}
	if (middles->size() > 0) {
		if (debugLevel > 3) {
			cout << "Sorting middles..." << endl;
		}
		middles = sortValues(middles, boundRect->size());
		if (debugLevel > 3) {
			cout << "Container @ " << middles <<  " [" << middles->size() << " location/s]" << endl;
			cout << "Best middle " << middles->at(0)[0] << " - " << middles->at(0)[1] << " [" << middles->at(0)[2] << " rectangle/s]" << endl;
			cout << "==================================================" << endl;
		}
		if (debugLevel > 3) {
			cout << "Sorting tops..." << endl;
		}
		tops = sortValues(tops, boundRect->size());
		if (debugLevel > 3) {
			cout << "Sorting heights..." << endl;
		}
		heights = sortValues(heights, boundRect->size());
		if (debugLevel > 3) {
			cout << "Sorting widths..." << endl;
		}
		widths = sortValues(widths, boundRect->size());
	}

	if (debugLevel > 3) {
		int showLocation = 1;
		cout << "Middle " << showLocation << " " << middles->at(showLocation)[0] << " - " << middles->at(showLocation)[1] << " [" << middles->at(showLocation)[2] << " rectangle/s]" << endl;
		line(postProcImage, Point(0, middles->at(showLocation)[0]), Point(postProcImage.cols, middles->at(showLocation)[0]), Scalar( 255, 0, 0), 2, CV_AA);
		line(postProcImage, Point(0, middles->at(showLocation)[1]), Point(postProcImage.cols, middles->at(showLocation)[1]), Scalar( 100, 0, 0 ), 2, CV_AA);

		showLocation = 0;
		cout << "Top " << showLocation << " " << tops->at(showLocation)[0] << " - " << tops->at(showLocation)[1] << " [" << tops->at(showLocation)[2] << " rectangle/s]" << endl;
		//line(postProcImage, Point(0, tops->at(showLocation)[0]), Point(postProcImage.cols, tops->at(showLocation)[0]), Scalar( 255, 0, 0), 2, CV_AA);
		//line(postProcImage, Point(0, tops->at(showLocation)[1]), Point(postProcImage.cols, tops->at(showLocation)[1]), Scalar( 100, 0, 0 ), 2, CV_AA);
	}

}

void detectContours(int userThreshold) {
	Mat threshold_output;
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;

	if (debugLevel > 1) {
		cout << "Detecting contours..." << endl;
	}
	threshold( preProcImage, threshold_output, userThreshold, 255, THRESH_BINARY );
	findContours( threshold_output, contours, hierarchy, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );

	if (debugLevel > 1) {
		cout << "Drawing bounding rectangles..." << endl;
	}
	vector<vector<Point> > contours_poly( contours.size() );
	contImage = Mat::zeros( threshold_output.size(), CV_8UC3 );
	postProcImage = preProcImage.clone();

	boundRect = new vector<Rect>(contours.size());
	for( int i = 0; i < contours.size(); i++ ) {
		approxPolyDP( Mat(contours[i]), contours_poly[i], 3, true );
		boundRect->push_back(boundingRect( Mat(contours_poly[i])));
		Scalar color = Scalar( rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255) );
		rectangle( contImage, boundRect->at(i).tl(), boundRect->at(i).br(), color, 2, 8, 0 );
		rectangle( postProcImage, boundRect->at(i).tl(), boundRect->at(i).br(), color, 2, 8, 0 );
	}

	if (debugLevel > 1) {
		cout << "Drawing image..." << endl;
	}
}

int analyze(int userThreshold) {
	detectContours(userThreshold);
	analyzeRectangles();
	if (boundRect->size() < 3) {
		return(-1);
	}
	getRectStats();
	pruneRectangles();
	int nRect = selectNumberRectangles();

	if (nRect > 0) {
		std::string result = ocrRectangles();
		std::ostringstream stringStreamFinal;
		stringStreamFinal << "results/" << result << "_" << fileName << "-" << userThreshold << ".tiff";
		std::string filenameFinal = stringStreamFinal.str();
		cout << "Writing " << filenameFinal << endl;
		imwrite( filenameFinal.c_str(), postProcImage);

		if (debugLevel > 3) {
			namedWindow("postProcImage", CV_WINDOW_AUTOSIZE);
			imshow("postProcImage", postProcImage);
			waitKey();
		}
	}
	return(0);
}

/** @function main */
int main( int argc, char** argv ) {
	if (argc < 2) {
		exit (-1);
	}
	fileName = argv[1];
	loadImage(fileName);
	preProcessImage();
	userThreshold = atoi(argv[2]);
	if (userThreshold == -1) {
		for (int t = 0; t <= 100; t+=10) {
			if (debugLevel > 0) {
				cout << "Setting threshold to " << t << endl;
			}
			analyze(t);
		}
	}else{
		if (debugLevel > 0) {
			cout << "Setting threshold to " << userThreshold << endl;
		}
		analyze(userThreshold);
	}
	return(0);
}
