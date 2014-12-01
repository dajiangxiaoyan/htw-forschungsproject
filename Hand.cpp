// Forschungsproject (teil 2) at HTW Berlin
// Recognition of short-time micro-gestures from a single-PoV video stream
// (c) Nikita "Xonxt" Kovalenko, 2013-2014, Berlin

#include "stdafx.h"
#include "Hand.h"

Hand::Hand() {
	// new hand created!
	Parameters.moveAngle = NAN;
	Parameters.moveSpeed = -1;

	// YCrCb thresholds
	YCbCr.Y_MIN = 0;
	YCbCr.Y_MAX = 255;
	YCbCr.Cr_MIN = 133;
	YCbCr.Cr_MAX = 173;
	YCbCr.Cb_MIN = 77;
	YCbCr.Cb_MAX = 127;

	// HSV thresholds
	HSV.H_MIN = 0;
	HSV.H_MAX = 15;
	HSV.S_MIN = 76;
	HSV.S_MAX = 118;
	HSV.V_MIN = 0;
	HSV.V_MAX = 255;
}

Hand::~Hand() {
	//Tracker.hist.release();
	Tracker.KalmanTracker.KF.~KalmanFilter();
}

// add a new measurement for the Kalman tracker
void Hand::addKalmanMeasurement(const cv::Point coords) {
	// predict the location with KalmanFilter
	cv::Mat prediction = Tracker.KalmanTracker.KF.predict();

	Tracker.KalmanTracker.measurement(0) = coords.x;
	Tracker.KalmanTracker.measurement(1) = coords.y;
}

// get the latest predicted position by the Kalman
cv::Point Hand::getLatestKalmanPrediction() {
	// estimate the new position of the hand
	cv::Mat estimated = Tracker.KalmanTracker.KF.correct(Tracker.KalmanTracker.measurement);
	cv::Point statePt(estimated.at<float>(0), estimated.at<float>(1));

	Tracker.kalmTrack.push_back(statePt);

	return statePt;
}

void Hand::assignNewLocation(const Hand newHand) {
	handBox = newHand.handBox;
	Tracker.trackWindow = handBox.boundingRect();

	detectionBox = newHand.detectionBox;

	addKalmanMeasurement(cv::Point(newHand.handBox.center.x, newHand.handBox.center.y));
	Tracker.kalmTrack.push_back(getLatestKalmanPrediction());

	Tracker.camsTrack.push_back(cv::Point(handBox.center.x, handBox.center.y));

	Tracker.isTracked = false;
}

void Hand::initTracker() {
	// init Tracker
	Tracker.trackWindow = handBox.boundingRect();
	Tracker.isTracked = false;

	Tracker.trackedFrames = 0;

	// init Kalman filter
	Tracker.KalmanTracker.KF = cv::KalmanFilter(6, 2, 0);
	Tracker.KalmanTracker.state = cv::Mat(6, 1, CV_32F);
	Tracker.KalmanTracker.processNoise = cv::Mat(6, 1, CV_32F);

	randn(Tracker.KalmanTracker.state, cv::Scalar::all(0), cv::Scalar::all(0.1));
	randn(Tracker.KalmanTracker.KF.statePost, cv::Scalar::all(0), cv::Scalar::all(0.1));

	Tracker.KalmanTracker.KF.transitionMatrix = *(cv::Mat_<float>(6, 6) <<
			1, 0, 1, 0, 0.5, 0,
			0, 1, 0, 1, 0, 0.5,
			0, 0, 1, 0, 1, 0,
			0, 0, 0, 1, 0, 1,
			0, 0, 0, 0, 1, 0,
			0, 0, 0, 0, 0, 1);

	Tracker.KalmanTracker.KF.measurementMatrix = *(cv::Mat_<float>(2, 6) << 
			1, 0, 1, 0, 0.5, 0,
			0, 1, 0, 1, 0, 0.5);

	Tracker.KalmanTracker.measurement = cv::Mat(2, 1, CV_32F);
	Tracker.KalmanTracker.measurement.setTo(cv::Scalar(0));

	Tracker.KalmanTracker.KF.statePre.at<float>(0) = handBox.center.x;
	Tracker.KalmanTracker.KF.statePre.at<float>(1) = handBox.center.y;
	Tracker.KalmanTracker.KF.statePre.at<float>(2) = 0;
	Tracker.KalmanTracker.KF.statePre.at<float>(3) = 0;
	Tracker.KalmanTracker.KF.statePre.at<float>(4) = 0;
	Tracker.KalmanTracker.KF.statePre.at<float>(5) = 0;

	cv::setIdentity(Tracker.KalmanTracker.KF.measurementMatrix);
	cv::setIdentity(Tracker.KalmanTracker.KF.processNoiseCov, cv::Scalar::all(1e-4));
	cv::setIdentity(Tracker.KalmanTracker.KF.measurementNoiseCov, cv::Scalar::all(1e-1));
	cv::setIdentity(Tracker.KalmanTracker.KF.errorCovPost, cv::Scalar::all(.1));

	Tracker.isKalman = false;

	Tracker.kalmTrack.clear();
	//Tracker.kalmTrack.push_back(cv::Point(handBox.center.x, handBox.center.y));

	Tracker.camsTrack.clear();
	//Tracker.camsTrack.push_back(cv::Point(handBox.center.x, handBox.center.y));
}

