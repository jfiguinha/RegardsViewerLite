#include <header.h>
#include "FiltreEffetCPU.h"
#include "PerlinNoise.h"
#include "LensFlare.h"
#include "MotionBlur.h"
#include "Filtre.h"
#include "Wave.h"
#include <ImageLoadingFormat.h>
#include <hqdn3d.h>
#include "MeanShift.h"
#include <fstream>
#include <GLTexture.h>
#include <opencv2/xphoto.hpp>
#include "VideoStabilization.h"
#include <opencv2/dnn_superres.hpp>
#include <FileUtility.h>
#include <ParamInit.h>
#include <RegardsConfigParam.h>
#include <ConvertUtility.h>
using namespace Regards::OpenCV;
using namespace Regards::OpenGL;
using namespace cv;
extern float value[256];
using namespace Regards::FiltreEffet;

#define EDSR 0
#define ESPCN 1
#define FSRCNN 2
#define LapSRN 3

class CFiltreEffetCPUImpl
{
public:
	//Vignette effect
	static void generateGradient(Mat& mask, const double& radius, const double& power);
	static double getMaxDisFromCorners(const cv::Size& imgSize, const cv::Point& center);
	static double dist(cv::Point a, cv::Point b);
	static cv::Rect CalculRect(int widthIn, int heightIn, int widthOut, int heightOut, int flipH, int flipV, int angle, float ratioX, float ratioY, int x, int y, float left, float top);

};




cv::Rect CFiltreEffetCPUImpl::CalculRect(int widthIn, int heightIn, int widthOut, int heightOut, int flipH, int flipV, int angle, float ratioX, float ratioY, int x, int y, float left, float top)
{
	cv::Rect rect;
	float posX = (float)x * ratioX + left * ratioX;
	float posY = (float)y * ratioY + top * ratioY;

	if (angle == 270)
	{
		int srcx = posY;
		int srcy = posX;

		posX = srcx;
		posY = srcy;

		posX = widthIn - posX - 1;
	}
	else if (angle == 180)
	{
		posX = widthIn - posX - 1;
		posY = heightIn - posY - 1;
	}
	else if (angle == 90)
	{
		int srcx = posY;
		int srcy = posX;

		posX = srcx;
		posY = srcy;

		posY = heightIn - posY - 1;
	}

	if (angle == 90 || angle == 270)
	{
		if (flipV == 1)
		{
			posX = widthIn - posX - 1;
		}

		if (flipH == 1)
		{
			posY = heightIn - posY - 1;
		}

	}
	else
	{
		if (flipH == 1)
		{
			posX = widthIn - posX - 1;
		}

		if (flipV == 1)
		{
			posY = heightIn - posY - 1;
		}
	}

	rect.x = abs(posX);
	rect.y = abs(posY);
	return rect;
}





CFiltreEffetCPU::CFiltreEffetCPU(CRgbaquad back_color, CImageLoadingFormat* bitmap)
	: IFiltreEffet(back_color)
{
	this->backColor = back_color;

	if (bitmap != nullptr && bitmap->IsOk())
	{
		cv::Mat local = bitmap->GetOpenCVPicture();
		filename = bitmap->GetFilename();
		vector<cv::Mat> channels;
		cv::extractChannel(local, alphaChannel, 3);
		cv::cvtColor(local, input, cv::COLOR_BGRA2BGR);
		preview = false;

	}
}

cv::Mat CFiltreEffetCPU::GetMat()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	return image;
}

cv::UMat CFiltreEffetCPU::GetUMat()
{
	cv::UMat image;
	if (preview)
		paramOutput.copyTo(image);
	else
		input.copyTo(image);

    return image;    
}


bool CFiltreEffetCPU::StabilizeVideo(Regards::OpenCV::COpenCVStabilization * openCVStabilization)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	bool frameStabilized = false;

	if (openCVStabilization != nullptr)
	{
		if (openCVStabilization->GetNbFrameBuffer() == 0)
		{
			openCVStabilization->BufferFrame(image);
		}
		else
		{
			frameStabilized = true;
			openCVStabilization->AddFrame(image);
		}

		if (frameStabilized)
			openCVStabilization->CorrectFrame(image);
	}

	return frameStabilized;
}

void CFiltreEffetCPU::LoadAndRotate(const wxString& filePath, const int& rotate)
{
	if (!wxFileExists(filePath))
	{
		return;
	}

	Mat src = imread(CConvertUtility::ConvertToStdString(filePath));
	if (src.empty())
		return;

	if (rotate == 90)
	{
		// Rotate clockwise 270 degrees
		transpose(src, src);
		flip(src, src, 0);
	}
	else if (rotate == 180)
	{
		// Rotate clockwise 180 degrees
		flip(src, src, -1);
	}
	else if (rotate == 270)
	{
		// Rotate clockwise 90 degrees
		transpose(src, src);
		flip(src, src, 1);
	}
	imwrite(CConvertUtility::ConvertToStdString(filePath), src);
	src.release();
}


int CFiltreEffetCPU::GetWidth()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	return image.size().width;
}

int CFiltreEffetCPU::GetHeight()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	return image.size().height;
}

