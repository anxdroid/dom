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

cv::Mat sourceImage, preProcImage, postProcImage;
char* fileName;
int userThreshold;
int debugLevel = 1;
RNG rng(12345);

vector<Rect> *boundRect;
vector<Rect> *mergedRect;
vector<Rect> *suitableBoundRect;
vector<Vec4i> *middles;
vector<Vec4i> *tops;
vector<Vec4i> *heights;
vector<Vec4i> *widths;
vector<Vec4i> hierarchy;
std::map<int, vector<int> > *mergedHierarchy;
vector<int> *rectNotAreas;

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

int pruneRectangles() {
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

void analyzeRectangles(double minPerc = 0.01, double maxPerc = 0.3, int minWidth = 10, int minHeight = 10, bool landscape = true) {
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
		if (area < (minPerc*imgArea) || area > (maxPerc*imgArea) || width < minWidth || height < minHeight || (landscape && width > height)) {
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

void mergeRects2(int nRect = -1, int nLevel = 0) {
	if (nRect == -1) {
		for (int i = 0 ; i < mergedRect->size(); i++) {
			if (hierarchy[i][2] > 0 && hierarchy[i][3] < 0) {
				//cout << "Calling with value " << i << endl;
 				mergeRects2(i, nLevel+1);
			}
		}
	}else{
		//cout << "Called with value " << nRect << endl;
		int area = boundRect->at(nRect).area();
		int areas = 0;
		int nextRect = hierarchy[nRect][2];
		int nRects = 0;
		vector<int> childrenRect;
		while (nextRect > 0) {
			childrenRect.push_back(nextRect);
			if (hierarchy[nextRect][3] > 0) {
				mergeRects2(nextRect, nLevel+1);
			}
			//Rect rect = boundRect->at(nextRect);
			//areas += rect.area();
			//nRects++;
			nextRect = hierarchy[nextRect][0];
		}


		mergedHierarchy = new std::map<int, vector<int> >;
		int nMerged;
		do {
			nMerged = 0;
			for( int i = 0; i < childrenRect.size(); i++ ) {
				for( int j = 0; j < childrenRect.size(); j++ ) {
					//cout << i << " " << j << endl;
					if (i != j && childrenRect[i] >= 0 && childrenRect[j] >= 0) {
						Rect a = mergedRect->at(childrenRect[i]);
						Rect b = mergedRect->at(childrenRect[j]);
						if (a.area() > 0 && b.area() > 0) {
							int dist1 = abs(b.tl().x - a.br().x);
							int dist = dist1;
							int dist2 = abs(a.tl().x - b.br().x);
							if (dist2 < dist) {
								dist = dist2;
							}
							int dist3 = abs(b.tl().y - a.br().y);
							if (dist3 < dist) {
								dist = dist3;
							}
							int dist4 = abs(a.tl().y - b.br().y);
							if (dist4 < dist) {
								dist = dist4;
							}


							if (dist == 1 && dist > 0) {
								//cout << childrenRect[i] << " - " << childrenRect[j] << " dist " << dist << "(" << dist1 << "," << dist2 << "," << dist3 << "," << dist4 << ")" << endl;
								int x = min(a.tl().x, b.tl().x);
								int y = min(a.tl().y, b.tl().y);
								int w = max(a.br().x, b.br().x) - x;
								int h = max(a.br().y, b.br().y) - y;
								Rect newRect = Rect(x, y, w, h);

								int bigI = i;
								int smallI = j;
								if (b.area() > a.area()) {
									bigI = j;
									smallI = i;
								}

								mergedRect->at(childrenRect[bigI]) = Rect(x, y, w, h);
								mergedRect->at(childrenRect[smallI]) = Rect(0,0,0,0);
								childrenRect[smallI] = -1;
								//cout << "i = " << childrenRect[i] << " j = " << childrenRect[j] << endl;
								nMerged++;
							}
						}
					}
				}
			}
			//cout << "Merged " << nMerged << " rect" << endl;
		}while(nMerged > 0);


		for(int i = 0; i < childrenRect.size(); i++ ) {
			for(int j = 0; j < childrenRect.size(); j++ ) {
				if (i != j && childrenRect[i] >= 0 && childrenRect[j] >= 0) {
				if (childrenRect[i] == 194 || childrenRect[j] == 194) {
					Rect rectI = mergedRect->at(childrenRect[i]);
					Rect rectJ = mergedRect->at(childrenRect[j]);

					if (rectJ.tl().y < rectI.br().y) {
						bool overlap = false;
						if (rectJ.tl().x > rectI.tl().x && rectJ.tl().x < rectI.br().x) {
							//cout << "Overlap ! " << childrenRect[i] << " with " << childrenRect[j] << " " << rectI.tl() << "," << rectI.br() << " - " << rectJ.tl() << "," << rectJ.br() << endl;
							overlap = true;
						}
						if (rectJ.br().x > rectI.tl().x && rectJ.br().x < rectI.br().x) {
							//cout << "Overlap ! " << childrenRect[i] << " with " << childrenRect[j] << " " << rectI.tl() << "," << rectI.br() << " - " << rectJ.tl() << "," << rectJ.br() << endl;
							overlap = true;
						}
						if (overlap) {
							//mergedRect->at(childrenRect[i]).br().y = rectJ.tl().y;
							//mergedRect->at(childrenRect[j]).tl().y = rectI.br().y;

							//mergedRect->at(childrenRect[i]) = Rect(rectI.tl().x, rectI.tl().y, rectI.width, rectJ.tl().y - rectI.tl().y);
							//mergedRect->at(childrenRect[j]) = Rect(rectJ.tl().x, rectI.br().y, rectJ.width, rectI.br().y - rectJ.br().y);
							rectangle(preProcImage, mergedRect->at(childrenRect[i]).tl(), mergedRect->at(childrenRect[i]).br(), Scalar(255,255,255), 1, 8, 0 );
							//rectangle(preProcImage, mergedRect->at(childrenRect[j]).tl(), mergedRect->at(childrenRect[j]).br(), Scalar(255,255,255), 2, 8, 0 );

							//rectI = mergedRect->at(childrenRect[i]);
							//rectJ = mergedRect->at(childrenRect[j]);
							//cout << "New rects  " << rectI.tl() << "," << rectI.br() << " - " << rectJ.tl() << "," << rectJ.br() << endl;

						}
					}
				}
				}
			}
		}

		for(int i = 0; i < childrenRect.size(); i++ ) {
			//cout << i << " " << j << endl;
			if (childrenRect[i] >= 0) {
				//cout << childrenRect[i] << endl;
				Rect rect = boundRect->at(childrenRect[i]);
				areas += rect.area();
				nRects++;
			}
		}
		if (nRects > 0) {
			double ratioAreas = 100 * areas/area;
			double ratioAreasByRect = ratioAreas / nRects;
			cout << nRect << " level " << nLevel << " has " << nRects << " subrectangles with " << ratioAreas << "% coverage [avg " << ratioAreasByRect << "%]" << endl;
		}
	}
}

void mergeRects(int minDist) {
	if (debugLevel > 1) {
		cout << "Merging rectangles..." << endl;
	}
	int nMergedRecs = 0;
	int nMerged;
	mergedHierarchy = new std::map<int, vector<int> >;
	do {
		cout << "Doing..." << endl;
		nMerged = 0;
		for( int i = 0; i < mergedRect->size(); i++ ) {
			for( int j = 0; j < mergedRect->size(); j++ ) {
				//cout << i << " " << j << endl;
				if (i != j) {
					Rect a = mergedRect->at(i);
					Rect b = mergedRect->at(j);
					if (a.area() > 0 && b.area() > 0) {
						int dist1 = abs(b.tl().x - a.br().x);
						int dist = dist1;
						int dist2 = abs(a.tl().x - b.br().x);
						if (dist2 < dist) {
							dist = dist2;
						}
						int dist3 = abs(b.tl().y - a.br().y);
						if (dist3 < dist) {
							dist = dist3;
						}
						int dist4 = abs(a.tl().y - b.br().y);
						if (dist4 < dist) {
							dist = dist4;
						}


						if (dist <= minDist) {
							cout << i << " - " << j << " dist " << dist << "(" << dist1 << "," << dist2 << "," << dist3 << "," << dist4 << ")" << endl;
							int x = min(a.tl().x, b.tl().x);
							int y = min(a.tl().y, b.tl().y);
							int w = max(a.br().x, b.br().x) - x;
							int h = max(a.br().y, b.br().y) - y;
							mergedRect->at(i) = Rect(x, y, w, h);
							mergedRect->at(j) = Rect(0,0,0,0);
							if (mergedHierarchy->find(i) == mergedHierarchy->end()) {
								mergedHierarchy->insert(std::pair<int, vector<int> >(i, vector<int>()));
								//cout << "Inserting pair " << i << ", " << j  << "[" << mergedHierarchy->at(i).size() << "]" << endl;
							}
							mergedHierarchy->at(i).push_back(j);
							//(*mergedHierarchy)[i].push_back(j);
							cout << "Updating " << i << " with " << j << "[" << mergedHierarchy->at(i).size() << "]" << endl;
							//rectangle(sourceImage, mergedRect->at(i).tl(), mergedRect->at(i).br(), Scalar(255,255,255), 3, 8, 0 );
							nMerged++;
							nMergedRecs++;
						}
					}
				}
			}
		}
	}while(nMerged > 0);
}

Rect detectContainer() {
	double maxAvgArea = 0;
	int maxAvgAreaI = -1;

	int maxRectCount = 0;
	int maxRectCountI = -1;

	for( int i = 0; i < mergedRect->size(); i++ ) {
		if (mergedRect->at(i).area() > 0 && mergedHierarchy->find(i) != mergedHierarchy->end()) {
			//rectangle(sourceImage, mergedRect->at(i).tl(), mergedRect->at(i).br(), Scalar(255,255,255), 3, 8, 0 );
			cout << i << " area " << mergedRect->at(i).area() << " hierarchy: " <<  mergedHierarchy->at(i).size() << endl;
			double avgArea = 0;
			int rectCount = 0;
			for (int j = 0; j < mergedHierarchy->at(i).size(); j++) {
				int nRect = mergedHierarchy->at(i).at(j);
				if ((boundRect->at(nRect)).area() > 0) {
					cout << "\t" << nRect << " area " << (boundRect->at(nRect)).area() << " hierarchy " << hierarchy[nRect] << endl;
					avgArea += (boundRect->at(nRect)).area();
					rectCount++;
				}
			}
			avgArea /= rectCount;
			if (avgArea > maxAvgArea) {
				maxAvgArea = avgArea;
				maxAvgAreaI = i;
			}
			if (rectCount > maxRectCount) {
				maxRectCount = rectCount;
				maxRectCountI = i;
			}
		}
	}
	cout << "maxAvgAreaI " << maxAvgAreaI << " maxRectCountI " << maxRectCountI << endl;
	if ((maxAvgAreaI == maxRectCountI) && (maxAvgAreaI != -1)) {
		//rectangle(preProcImage, mergedRect->at(maxAvgAreaI).tl(), mergedRect->at(maxAvgAreaI).br(), Scalar(255,255,255), 3, 8, 0 );
		//namedWindow("preProcImage", CV_WINDOW_AUTOSIZE);
		//imshow("preProcImage", preProcImage);
		//waitKey(0);
		return mergedRect->at(maxAvgAreaI);
	}
	return Rect(0,0,0,0);
}

void detectContours(int userThreshold) {
	Mat threshold_output;
	vector<vector<Point> > contours;

	if (debugLevel > 1) {
		cout << "Detecting contours..." << endl;
	}
	threshold( preProcImage, threshold_output, userThreshold, 255, THRESH_BINARY );
	findContours( threshold_output, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );
	vector<vector<Point> > contours_poly( contours.size() );
	postProcImage = preProcImage.clone();
	boundRect = new vector<Rect>();
	mergedRect = new vector<Rect>();

	for( int i = 0; i < contours.size(); i++ ) {
		//cout << i << " has hierarchy: " << hierarchy[i] << endl;
		approxPolyDP( Mat(contours[i]), contours_poly[i], 3, true );
		Rect rect = boundingRect( Mat(contours_poly[i]));
		boundRect->push_back(rect);
		mergedRect->push_back(rect);
	}

}

int analyze(int userThreshold) {
	detectContours(userThreshold);
	rectNotAreas = new vector<int>(boundRect->size());
	mergeRects2();
	//mergeRects(6);

	//Rect container = detectContainer();
	//preProcImage = sourceImage(container);

	return 0;

/*
	analyzeRectangles(0.01, 0.6, 10, 10, false);
	if (boundRect->size() < 3) {
		return(-1);
	}
	getRectStats();
	int nRect = pruneRectangles();

	if (nRect > 0) {
		std::string result = ocrRectangles();
		cout << "Result: " << result << endl;
		std::ostringstream stringStreamFinal;
		stringStreamFinal << "results/" << result << "_" << fileName << "-" << userThreshold << ".tiff";
		std::string filenameFinal = stringStreamFinal.str();
		cout << "Writing " << filenameFinal << endl;
		imwrite( filenameFinal.c_str(), postProcImage);

		if (debugLevel > 5) {
			namedWindow("postProcImage", CV_WINDOW_AUTOSIZE);
			imshow("postProcImage", postProcImage);
			waitKey();
		}
	}
	return(0);
*/
}

void deskew(double angle) {
	//cv::bitwise_not(preProcImage, preProcImage);

	std::vector<cv::Point> points;
	cv::Mat_<uchar>::iterator it = preProcImage.begin<uchar>();
	cv::Mat_<uchar>::iterator end = preProcImage.end<uchar>();
	for (; it != end; ++it)
	if (*it)
	points.push_back(it.pos());

	cv::RotatedRect box = cv::minAreaRect(cv::Mat(points));
	cv::Mat rot_mat = cv::getRotationMatrix2D(box.center, angle, 1);
	cv::warpAffine(preProcImage, preProcImage, rot_mat, preProcImage.size(), cv::INTER_CUBIC);
}

double hough(int userThreshold) {
	Mat tmp = sourceImage.clone();
    Canny( tmp, tmp, userThreshold, 200, 3 );
    vector<Vec4i> lines;
    HoughLinesP( tmp, lines, 1, CV_PI/180, 80, 30, 10 );
    cvtColor( tmp, tmp, CV_GRAY2BGR );
    int maxLen = 0;
    int maxLenI = -1;
    for( size_t i = 0; i < lines.size(); i++ )
    {
        //line( tmp, Point(lines[i][0], lines[i][1]), Point(lines[i][2], lines[i][3]), Scalar(0,0,255), 3, 8 );
        int tmp = pow(lines[i][2]-lines[i][0], 2) + pow(lines[i][3]-lines[i][1], 2);
        double len = sqrt(tmp);
        if (debugLevel > 3) {
        	cout << i << ") " << len << endl;
        }
        if (len > maxLen) {
        	maxLen = len;
        	maxLenI = i;
        }
    }
    line( tmp, Point(lines[maxLenI][0], lines[maxLenI][1]), Point(lines[maxLenI][2], lines[maxLenI][3]), Scalar(0,0,255), 3, 8 );

    int refY = -1;
    if (lines[maxLenI][3] > lines[maxLenI][1]) {
    	refY = lines[maxLenI][3];
    }else{
    	refY = lines[maxLenI][1];
    }
    line( tmp, Point(lines[maxLenI][0], refY), Point(lines[maxLenI][2], refY), Scalar(0,255,0), 3, 8 );
    float dx21 = lines[maxLenI][2]-lines[maxLenI][0];
    float dx31 = lines[maxLenI][2]-lines[maxLenI][0];
    float dy21 = lines[maxLenI][3]-lines[maxLenI][1];
    float dy31 = 0;
    float m12 = sqrt( dx21*dx21 + dy21*dy21 );
    float m13 = sqrt( dx31*dx31 + dy31*dy31 );
    double theta = acos( (dx21*dx31 + dy21*dy31) / (m12 * m13) ) * 180 / CV_PI;
	if (debugLevel > 3) {
		namedWindow("tmp", CV_WINDOW_AUTOSIZE);
		imshow("tmp", tmp);
		waitKey(0);
	}
    return theta;
}

void preProcessImage(int userThreshold) {

	if (debugLevel > 0) {
		cout << "Detecting skew..." << endl;
	}
	double angle = hough(userThreshold);

	cvtColor( sourceImage, preProcImage, CV_BGR2GRAY );
	blur( preProcImage, preProcImage, Size(3,3) );
	if (debugLevel > 0) {
		cout << "\tSkew angle :" << angle << endl;
	}
	deskew(angle);
}

/** @function main */
int main( int argc, char** argv ) {
	if (argc < 2) {
		exit (-1);
	}
	userThreshold = atoi(argv[2]);
	debugLevel = atoi(argv[3]);

	fileName = argv[1];
	loadImage(fileName);
	preProcessImage(userThreshold);

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
	if (debugLevel > 1) {
		namedWindow("preProcImage", CV_WINDOW_AUTOSIZE);
		imshow("preProcImage", preProcImage);
		waitKey(0);
	}
	return(0);
}