// recalculate the hand's thresholding ranges:
void Hand::recalculateRange(const cv::Mat frame, SkinSegmMethod method) {
	if (method != SKIN_SEGMENT_ADAPTIVE) {
		cv::Rect rect = handBox.boundingRect();
		
		// reduce the rect to half of its size
		rect.y += (rect.height * 0.5);
		rect.height *= 0.5;
		rect.x += (rect.width * 0.1);
		rect.width *= 0.8;

		// crop the piece of the image with the palm
		cv::Mat hsv = cv::Mat(frame.clone(), rect);
		//cv::blur(hsv, hsv, cv::Size(3, 3));

		// convert it to HSV or YCrCb, depending on chosen segmentation method
		if (method == SKIN_SEGMENT_HSV)
			cv::cvtColor(hsv, hsv, cv::COLOR_BGR2HSV);
		else
			cv::cvtColor(hsv, hsv, cv::COLOR_BGR2YCrCb);

		// split the cropped piece into channels:
		std::vector<cv::Mat> bgr_planes;
		cv::split(hsv, bgr_planes);
		
		// prepare variables
		int histSize = 256;
		float range[] = { 0, 256 };
		const float* histRange = { range };
		std::vector<cv::Mat> histograms(3);

		// Compute the histograms:
		cv::calcHist(&bgr_planes[0], 1, 0, cv::Mat(), histograms[0], 1, &histSize, &histRange, true, false);
		cv::calcHist(&bgr_planes[1], 1, 0, cv::Mat(), histograms[1], 1, &histSize, &histRange, true, false);
		cv::calcHist(&bgr_planes[2], 1, 0, cv::Mat(), histograms[2], 1, &histSize, &histRange, true, false);

		// create a temporary variable of ranges
		int color_ranges[3][2] = { 0, 256, 0, 256, 0, 256 };

		// analyze the histograms
		for (int idx = 0; idx < 3; idx++) {
			cv::blur(histograms[idx], histograms[idx], cv::Size(5, 1));
			// min and max
			double minVal, maxVal;
			int minIdx, maxIdx;

			// find the maximum of the histogram
			try {
				cv::SparseMat S = cv::SparseMat(histograms[idx]);
				cv::minMaxLoc(S, &minVal, &maxVal, &minIdx, &maxIdx);
			}
			catch (cv::Exception exc) {
				std::cout << "error while computing MinMaxLoc!" << std::endl;
				return;
			}

			// find the left boundary
			for (int i = 0; i <= maxIdx; i++) {
				if (cvRound(histograms[idx].at<float>(i)) >= (0.15 * maxVal) || i == maxIdx) {
					color_ranges[idx][0] = i;
					break;
				}
			}
			// find the right boundary
			for (int i = 255; i >= maxIdx; i--) {
				if (cvRound(histograms[idx].at<float>(i)) >= (0.15 * maxVal) || i == maxIdx) {
					color_ranges[idx][1] = i;
					break;
				}
			}
		}

		// assign the ranges
		// if the segmentation method is HSV:
		if (method == SKIN_SEGMENT_HSV) {
			HSV.H_MIN = color_ranges[0][0];
			HSV.H_MAX = color_ranges[0][1];
			HSV.S_MIN = color_ranges[1][0];
			HSV.S_MAX = color_ranges[1][1];
			HSV.V_MIN = color_ranges[2][0];
			HSV.V_MAX = color_ranges[2][1];
		}
		else { // if YCrCb
			YCbCr.Y_MIN = color_ranges[0][0];
			YCbCr.Y_MAX = color_ranges[0][1];
			YCbCr.Cr_MIN = color_ranges[1][0];
			YCbCr.Cr_MAX = color_ranges[1][1];
			YCbCr.Cb_MIN = color_ranges[2][0];
			YCbCr.Cb_MAX = color_ranges[2][1];
		}
	}
}