// ReSharper disable once CppDoxygenUnresolvedReference
// ReSharper disable once CppDoxygenUnresolvedReference
/**
 *  \brief Automatic brightness and contrast optimization with optional histogram clipping
 *  \param [in]src Input image GRAY or BGR or BGRA
 *  \param [out]dst Destination image
 *  \param clipHistPercent cut wings of histogram at given percent tipical=>1, 0=>Disabled
 *  \note In case of BGRA image, we won't touch the transparency
*/
void CFiltreEffetCPU::BrightnessAndContrastAuto(Mat& image, float clipHistPercent)
{
	int histSize = 256;
	float alpha, beta;
	double minGray = 0, maxGray = 0;

	CV_Assert(clipHistPercent >= 0);
	CV_Assert((image.type() == CV_8UC1) || (image.type() == CV_8UC3) || (image.type() == CV_8UC4));

	//to calculate grayscale histogram
	Mat gray;
	if (image.type() == CV_8UC1) gray = image;
	else if (image.type() == CV_8UC3) cvtColor(image, gray, COLOR_BGR2GRAY);
	else if (image.type() == CV_8UC4) cvtColor(image, gray, COLOR_BGRA2GRAY);
	if (clipHistPercent == 0)
	{
		// keep full available range
		minMaxLoc(gray, &minGray, &maxGray);
	}
	else
	{
		Mat hist; //the grayscale histogram

		float range[] = { 0, 256 };
		const float* histRange = { range };
		bool uniform = true;
		bool accumulate = false;
		calcHist(&gray, 1, nullptr, Mat(), hist, 1, &histSize, &histRange, uniform, accumulate);

		// calculate cumulative distribution from the histogram
		std::vector<float> accumulator(histSize);
		accumulator[0] = hist.at<float>(0);
		for (int i = 1; i < histSize; i++)
		{
			accumulator[i] = accumulator[i - 1] + hist.at<float>(i);
		}

		// locate points that cuts at required value
		float max = accumulator.back();
		clipHistPercent *= (max / 100.0); //make percent as absolute
		clipHistPercent /= 2.0; // left and right wings
		// locate left cut
		minGray = 0;
		while (accumulator[minGray] < clipHistPercent)
			minGray++;

		// locate right cut
		maxGray = histSize - 1;
		while (accumulator[maxGray] >= (max - clipHistPercent))
		{
			maxGray--;
			if (maxGray == 0)
				break;
		}
	}


	// current range
	float inputRange = maxGray - minGray;

	alpha = (histSize - 1) / inputRange; // alpha expands current range to histsize range
	beta = -minGray * alpha; // beta shifts current range so that minGray will go to 0

	convertScaleAbs(image, image, alpha, beta);
}
/*
void CFiltreEffetCPU::CopyPictureToTexture2D(GLTexture* texture, const bool& source, int rgba)
{
	CRegardsBitmap* bitmap;
	if (preview)
		bitmap = bitmapOut;
	else
		bitmap = pBitmap;

	if (texture != nullptr)
	{
		try
		{
			texture->SetData(bitmap);
		}
		catch (...)
		{

		}
	}
}*/


int CFiltreEffetCPU::OilPaintingEffect(const int& size, const int& dynRatio)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	xphoto::oilPainting(image, image, size, dynRatio, COLOR_BGR2Lab);

	return 0;
}

// Helper function to calculate the distance between 2 points.
double CFiltreEffetCPUImpl::dist(cv::Point a, cv::Point b)
{
	return sqrt(pow(static_cast<double>(a.x - b.x), 2) + pow(static_cast<double>(a.y - b.y), 2));
}

// Helper function that computes the longest distance from the edge to the center point.
double CFiltreEffetCPUImpl::getMaxDisFromCorners(const cv::Size& imgSize, const cv::Point& center)
{
	// given a rect and a line
	// get which corner of rect is farthest from the line

	std::vector<cv::Point> corners(4);
	corners[0] = cv::Point(0, 0);
	corners[1] = cv::Point(imgSize.width, 0);
	corners[2] = cv::Point(0, imgSize.height);
	corners[3] = cv::Point(imgSize.width, imgSize.height);

	double maxDis = 0;
	for (int i = 0; i < 4; ++i)
	{
		double dis = dist(corners[i], center);
		if (maxDis < dis)
			maxDis = dis;
	}

	return maxDis;
}

double fastCos(double x)
{
	x += 1.57079632;
	if (x > 3.14159265)
		x -= 6.28318531;
	if (x < 0)
		return 1.27323954 * x + 0.405284735 * x * x;
	return 1.27323954 * x - 0.405284735 * x * x;
}

// Helper function that creates a gradient image.   
// firstPt, radius and power, are variables that control the artistic effect of the filter.
void CFiltreEffetCPUImpl::generateGradient(Mat& mask, const double& radius, const double& power)
{
	auto firstPt = cv::Point(mask.size().width / 2, mask.size().height / 2);
	double maxImageRad = (radius * getMaxDisFromCorners(mask.size(), firstPt));
	double maxImageRadPower = maxImageRad * power;
	mask.setTo(Scalar(1));
	for (int i = 0; i < mask.rows; i++)
	{
		for (int j = 0; j < mask.cols; j++)
		{
			double temp = dist(firstPt, cv::Point(j, i));

			if (temp > maxImageRad)
			{
				mask.at<double>(i, j) = 0;
			}
			else
			{
				if (temp > maxImageRadPower)
				{
					double max = maxImageRad - maxImageRadPower;
					const double value = temp - maxImageRadPower;
					mask.at<double>(i, j) = (max - value) / max;
				}
				else
					mask.at<double>(i, j) = 1;
			}
		}
	}
}


double dist(double ax, double ay, double bx, double by)
{
	return sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by));
}


int CFiltreEffetCPU::VignetteEffect(const double& radius, const double& power)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	auto firstPt = cv::Point(image.size().width / 2, image.size().height / 2);
	double maxImageRad = static_cast<float>(radius / 100.0) * CFiltreEffetCPUImpl::getMaxDisFromCorners(
		image.size(), firstPt);
	double maxImageRadPower = maxImageRad * static_cast<float>(power / 100.0);

	Mat labImg(image.size(), CV_8UC3);

	cvtColor(image, labImg, COLOR_BGR2Lab);

	for (int row = 0; row < image.size().height; row++)
	{
		for (int col = 0; col < image.size().width; col++)
		{

			double temp = CFiltreEffetCPUImpl::dist(firstPt, cv::Point(col, row));
			if (temp > maxImageRad)
				labImg.at<Vec3b>(row, col)[0] = 0;
			else
			{
				if (temp > maxImageRadPower)
				{
					double max = maxImageRad - maxImageRadPower;
					double _value = temp - maxImageRadPower;
					labImg.at<Vec3b>(row, col)[0] *= (max - _value) / max;
				}
			}
		}
	}

	cvtColor(labImg, image, COLOR_Lab2BGR);
	labImg.release();
	return 0;
}


