// Forschungsproject (teil 2) at HTW Berlin
// Recognition of short-time micro-gestures from a single-PoV video stream
// (c) Nikita "Xonxt" Kovalenko, 2013-2014, Berlin

#pragma once

#include "stdafx.h"
#include "Hand.h"
#include "HandDetector.h"
#include "Tracker.h"
#include "GestureAnalyzer.h"

#include <time.h>
#include <ctime>

#include <fstream>

#define FACE_DETECTOR_XML "lbpcascade_frontalface.xml"

class FrameProcessor
{
public:
	FrameProcessor();
    ~FrameProcessor();

	// initizlize the processor
	bool initialize(bool isWebCam = false);

	// the list of detected hands
	std::vector<Hand> hands;

	// process frame, find hands, detect gestures
	void processFrame(cv::Mat& frame);
	
	// draw everything on frame (hands, fingers, etc.)
	void drawFrame(cv::Mat& frame);

	// change skin segmentation method
	void changeSkinMethod();

	// toggle show boolean skin-mask
	void toggleShowMask();

	// toggle showing the hand contours
	void toggleShowContour();

	// toggle showing the bounding box
	void toggleShowBoundingBox();

	// toggle showing fingertips as small circles
	void toggleShowFingers();

	// toggle showing information about the hand
	void toggleShowHandText();

	// toggle showing on-screen display information
	void toggleShowInformation();

private:
	// detect and track hands in the frame
	void detectAndTrack(const cv::Mat& frame);

	// Tool for detecting every hand (open palm!) in the image
	HandDetector handDetector;

	// face cascade detector
	cv::CascadeClassifier faceCascade;

	// Tool fot tracking of hands
	Tracker handTracker;

	// The gesture analyzer
	GestureAnalyzer gestureAnalyzer;

	// show backprojection mask or not?
	bool showMask;

	// show the contour?
	bool showContour;

	// show the bounding box (or bounding ellipse)?
	bool showBoundingBox;

	// bool show fingertips?
	bool showFingers;

	// is a webcam?
	bool isWebCam;

	// show hand-text?
	bool showHandText;

	// show text information
	bool showInformation;
};