void CFiltreEffetCPU::SetBitmap(CImageLoadingFormat* bitmap)
{
	if (bitmap != nullptr)
	{
		cv::Mat local = bitmap->GetOpenCVPicture();
		filename = bitmap->GetFilename();
		vector<cv::Mat> channels;
		cv::extractChannel(local, alphaChannel, 3);
		cv::cvtColor(local, input, cv::COLOR_BGRA2BGR);
		preview = false;
	}
}

int CFiltreEffetCPU::WaveFilter(int x, int y, short height, int scale, int radius)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	auto waveFilter = new CWaveFilter();
	waveFilter->ProcessEffect(image, x, y, height, scale, radius);
	delete waveFilter;
	return 0;
}


// Remove black dots (upto 4x4 in size) of noise from a pure black & white image.
// ie: The input image should be mostly white (255) and just contains some black (0) noise
// in addition to the black (0) edges.
void CFiltreEffetCPU::RemovePepperNoise(Mat& mask)
{
	// For simplicity, ignore the top & bottom row border.
	for (int y = 2; y < mask.rows - 2; y++)
	{
		// Get access to each of the 5 rows near this pixel.
		uchar* pThis = mask.ptr(y);
		uchar* pUp1 = mask.ptr(y - 1);
		uchar* pUp2 = mask.ptr(y - 2);
		uchar* pDown1 = mask.ptr(y + 1);
		uchar* pDown2 = mask.ptr(y + 2);

		// For simplicity, ignore the left & right row border.
		pThis += 2;
		pUp1 += 2;
		pUp2 += 2;
		pDown1 += 2;
		pDown2 += 2;
		for (int x = 2; x < mask.cols - 2; x++)
		{
			uchar v = *pThis; // Get the current pixel value (either 0 or 255).
			// If the current pixel is black, but all the pixels on the 2-pixel-radius-border are white
			// (ie: it is a small island of black pixels, surrounded by white), then delete that island.
			if (v == 0)
			{
				bool allAbove = *(pUp2 - 2) && *(pUp2 - 1) && *(pUp2) && *(pUp2 + 1) && *(pUp2 + 2);
				bool allLeft = *(pUp1 - 2) && *(pThis - 2) && *(pDown1 - 2);
				bool allBelow = *(pDown2 - 2) && *(pDown2 - 1) && *(pDown2) && *(pDown2 + 1) && *(pDown2 + 2);
				bool allRight = *(pUp1 + 2) && *(pThis + 2) && *(pDown1 + 2);
				bool surroundings = allAbove && allLeft && allBelow && allRight;
				if (surroundings == true)
				{
					// Fill the whole 5x5 block as white. Since we know the 5x5 borders
					// are already white, just need to fill the 3x3 inner region.
					*(pUp1 - 1) = 255;
					*(pUp1 + 0) = 255;
					*(pUp1 + 1) = 255;
					*(pThis - 1) = 255;
					*(pThis + 0) = 255;
					*(pThis + 1) = 255;
					*(pDown1 - 1) = 255;
					*(pDown1 + 0) = 255;
					*(pDown1 + 1) = 255;
				}
				// Since we just covered the whole 5x5 block with white, we know the next 2 pixels
				// won't be black, so skip the next 2 pixels on the right.
				pThis += 2;
				pUp1 += 2;
				pUp2 += 2;
				pDown1 += 2;
				pDown2 += 2;
			}
			// Move to the next pixel.
			pThis++;
			pUp1++;
			pUp2++;
			pDown1++;
			pDown2++;
		}
	}
}

int CFiltreEffetCPU::CartoonifyImage(const int& mode)
{
	bool sketchMode = false;
	bool alienMode = false;
	bool evilMode = false;

	switch (mode)
	{
	case 1:
		sketchMode = true;
		break;
	case 2:
		alienMode = true;
		break;
	case 3:
		evilMode = true;
		break;
	default:
		break;
	}

	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	Mat dst;

	// Convert from BGR color to Grayscale
	Mat srcGray;
	cvtColor(image, srcGray, COLOR_BGRA2GRAY);

	// Remove the pixel noise with a good Median filter, before we start detecting edges.
	medianBlur(srcGray, srcGray, 7);

	cv::Size size = srcGray.size();
	auto mask = Mat(size, CV_8U);
	auto edges = Mat(size, CV_8U);
	if (!evilMode)
	{
		// Generate a nice edge mask, similar to a pencil line drawing.
		Laplacian(srcGray, edges, CV_8U, 5);
		threshold(edges, mask, 80, 255, THRESH_BINARY_INV);
		// Mobile cameras usually have lots of noise, so remove small
		// dots of black noise from the black & white edge mask.
		RemovePepperNoise(mask);
	}
	else
	{
		// Evil mode, making everything look like a scary bad guy.
		// (Where "srcGray" is the original grayscale image plus a medianBlur of size 7x7).
		Mat edges2;
		Scharr(srcGray, edges, CV_8U, 1, 0);
		Scharr(srcGray, edges2, CV_8U, 1, 0, -1);
		edges += edges2;
		threshold(edges, mask, 12, 255, THRESH_BINARY_INV);
		medianBlur(mask, mask, 3);
		edges2.release();
	}
	//imshow("edges", edges);
	//imshow("mask", mask);

	// For sketch mode, we just need the mask!
	if (sketchMode)
	{
		// The output image has 3 channels, not a single channel.
		cvtColor(mask, image, COLOR_GRAY2BGR);
		dst.release();
		mask.release();
		edges.release();
		return -1;
	}

	// Do the bilateral filtering at a shrunken scale, since it
	// runs so slowly but doesn't need full resolution for a good effect.
	cv::Size smallSize;
	smallSize.width = size.width / 2;
	smallSize.height = size.height / 2;

	auto smallImg = Mat(smallSize, CV_8UC3);
	resize(image, smallImg, smallSize, 0, 0, INTER_LINEAR);

	// Perform many iterations of weak bilateral filtering, to enhance the edges
	// while blurring the flat regions, like a cartoon.
	auto tmp = Mat(smallSize, CV_8UC3);
	int repetitions = 7; // Repetitions for strong cartoon effect.
	for (int i = 0; i < repetitions; i++)
	{
		int d = 9; // Filter size. Has a large effect on speed.
		double sigmaColor = 9; // Filter color strength.
		double sigmaSpace = 7; // Positional strength. Effects speed.
		bilateralFilter(smallImg, tmp, d, sigmaColor, sigmaSpace);
		bilateralFilter(tmp, smallImg, d, sigmaColor, sigmaSpace);
	}

	if (alienMode)
	{
		// Apply an "alien" filter, when given a shrunken image and the full-res edge mask.
		// Detects the color of the pixels in the middle of the image, then changes the color of that region to green.
		ChangeFacialSkinColor(smallImg, edges);
	}

	// Go back to the original scale.
	//cvtColor(smallImg, srcBgr, COLOR_BGR2BGRA);
	resize(smallImg, dst, size, 0, 0, INTER_LINEAR);

	// Use the blurry cartoon image, except for the strong edges that we will leave black.
	dst.copyTo(image, mask);

	dst.release();
	mask.release();
	tmp.release();
	//srcBgr.release();
	smallImg.release();
	edges.release();
	return 0;
}

// Apply an "alien" filter, when given a shrunken BGR image and the full-res edge mask.
// Detects the color of the pixels in the middle of the image, then changes the color of that region to green.
void CFiltreEffetCPU::ChangeFacialSkinColor(Mat smallImgBGR, Mat bigEdges)
{
	// Convert to Y'CrCb color-space, since it is better for skin detection and color adjustment.
	auto yuv = Mat(smallImgBGR.size(), CV_8UC3);
	cvtColor(smallImgBGR, yuv, COLOR_BGR2YCrCb);

	// The floodFill mask has to be 2 pixels wider and 2 pixels taller than the small image.
	// The edge mask is the full src image size, so we will shrink it to the small size,
	// storing into the floodFill mask data.
	int sw = smallImgBGR.cols;
	int sh = smallImgBGR.rows;
	Mat maskPlusBorder = Mat::zeros(sh + 2, sw + 2, CV_8U);
	Mat mask = maskPlusBorder(cv::Rect(1, 1, sw, sh)); // mask is a ROI in maskPlusBorder.
	resize(bigEdges, mask, smallImgBGR.size());

	// Make the mask values just 0 or 255, to remove weak edges.
	threshold(mask, mask, 80, 255, THRESH_BINARY);
	// Connect the edges together, if there was a pixel gap between them.
	dilate(mask, mask, Mat());
	erode(mask, mask, Mat());
	//imshow("constraints for floodFill", mask);

	// YCrCb Skin detector and color changer using multiple flood fills into a mask.
	// Apply flood fill on many points around the face, to cover different shades & colors of the face.
	// Note that these values are dependent on the face outline, drawn in drawFaceStickFigure().
	const int NUM_SKIN_POINTS = 6;
	cv::Point skinPts[NUM_SKIN_POINTS];
	skinPts[0] = cv::Point(sw / 2, sh / 2 - sh / 6);
	skinPts[1] = cv::Point(sw / 2 - sw / 11, sh / 2 - sh / 6);
	skinPts[2] = cv::Point(sw / 2 + sw / 11, sh / 2 - sh / 6);
	skinPts[3] = cv::Point(sw / 2, sh / 2 + sh / 16);
	skinPts[4] = cv::Point(sw / 2 - sw / 9, sh / 2 + sh / 16);
	skinPts[5] = cv::Point(sw / 2 + sw / 9, sh / 2 + sh / 16);
	// Skin might be fairly dark, or slightly less colorful.
	// Skin might be very bright, or slightly more colorful but not much more blue.
	const int LOWER_Y = 60;
	const int UPPER_Y = 80;
	const int LOWER_Cr = 25;
	const int UPPER_Cr = 15;
	const int LOWER_Cb = 20;
	const int UPPER_Cb = 15;
	auto lowerDiff = Scalar(LOWER_Y, LOWER_Cr, LOWER_Cb);
	auto upperDiff = Scalar(UPPER_Y, UPPER_Cr, UPPER_Cb);
	// Instead of drawing into the "yuv" image, just draw 1's into the "maskPlusBorder" image, so we can apply it later.
	// The "maskPlusBorder" is initialized with the edges, because floodFill() will not go across non-zero mask pixels.
	Mat edgeMask = mask.clone(); // Keep an duplicate copy of the edge mask.
	for (int i = 0; i < NUM_SKIN_POINTS; i++)
	{
		// Use the floodFill() mode that stores to an external mask, instead of the input image.
		const int flags = 4 | FLOODFILL_FIXED_RANGE | FLOODFILL_MASK_ONLY;
		floodFill(yuv, maskPlusBorder, skinPts[i], Scalar(), nullptr, lowerDiff, upperDiff, flags);
	}

	// After the flood fill, "mask" contains both edges and skin pixels, whereas
	// "edgeMask" just contains edges. So to get just the skin pixels, we can remove the edges from it.
	mask -= edgeMask;
	// "mask" now just contains 1's in the skin pixels and 0's for non-skin pixels.

	// Change the color of the skin pixels in the given BGR image.
	int Red = 0;
	int Green = 70;
	int Blue = 0;
	add(smallImgBGR, Scalar(Blue, Green, Red), smallImgBGR, mask);

	edgeMask.release();
	yuv.release();
	maskPlusBorder.release();
	mask.release();
}


int CFiltreEffetCPU::MeanShift(const float& fSpatialRadius, const float& fColorRadius)
{
	cv::Mat image, dst;
	if (preview)
		image = paramOutput;
	else
		image = input;

	cvtColor(image, dst, COLOR_BGR2Lab);
	// Initilize Mean Shift with spatial bandwith and color bandwith
	CMeanShift msProcess(fSpatialRadius, fColorRadius);
	//MSProc(fSpatialRadius, fColorRadius);
	// Filtering Process
	msProcess.MSFiltering(dst);
	cvtColor(dst, image, COLOR_Lab2RGB);

	return 0;
}

int CFiltreEffetCPU::BilateralFilter(const int& fSize, const int& sigmaX, const int& sigmaP)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	try
	{
		cv::Mat output;
		bilateralFilter(image, output, fSize, sigmaX, sigmaP, BORDER_DEFAULT);
		output.copyTo(image);
	}
	catch (Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
	return 0;
}

int CFiltreEffetCPU::NlmeansFilter(const int& h, const int& hColor, const int& templateWindowSize, const int& searchWindowSize)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	try
	{
		fastNlMeansDenoisingColored(image, image, h, hColor, templateWindowSize, searchWindowSize);
	}
	catch (Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
	return 0;

}


int CFiltreEffetCPU::SharpenMasking(const float& sharpness)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	try
	{
		Mat blurred; 
		double sigma = 1, threshold = 5, amount = sharpness;
		cv::GaussianBlur(image, blurred, cv::Size(), sigma, sigma);
		Mat lowConstrastMask = abs(image - blurred) < threshold;
		Mat sharpened = image * (1 + amount) + blurred * (-amount);
		image.copyTo(sharpened, lowConstrastMask);
		sharpened.copyTo(image);
	}
	catch (Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
	return 0;
}

CFiltreEffetCPU::~CFiltreEffetCPU()
{
	if (hq3d != nullptr)
		delete hq3d;
}

wxImage CFiltreEffetCPU::GetwxImage()
{
	/*
	CImageLoadingFormat picture;

	if (preview)
		picture.SetPicture(paramOutput);
	else
		picture.SetPicture(input);


	
	return picture.GetwxImage();
	*/

	
	cv::Mat im2;

	if (preview)
		im2 = paramOutput;
	else
		im2 = input;

	//if (im2.channels() == 1) { cvtColor(im2, im2, cv::COLOR_GRAY2RGB); }
	//else if (im2.channels() == 4) { cvtColor(im2, im2, cv::COLOR_BGRA2RGB); }
	//else { cvtColor(im2, im2, cv::COLOR_BGR2RGB); }

	long imsize = im2.rows * im2.cols * im2.channels();
	wxImage wx(im2.cols, im2.rows, (unsigned char*)malloc(imsize), false);
	unsigned char* s = im2.data;
	unsigned char* d = wx.GetData();
	memcpy(d, s, imsize);
	return wx;
}

cv::Mat CFiltreEffetCPU::Interpolation(const cv::Mat & inputData, const int& widthOut, const int& heightOut, const wxRect& rc, const int& method, int flipH, int flipV, int angle, int ratio)
{
	cv::Mat cvImage;
	//cv::Mat cvImage;
	//inputData.copyTo(cvImage);
	try
	{
		float ratioX = (float)inputData.cols / rc.width;
		float ratioY = (float)inputData.rows / rc.height;
		if (angle == 90 || angle == 270)
		{
			ratioX = (float)inputData.cols / (float)rc.height;
			ratioY = (float)inputData.rows / (float)rc.width;
		}

		cv::Rect rectGlobal;
		cv::Rect rect_begin = CFiltreEffetCPUImpl::CalculRect(inputData.cols, inputData.rows, widthOut, heightOut, flipH, flipV, angle, ratioX, ratioY, 0, 0, rc.x, rc.y);
		cv::Rect rect_end = CFiltreEffetCPUImpl::CalculRect(inputData.cols, inputData.rows, widthOut, heightOut, flipH, flipV, angle, ratioX, ratioY, widthOut, heightOut, rc.x, rc.y);
		rectGlobal.x = rect_begin.x;
		rectGlobal.y = rect_begin.y;
		rectGlobal.width = rect_end.x;
		rectGlobal.height = rect_end.y;
		if (rectGlobal.x > rectGlobal.width)
		{
			int x_end = rectGlobal.x;
			int x = rectGlobal.width;
			rectGlobal.x = x;
			rectGlobal.width = x_end - x;
		}
		else
		{
			rectGlobal.width -= rectGlobal.x;
		}

		if (rectGlobal.y > rectGlobal.height)
		{
			int y_end = rectGlobal.y;
			int y = rectGlobal.height;
			rectGlobal.y = y;
			rectGlobal.height = y_end - y;
		}
		else
		{
			rectGlobal.height -= rectGlobal.y;
		}

		if ((rectGlobal.height + rectGlobal.y) > inputData.rows)
		{
			rectGlobal.height = inputData.rows - rectGlobal.y;
		}
		if ((rectGlobal.width + rectGlobal.x) > inputData.cols)
		{
			rectGlobal.width = inputData.cols - rectGlobal.x;
		}
		
		//cv::Mat crop;
		inputData(rectGlobal).copyTo(cvImage);
		//crop.copyTo(cvImage);

		if (angle == 90)
		{
			if (flipV && flipH)
				cv::rotate(cvImage, cvImage, cv::ROTATE_90_CLOCKWISE);
			else if (flipV || flipH)
				cv::rotate(cvImage, cvImage, cv::ROTATE_90_COUNTERCLOCKWISE);
			else
				cv::rotate(cvImage, cvImage, cv::ROTATE_90_CLOCKWISE);

		}
		else if (angle == 270)
		{
			if (flipV && flipH)
				cv::rotate(cvImage, cvImage, cv::ROTATE_90_COUNTERCLOCKWISE);
			else if (flipV || flipH)
				cv::rotate(cvImage, cvImage, cv::ROTATE_90_CLOCKWISE);
			else
				cv::rotate(cvImage, cvImage, cv::ROTATE_90_COUNTERCLOCKWISE);

		}
		else if (angle == 180)
		{
			cv::rotate(cvImage, cvImage, cv::ROTATE_180);
		}

		if (ratio != 100)
		{
			cv::resize(cvImage, cvImage, cv::Size(widthOut, heightOut), method);
		}

		if (cvImage.cols != widthOut || cvImage.rows != heightOut)
			cv::resize(cvImage, cvImage, cv::Size(widthOut, heightOut), cv::INTER_NEAREST_EXACT);

		//Apply Transformation

		if (flipH)
		{
			if (angle == 90 || angle == 270)
				cv::flip(cvImage, cvImage, 0);
			else
				cv::flip(cvImage, cvImage, 1);
		}
		if (flipV)
		{
			if (angle == 90 || angle == 270)
				cv::flip(cvImage, cvImage, 1);
			else
				cv::flip(cvImage, cvImage, 0);
		}

		//cv::cvtColor(cvImage, cvImage, cv::COLOR_BGR2BGRA);
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;

	}
	return cvImage;
}

void CFiltreEffetCPU::Interpolation(const int& widthOut, const int& heightOut, const wxRect& rc, const int& method,
	int flipH, int flipV, int angle, int ratio)
{
	paramOutput = Interpolation(input, widthOut, heightOut, rc, method, flipH, flipV, angle, ratio);
	preview = true;
}


int CFiltreEffetCPU::HistogramNormalize()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	vector<Mat> bgr_planes;
	split(image, bgr_planes);
	int gridsize = 8;
	cv::Ptr<CLAHE> clahe = createCLAHE(2.0, cv::Size(gridsize, gridsize));
	clahe->apply(bgr_planes[0], bgr_planes[0]);
	clahe->apply(bgr_planes[1], bgr_planes[1]);
	clahe->apply(bgr_planes[2], bgr_planes[2]);
	cv::merge(bgr_planes, image);

	bgr_planes[0].release();
	bgr_planes[1].release();
	bgr_planes[2].release();

	return 0;
	//return CHistogramme::HistogramNormalize(pBitmap);
}

int CFiltreEffetCPU::HistogramEqualize()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;


	vector<Mat> bgr_planes;
	split(image, bgr_planes);
	equalizeHist(bgr_planes[0], bgr_planes[0]);
	equalizeHist(bgr_planes[1], bgr_planes[1]);
	equalizeHist(bgr_planes[2], bgr_planes[2]);

	cv::merge(bgr_planes, image);

	bgr_planes[0].release();
	bgr_planes[1].release();
	bgr_planes[2].release();

	image.release();

	return 0;

}

int CFiltreEffetCPU::HQDn3D(const double& LumSpac, const double& ChromSpac, const double& LumTmp,
                            const double& ChromTmp)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	if (hq3d == nullptr)
	{
		hq3d = new Chqdn3d(image.size().width, image.size().height, LumSpac, LumTmp);
	}
	else if (oldLevelDenoise != LumSpac || image.size().width != oldwidthDenoise || image.size().height
		!= oldheightDenoise)
	{
		delete hq3d;
		hq3d = new Chqdn3d(image.size().width, image.size().height, LumSpac, LumTmp);
	}

	hq3d->ApplyDenoise3D(image);


	return 0;
}

int CFiltreEffetCPU::LensFlare(const int& iPosX, const int& iPosY, const int& iPuissance, const int& iType,
                               const int& iIntensity, const int& iColor, const int& iColorIntensity)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	{
		cv:Mat output;
		cv::cvtColor(image, output, cv::COLOR_BGR2BGRA);
		auto filtre = new CLensFlare();
		filtre->LensFlare(&output, iPosX, iPosY, iPuissance, iType, iIntensity, iColor, iColorIntensity);
		delete filtre;
		cv::cvtColor(output, image, cv::COLOR_BGRA2BGR);
	}
	return 0;
}

//---------------------------------------------------------------------
//Effet CRgbaquad 
//---------------------------------------------------------------------
int CFiltreEffetCPU::RGBFilter(const int& red, const int& green, const int& blue)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	cv::Scalar color = cv::Scalar(red, green, blue);
	image = image + color;
	return 0;
}

int CFiltreEffetCPU::Solarize(const long& threshold)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	auto filtre = new CSolarize(static_cast<int>(threshold));
	filtre->SetParameter(image, backColor);
	filtre->Compute();
	delete filtre;

	return 0;
}

int CFiltreEffetCPU::Posterize(const float& level, const float& gamma)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;


	auto filtre = new CPosterize(level, gamma);
	filtre->SetParameter(image, backColor);
	filtre->Compute();
	delete filtre;

	return 0;
}


//---------------------------------------------------------------------
//Effet Clouds 
//---------------------------------------------------------------------
int CFiltreEffetCPU::CloudsFilter(const CRgbaquad& color1, const CRgbaquad& color2, const float& amplitude,
                                  const float& frequence, const int& octave, const int& intensity)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;


	auto m_perlinNoise = new CPerlinNoise();
	cv::Mat localBitmap = cv::Mat(250, 250, CV_8UC4);
	m_perlinNoise->Clouds(localBitmap, color1, color2, amplitude / 100.0f, frequence / 100.0f, octave);
	delete m_perlinNoise;
	cv::resize(localBitmap, localBitmap, cv::Size(image.size().width, image.size().height), INTER_CUBIC);
	Fusion(localBitmap, intensity / 100.0f);
	return 0;
}

//---------------------------------------------------------------------
//Effet Swirl 
//---------------------------------------------------------------------
int CFiltreEffetCPU::Swirl(const float& radius, const float& angle)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;


	auto filtre = new CSwirl(angle, radius);
	filtre->SetParameter(image, backColor);
	filtre->Compute();
	delete filtre;

	return 0;
}

//---------------------------------------------------------------------
//Lumi??re et Contraste 
//---------------------------------------------------------------------
int CFiltreEffetCPU::BrightnessAndContrast(const double& brightness, const double& contrast)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	float alpha = contrast / 100.0f; //# Contrast control(1.0 - 3.0)
	float beta = brightness; //# Brightness control(0 - 100)
	cv::convertScaleAbs(image, image, alpha, beta);
	return 0;
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
int CFiltreEffetCPU::NiveauDeGris()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	Mat dest;
	cvtColor(image, dest, COLOR_BGR2GRAY);
	cvtColor(dest, image, COLOR_GRAY2BGR);

	return 0;
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
int CFiltreEffetCPU::NoirEtBlanc()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	Mat dest;
	cvtColor(image, dest, COLOR_BGR2GRAY);
	threshold(dest, dest, 127, 255, THRESH_BINARY);
	cvtColor(dest, image, COLOR_GRAY2BGR);
	dest.release();

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Sepia()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;


	cv::Mat kernel =
		(cv::Mat_<float>(3, 3)
			<<
			0.272, 0.534, 0.131,
			0.349, 0.686, 0.168,
			0.393, 0.769, 0.189);

	cv::transform(image, image, kernel);

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Soften()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	short kernel[] = {1, 1, 1, 1, 8, 1, 1, 1, 1};
	auto filtre = new CMatrixConvolution(kernel, 3, 16, 0);
	filtre->SetParameter(image, backColor);
	filtre->Compute();
	delete filtre;

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Blur(const int& radius)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	blur(image, image, cv::Size(radius, radius));
	return 0;
}


//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::GaussianBlur(const int& radius, const int& boxSize)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	cv::GaussianBlur(image, image, cv::Size(boxSize, boxSize), radius);
	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Emboss()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;


	// Construct kernel (all entries initialized to 0)
	Mat kernel(3, 3, CV_32F, Scalar(0));
	kernel.at<float>(0, 0) = -1.0;
	kernel.at<float>(2, 2) = 1.0;

	filter2D(image, image, image.depth(), kernel);

	kernel.release();


	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::SharpenStrong()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	Mat kernel(3, 3, CV_32F, Scalar(0));
	kernel.at<float>(0, 0) = -1.0;
	kernel.at<float>(0, 1) = -1.0;
	kernel.at<float>(0, 2) = -1.0;
	kernel.at<float>(1, 0) = -1.0;
	kernel.at<float>(1, 1) = 9.0;
	kernel.at<float>(1, 2) = -1.0;
	kernel.at<float>(2, 0) = -1.0;
	kernel.at<float>(2, 1) = -1.0;
	kernel.at<float>(2, 2) = -1.0;

	filter2D(image, image, image.depth(), kernel);
	kernel.release();


	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Sharpen()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	Mat kernel(3, 3, CV_32F, Scalar(0));
	kernel.at<float>(1, 1) = 5.0;
	kernel.at<float>(0, 1) = -1.0;
	kernel.at<float>(2, 1) = -1.0;
	kernel.at<float>(1, 0) = -1.0;
	kernel.at<float>(1, 2) = -1.0;

	filter2D(image, image, image.depth(), kernel);

	kernel.release();

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Erode()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;


	erode(image, image, Mat());

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Median()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	medianBlur(image, image, 3);

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Noise()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;


	auto filtre = new CNoise();
	filtre->SetParameter(image, backColor);
	filtre->Compute();
	delete filtre;

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Dilate()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	dilate(image, image, Mat());

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Negatif()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	bitwise_not(image, image);

	return 0;
}

int CFiltreEffetCPU::LensDistortionFilter(const int &size)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;  
        
    Mat eiffel;
    image.copyTo(eiffel);

    int halfWidth = eiffel.rows / 2;
    int halfHeight = eiffel.cols / 2;
	double strength = (double)size/100;
    double correctionRadius = sqrt(pow(eiffel.rows, 2) + pow(eiffel.cols, 2)) / strength;

    int newX, newY;
    double distance; 
    double theta; 
    int sourceX; 
    int sourceY; 
    double r;
    for (int i = 0; i < image.rows; ++i)
    {
        for (int j = 0; j < image.cols; j++)
        {
            newX = i - halfWidth; 
            newY = j - halfHeight;
            distance = sqrt(pow(newX, 2) + pow(newY, 2));
            r = distance / correctionRadius;
            if (r == 0.0)
                theta = 1;
            else
                theta = atan(r) / r;

            sourceX = round(halfWidth + theta*newX);
            sourceY = round(halfHeight + theta * newY);

            
            image.at<Vec3b>(i, j)[0] =eiffel.at<Vec3b>(sourceX, sourceY)[0];
            image.at<Vec3b>(i, j)[1] =eiffel.at<Vec3b>(sourceX, sourceY)[1];
            image.at<Vec3b>(i, j)[2] = eiffel.at<Vec3b>(sourceX, sourceY)[2];
        }
    }  
	return 0;
	//imwrite("d:\\test.jpg", image);
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::FiltreEdge()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	Mat dest;
	cvtColor(image, dest, COLOR_BGR2GRAY);

	Mat img_blur;
	cv::GaussianBlur(dest, img_blur, cv::Size(3, 3), 0, 0);
	Mat edges;
	Canny(img_blur, edges, 100, 200, 3, false);

	cvtColor(edges, image, COLOR_GRAY2BGR);
		
	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::FiltreMosaic(const int& size)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	auto filtre = new CMosaic(size);
	filtre->SetParameter(image, backColor);
	filtre->Compute();
	delete filtre;

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::FlipVertical()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	cv::flip(image, image, 0);
	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::FlipHorizontal()
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	cv::flip(image, image, 1);

	return 0;
}



//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::MotionBlur(const double& radius, const double& sigma, const double& angle)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	auto filtre = new CMotionBlur();
	filtre->MotionBlur(image, radius, sigma, angle);
	delete filtre;

	return 0;
}

int CFiltreEffetCPU::GroundGlassEffect(const double& radius)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	Mat imageResult = image.clone();
	RNG rng;
	int randomNum;
	int Number = radius;

	for (int i = 0; i < image.rows - Number; i++)
		for (int j = 0; j < image.cols - Number; j++)
		{
			randomNum = rng.uniform(0, Number);
			imageResult.at<Vec3b>(i, j)[0] = image.at<Vec3b>(i + randomNum, j + randomNum)[0];
			imageResult.at<Vec3b>(i, j)[1] = image.at<Vec3b>(i + randomNum, j + randomNum)[1];
			imageResult.at<Vec3b>(i, j)[2] = image.at<Vec3b>(i + randomNum, j + randomNum)[2];
		}

	imageResult.copyTo(image);

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::RotateFree(const double& angle, const int& widthOut, const int& heightOut)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	cv::Mat out;
	// get rotation matrix for rotating the image around its center in pixel coordinates
	const Point2f center((image.cols - 1) / 2.0, (image.rows - 1) / 2.0);
	Mat rot = getRotationMatrix2D(center, angle, 1.0);
	// determine bounding rectangle, center not relevant
	Rect2f bbox = RotatedRect(Point2f(), image.size(), angle).boundingRect2f();
	// adjust transformation matrix
	rot.at<double>(0, 2) += bbox.width / 2.0 - image.cols / 2.0;
	rot.at<double>(1, 2) += bbox.height / 2.0 - image.rows / 2.0;

	warpAffine(image, out, rot, bbox.size());

	
	if (preview)
		out.copyTo(paramOutput);
	else
		out.copyTo(input);

	rot.release();

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::PhotoFiltre(const CRgbaquad& clValue, const int& intensity)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;


	float coeff = (float)intensity / 100.0f;
	float diff = 1.0f - coeff;
	cv::Mat out;
	cv::Mat out_one;
	out_one = image.mul(diff);

	cv::Scalar color = cv::Scalar(clValue.GetBlue(), clValue.GetGreen(), clValue.GetRed());
	cv::Scalar out_two = color * coeff;

	cv::add(out_one, out_two, image);

	return 0;
}

void CFiltreEffetCPU::RotateMatrix(const int& angle, Mat& src)
{
	Mat dst;
	if (angle == 90)
	{
		// Rotate clockwise 270 degrees
		transpose(src, dst);
		flip(dst, src, 0);
	}
	else if (angle == 180)
	{
		// Rotate clockwise 180 degrees
		flip(src, src, -1);
	}
	else if (angle == 270)
	{
		// Rotate clockwise 90 degrees
		transpose(src, dst);
		flip(dst, src, 1);
	}
}


//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Rotate90()
{
	if (preview)
		cv::rotate(paramOutput, paramOutput, cv::ROTATE_90_CLOCKWISE);
	else
		cv::rotate(input, input, cv::ROTATE_90_CLOCKWISE);

	return 0;
}

int CFiltreEffetCPU::Rotate180()
{
	if (preview)
		cv::rotate(paramOutput, paramOutput, cv::ROTATE_180);
	else
		cv::rotate(input, input, cv::ROTATE_180);

	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Rotate270()
{
	if (preview)
		cv::rotate(paramOutput, paramOutput, cv::ROTATE_90_COUNTERCLOCKWISE);
	else
		cv::rotate(input, input, cv::ROTATE_90_COUNTERCLOCKWISE);
	
	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Resize(const int& imageWidth, const int& imageHeight, const int& interpolation)
{
	resize(input, paramOutput, cv::Size(imageHeight, imageWidth), 0, 0, INTER_CUBIC);
	return 0;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int CFiltreEffetCPU::Fusion(cv::Mat & bitmapSecond, const float& pourcentage)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	cv::Mat dst;
	cv::cvtColor(bitmapSecond, dst, COLOR_BGRA2BGR);
	float beta = (1.0 - pourcentage);
	cv::addWeighted(image, pourcentage, dst, beta, 0.0, dst);
	dst.copyTo(image);
	return 0;
}

cv::Mat CFiltreEffetCPU::GetBitmap(const bool& source)
{
	cv::Mat output;

	if (source)
	{
		input.copyTo(output);
		cv::cvtColor(output, output, cv::COLOR_BGR2BGRA);
		//cv::insertChannel(alphaChannel, output, 3);
	}
	else if (preview && !paramOutput.empty())
	{
		paramOutput.copyTo(output);
		cv::cvtColor(output, output, cv::COLOR_BGR2BGRA);
	}
	else
	{
		input.copyTo(output);
		cv::cvtColor(output, output, cv::COLOR_BGR2BGRA);
		//cv::insertChannel(alphaChannel, output, 3);
	}

	return output;
}

int CFiltreEffetCPU::BrightnessAndContrastAuto(float clipHistPercent)
{
	cv::Mat image;
	if (preview)
		image = paramOutput;
	else
		image = input;

	BrightnessAndContrastAuto(image, clipHistPercent);

	return 0;
}
