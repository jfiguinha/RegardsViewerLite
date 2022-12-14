#include <header.h>
#include "OpenCLFilter.h"

#include "utility.h"
#include <opencv2/xphoto.hpp>
//#include <opencv2/dnn_superres.hpp>
#include <FileUtility.h>
#include <mutex>
#include <ParamInit.h>
#include <RegardsConfigParam.h>
#include <opencv2/core/ocl.hpp>
#include <LibResource.h>
#if defined __APPLE__
#include <OpenCL/cl_gl.h>
#else
#include <CL/cl_gl.h>
#endif

using namespace Regards::OpenCL;
using namespace cv;

#define EDSR 0
#define ESPCN 1
#define FSRCNN 2
#define LapSRN 3

bool isUsed = false;
std::mutex muDnnSuperResImpl;
int numTexture = -1;


class CSuperSampling
{
public:
	CSuperSampling() {};
	~CSuperSampling() {};
	string GenerateModelPath(string modelName, int scale);
	bool TestIfMethodIsValid(int method, int scale);

private:

	int oldscale = -1;
	int oldmethod = -1;
};

string CSuperSampling::GenerateModelPath(string modelName, int scale)
{
	wxString path = "";
#ifdef WIN32
	path = CFileUtility::GetResourcesFolderPath() + "\\model\\" + modelName + "_x" + to_string(scale) + ".pb";
#else
	path = CFileUtility::GetResourcesFolderPath() + "/model/" + modelName + "_x" + to_string(scale) + ".pb";
#endif

	return CConvertUtility::ConvertToStdString(path);
}

bool CSuperSampling::TestIfMethodIsValid(int method, int scale)
{
	if (method == EDSR && (scale == 2 || scale == 3 || scale == 4))
	{
		return true;
	}
	else if (method == ESPCN && (scale == 2 || scale == 3 || scale == 4))
	{
		return true;
	}
	else if (method == FSRCNN && (scale == 2 || scale == 3 || scale == 4))
	{
		return true;
	}
	else if (method == LapSRN && (scale == 2 || scale == 4 || scale == 8))
	{
		return true;
	}
	return false;
}



COpenCLFilter::COpenCLFilter()
{
	bool useMemory = (cv::ocl::Device::getDefault().type() == CL_DEVICE_TYPE_GPU) ? false : true;
	flag = useMemory ? CL_MEM_USE_HOST_PTR : CL_MEM_COPY_HOST_PTR;
	hq3d = nullptr;
	superSampling = new CSuperSampling();
}

COpenCLFilter::~COpenCLFilter()
{
	if (hq3d != nullptr)
		delete hq3d;
	delete superSampling;
}


void COpenCLFilter::BilateralEffect(cv::UMat& inputData, const int& fSize, const int& sigmaX, const int& sigmaP)
{
	try
	{
		cv::UMat dest;
		cv::bilateralFilter(inputData, dest, fSize, sigmaX, sigmaP, cv::BORDER_DEFAULT);
		dest.copyTo(inputData);

	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}

}


void COpenCLFilter::NlMeans(cv::UMat& inputData, const int& h, const int& hColor, const int& templateWindowSize, const int& searchWindowSize)
{
	try
	{
		cv::UMat ycbcr;
		cv::UMat yChannel;
		cv::UMat yChannelOut;

		cvtColor(inputData, ycbcr, cv::COLOR_BGR2YCrCb);

		// Extract the Y channel
		cv::extractChannel(ycbcr, yChannel, 0); cv::extractChannel(ycbcr, yChannel, 0);

		cv::fastNlMeansDenoising(yChannel, yChannelOut, h, templateWindowSize, searchWindowSize);

		// Merge the the color planes back into an Lab image
		cv::insertChannel(yChannelOut, ycbcr, 0);

		// convert back to RGB
		cv::cvtColor(ycbcr, inputData, cv::COLOR_YCrCb2BGR);

		// Temporary Mat not reused, so release from memory.
		yChannel.release();
		ycbcr.release();
		yChannelOut.release();

	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}


void COpenCLFilter::Bm3d(cv::UMat& inputData, const float& fSigma)
{
	try
	{
		cv::UMat ycbcr;
		cv::UMat yChannel;
		cv::UMat yChannelOut;

		cvtColor(inputData, ycbcr, cv::COLOR_BGR2YCrCb);

		// Extract the Y channel
		cv::extractChannel(ycbcr, yChannel, 0);

		cv::xphoto::bm3dDenoising(yChannel, yChannelOut, fSigma);

		// Merge the the color planes back into an Lab image
		cv::insertChannel(yChannelOut, ycbcr, 0);

		// convert back to RGB
		cv::cvtColor(ycbcr, inputData, cv::COLOR_YCrCb2BGR);

		// Temporary Mat not reused, so release from memory.
		yChannel.release();
		ycbcr.release();
		yChannelOut.release();
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}



void COpenCLFilter::BrightnessAndContrastAuto(cv::UMat& inputData, float clipHistPercent)
{
	try
	{
		int histSize = 256;
		float alpha, beta;
		double minGray = 0, maxGray = 0;

		cv::UMat gray;
		cvtColor(inputData, gray, cv::COLOR_BGR2GRAY);

		if (clipHistPercent == 0)
		{
			// keep full available range
			minMaxLoc(gray, &minGray, &maxGray);
		}
		else
		{
			cv::UMat h;

			std::vector<int> channels = { 0 }; // Analyze only the channel 0.
			std::vector<int> hsize = { 256 };
			//Quantize the intensities in the image using 256 levels even if all the levels are not present.
			std::vector<float> hranges = { 0, 256 }; // The range is between 0 - 255 (so less than 256).

			calcHist(std::vector<cv::UMat>(1, gray), channels, cv::noArray(), h, hsize, hranges);

			cv::Mat hist;

			h.copyTo(hist);

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

		convertScaleAbs(inputData, inputData, alpha, beta);
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}


//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
void COpenCLFilter::Fusion(cv::UMat& inputData, const cv::UMat& secondPictureData, const float& pourcentage)
{
	try
	{
		cv::UMat dst;
		float beta = (1.0 - pourcentage);
		cv::addWeighted(inputData, pourcentage, secondPictureData, beta, 0.0, dst);
		dst.copyTo(inputData);
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}


void COpenCLFilter::SharpenMasking(const float& sharpness, cv::UMat& inputData)
{
	try
	{
        cv::UMat cvDestBgra;
        double sigma = 1;
		cv::GaussianBlur(inputData, cvDestBgra, cv::Size(), sigma, sigma);
        
        cv::UMat dest;
        cv::UMat inputDataBgra;
        cv::cvtColor(cvDestBgra, cvDestBgra, cv::COLOR_BGR2BGRA);
        cv::cvtColor(inputData, inputDataBgra, cv::COLOR_BGR2BGRA);

        vector<COpenCLParameter*> vecParam;
        cl_mem clBuffer = (cl_mem)inputDataBgra.handle(cv::ACCESS_READ);
        COpenCLParameterClMem* input = new COpenCLParameterClMem(true);
        input->SetValue(clBuffer);
        input->SetLibelle("input");
        input->SetNoDelete(true);
        vecParam.push_back(input);
        
        cl_mem clBuffer_out = (cl_mem)cvDestBgra.handle(cv::ACCESS_READ);
        COpenCLParameterClMem* gauss = new COpenCLParameterClMem(true);
        gauss->SetValue(clBuffer_out);
        gauss->SetLibelle("gaussian");
        gauss->SetNoDelete(true);
        vecParam.push_back(gauss);


        COpenCLParameterInt* paramWidth = new COpenCLParameterInt();
        paramWidth->SetValue(inputData.size().width);
        paramWidth->SetLibelle("width");
        vecParam.push_back(paramWidth);

        COpenCLParameterInt* paramHeight = new COpenCLParameterInt();
        paramHeight->SetValue(inputData.size().height);
        paramHeight->SetLibelle("height");
        vecParam.push_back(paramHeight);

        COpenCLParameterFloat * paramThreshold = new COpenCLParameterFloat();
        paramThreshold->SetLibelle("sharpness");
        paramThreshold->SetValue(sharpness);
        vecParam.push_back(paramThreshold);

        dest = ExecuteOpenCLCode("IDR_OPENCL_SHARPENMASKING", "SharpenMasking", vecParam, inputData.size().width, inputData.size().height);

        for (COpenCLParameter* parameter : vecParam)
        {
            if (!parameter->GetNoDelete())
            {
                delete parameter;
                parameter = nullptr;
            }
        }

        cv::cvtColor(dest, inputData, cv::COLOR_BGRA2BGR);

	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}

void COpenCLFilter::PhotoFiltre(const CRgbaquad& clValue, const int& intensity, cv::UMat& inputData)
{
	try
	{
		float coeff = (float)intensity / 100.0f;
		float diff = 1.0f - coeff;
		cv::UMat out;
		cv::UMat out_one;
		out_one = inputData.mul(diff);

		cv::Scalar color = cv::Scalar(clValue.GetBlue(), clValue.GetGreen(), clValue.GetRed());
		cv::Scalar out_two = color * coeff;

		cv::add(out_one, out_two, out);
		out.copyTo(inputData);
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}

void COpenCLFilter::RGBFilter(const int& red, const int& green, const int& blue, cv::UMat& inputData)
{
	try
	{
		cv::UMat out;
		cv::Scalar color = cv::Scalar(blue, green, red);
		cv::add(inputData, color, out);
		out.copyTo(inputData);
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}


}

void COpenCLFilter::FiltreMosaic(cv::UMat& inputData, const int& size)
{
	try
	{
		cv::UMat dest;
		cv::UMat cvDestBgra;
		cv::cvtColor(inputData, cvDestBgra, cv::COLOR_BGR2BGRA);

		vector<COpenCLParameter*> vecParam;
		cl_mem clBuffer = (cl_mem)cvDestBgra.handle(cv::ACCESS_READ);
		COpenCLParameterClMem* input = new COpenCLParameterClMem(true);
		input->SetValue(clBuffer);
		input->SetLibelle("input");
		input->SetNoDelete(true);
		vecParam.push_back(input);

		COpenCLParameterInt* paramWidth = new COpenCLParameterInt();
		paramWidth->SetValue(inputData.cols);
		paramWidth->SetLibelle("width");
		vecParam.push_back(paramWidth);

		COpenCLParameterInt* paramHeight = new COpenCLParameterInt();
		paramHeight->SetValue(inputData.rows);
		paramHeight->SetLibelle("height");
		vecParam.push_back(paramHeight);

		COpenCLParameterFloat* paramAngle = new COpenCLParameterFloat();
		paramAngle->SetLibelle("fTileSize");
		paramAngle->SetValue(size);
		vecParam.push_back(paramAngle);

		dest = ExecuteOpenCLCode("IDR_OPENCL_MOSAIC", "Mosaic", vecParam, inputData.size().width, inputData.size().height);

		for (COpenCLParameter* parameter : vecParam)
		{
			if (!parameter->GetNoDelete())
			{
				delete parameter;
				parameter = nullptr;
			}
		}

		cv::cvtColor(dest, inputData, cv::COLOR_BGRA2BGR);
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}

void COpenCLFilter::Blur(const int& radius, cv::UMat& inputData)
{
	try
	{
		cv::blur(inputData, inputData, cv::Size(radius, radius));
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}

void COpenCLFilter::GaussianBlur(const int& radius, const int& boxSize, cv::UMat& inputData)
{
	try
	{
		cv::GaussianBlur(inputData, inputData, cv::Size(boxSize, boxSize), radius);
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}

void COpenCLFilter::MotionBlurCompute(const vector<double>& kernelMotion, const vector<wxPoint>& offsets, const int& size, cv::UMat& inputData)
{
	cv::UMat dest;
	cv::UMat cvDestBgra;
	cv::cvtColor(inputData, cvDestBgra, cv::COLOR_BGR2BGRA);

	vector<COpenCLParameter*> vecParam;
	cl_mem clBuffer = (cl_mem)cvDestBgra.handle(cv::ACCESS_READ);
	COpenCLParameterClMem* input = new COpenCLParameterClMem(true);
	input->SetValue(clBuffer);
	input->SetLibelle("input");
	input->SetNoDelete(true);
	vecParam.push_back(input);

	COpenCLParameterInt* paramWidth = new COpenCLParameterInt();
	paramWidth->SetValue(inputData.cols);
	paramWidth->SetLibelle("width");
	vecParam.push_back(paramWidth);

	COpenCLParameterInt* paramHeight = new COpenCLParameterInt();
	paramHeight->SetValue(inputData.rows);
	paramHeight->SetLibelle("height");
	vecParam.push_back(paramHeight);

	float* kernel = new float[kernelMotion.size()];
	for (auto i = 0; i < kernelMotion.size(); i++)
		kernel[i] = kernelMotion[i];

	COpenCLParameterFloatArray* paramkernelMotion = new COpenCLParameterFloatArray();
	paramkernelMotion->SetLibelle("kernelMotion");
	paramkernelMotion->SetValue((cl_context)cv::ocl::Context::getDefault(false).ptr(), kernel, size, flag);
	vecParam.push_back(paramkernelMotion);

	int* offsetsMotion = new int[kernelMotion.size() * 2];
	for (auto i = 0, j = 0; i < offsets.size(); i++, j += 2)
	{
		offsetsMotion[j] = offsets[i].x;
		offsetsMotion[j + 1] = offsets[i].y;
	}

	COpenCLParameterIntArray* paramoffsets = new COpenCLParameterIntArray();
	paramoffsets->SetLibelle("offsets");
	paramoffsets->SetValue((cl_context)cv::ocl::Context::getDefault(false).ptr(), offsetsMotion, size * 2, flag);
	vecParam.push_back(paramoffsets);


	COpenCLParameterInt* paramkernelSize = new COpenCLParameterInt();
	paramkernelSize->SetLibelle("kernelSize");
	paramkernelSize->SetValue(size);
	vecParam.push_back(paramkernelSize);

	dest = ExecuteOpenCLCode("IDR_OPENCL_MOTIONBLUR", "MotionBlur", vecParam, inputData.size().width, inputData.size().height);

	for (COpenCLParameter* parameter : vecParam)
	{
		if (!parameter->GetNoDelete())
		{
			delete parameter;
			parameter = nullptr;
		}
	}


	delete[] kernel;
	delete[] offsetsMotion;

	cv::cvtColor(dest, inputData, cv::COLOR_BGRA2BGR);

}

void COpenCLFilter::Emboss(cv::UMat& inputData)
{
	try
	{
		// Construct kernel (all entries initialized to 0)
		Mat kernel(3, 3, CV_32F, Scalar(0));
		kernel.at<float>(0, 0) = -1.0;
		kernel.at<float>(2, 2) = 1.0;
		UMat dest;
		filter2D(inputData, dest, inputData.depth(), kernel);
		dest.copyTo(inputData);
		dest.release();
		kernel.release();
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}


}

void COpenCLFilter::Sharpen(cv::UMat& inputData)
{
	try
	{
		// Construct kernel (all entries initialized to 0)
		// Construct kernel (all entries initialized to 0)
		cv::Mat kernel(3, 3, CV_32F, cv::Scalar(0));
		// assigns kernel values
		kernel.at<float>(1, 1) = 5.0;
		kernel.at<float>(0, 1) = -1.0;
		kernel.at<float>(2, 1) = -1.0;
		kernel.at<float>(1, 0) = -1.0;
		kernel.at<float>(1, 2) = -1.0;

		UMat dest;
		filter2D(inputData, dest, inputData.depth(), kernel);
		dest.copyTo(inputData);
		dest.release();
		kernel.release();
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}

void COpenCLFilter::SharpenStrong(cv::UMat& inputData)
{
	try
	{
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
		UMat dest;
		filter2D(inputData, dest, inputData.depth(), kernel);
		dest.copyTo(inputData);
		dest.release();
		kernel.release();
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}


}

void COpenCLFilter::Edge(cv::UMat& inputData)
{
	try
	{
		UMat dest;
		cvtColor(inputData, dest, COLOR_BGR2GRAY);

		Mat img_blur;
		cv::GaussianBlur(dest, img_blur, cv::Size(3, 3), 0, 0);
		UMat edges;
		Canny(img_blur, edges, 100, 200, 3, false);

		cvtColor(edges, inputData, COLOR_GRAY2BGR);
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}

void COpenCLFilter::FiltreConvolution(const wxString& programName, const wxString& functionName, cv::UMat& inputData)
{
	cv::UMat dest;
	cv::UMat cvDestBgra;
	cv::cvtColor(inputData, cvDestBgra, cv::COLOR_BGR2BGRA);

	vector<COpenCLParameter*> vecParam;
	cl_mem clBuffer = (cl_mem)cvDestBgra.handle(cv::ACCESS_READ);
	COpenCLParameterClMem* input = new COpenCLParameterClMem(true);
	input->SetValue(clBuffer);
	input->SetLibelle("input");
	input->SetNoDelete(true);
	vecParam.push_back(input);

	COpenCLParameterInt* paramWidth = new COpenCLParameterInt();
	paramWidth->SetValue(cvDestBgra.cols);
	paramWidth->SetLibelle("width");
	vecParam.push_back(paramWidth);

	COpenCLParameterInt* paramHeight = new COpenCLParameterInt();
	paramHeight->SetValue(cvDestBgra.rows);
	paramHeight->SetLibelle("height");
	vecParam.push_back(paramHeight);

	dest = ExecuteOpenCLCode(programName, functionName, vecParam, cvDestBgra.size().width, cvDestBgra.size().height);

	for (COpenCLParameter* parameter : vecParam)
	{
		if (!parameter->GetNoDelete())
		{
			delete parameter;
			parameter = nullptr;
		}
	}

	cv::cvtColor(dest, inputData, cv::COLOR_BGRA2BGR);
}

void COpenCLFilter::ErodeDilate(const wxString& functionName, cv::UMat& inputData)
{
	try
	{
		if (functionName == "Erode")
			cv::erode(inputData, inputData, cv::Mat());
		else if (functionName == "Dilate")
			cv::dilate(inputData, inputData, cv::Mat());

	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}

void COpenCLFilter::Posterize(const float& level, const float& gamma, cv::UMat& inputData)
{
	cv::UMat dest;
	cv::UMat cvDestBgra;
	cv::cvtColor(inputData, cvDestBgra, cv::COLOR_BGR2BGRA);

	vector<COpenCLParameter*> vecParam;
	cl_mem clBuffer = (cl_mem)cvDestBgra.handle(cv::ACCESS_READ);
	COpenCLParameterClMem* input = new COpenCLParameterClMem(true);
	input->SetValue(clBuffer);
	input->SetLibelle("input");
	input->SetNoDelete(true);
	vecParam.push_back(input);

	COpenCLParameterInt* paramWidth = new COpenCLParameterInt();
	paramWidth->SetValue(cvDestBgra.size().width);
	paramWidth->SetLibelle("width");
	vecParam.push_back(paramWidth);

	COpenCLParameterInt* paramHeight = new COpenCLParameterInt();
	paramHeight->SetValue(cvDestBgra.size().height);
	paramHeight->SetLibelle("height");
	vecParam.push_back(paramHeight);

	COpenCLParameterInt* paramLevel = new COpenCLParameterInt();
	paramLevel->SetLibelle("level");
	paramLevel->SetValue(level);
	vecParam.push_back(paramLevel);

	dest = ExecuteOpenCLCode("IDR_OPENCL_COLOR", "Posterisation", vecParam, inputData.size().width, inputData.size().height);

	for (COpenCLParameter* parameter : vecParam)
	{
		if (!parameter->GetNoDelete())
		{
			delete parameter;
			parameter = nullptr;
		}
	}

	cv::cvtColor(dest, inputData, cv::COLOR_BGRA2BGR);
}


void COpenCLFilter::LensDistortion(const float& strength, cv::UMat& inputData)
{
	cv::UMat dest;
	cv::UMat cvDestBgra;
	cv::cvtColor(inputData, cvDestBgra, cv::COLOR_BGR2BGRA);

	double _strength = (double)strength / 100;
	double correctionRadius = sqrt(pow(inputData.rows, 2) + pow(inputData.cols, 2)) / _strength;

	vector<COpenCLParameter*> vecParam;
	cl_mem clBuffer = (cl_mem)cvDestBgra.handle(cv::ACCESS_READ);
	COpenCLParameterClMem* input = new COpenCLParameterClMem(true);
	input->SetValue(clBuffer);
	input->SetLibelle("input");
	input->SetNoDelete(true);
	vecParam.push_back(input);

	COpenCLParameterInt* paramWidth = new COpenCLParameterInt();
	paramWidth->SetValue(cvDestBgra.size().width);
	paramWidth->SetLibelle("width");
	vecParam.push_back(paramWidth);

	COpenCLParameterInt* paramHeight = new COpenCLParameterInt();
	paramHeight->SetValue(cvDestBgra.size().height);
	paramHeight->SetLibelle("height");
	vecParam.push_back(paramHeight);


	COpenCLParameterFloat* paramLevel = new COpenCLParameterFloat();
	paramLevel->SetLibelle("correctionRadius");
	paramLevel->SetValue(correctionRadius);
	vecParam.push_back(paramLevel);

	dest = ExecuteOpenCLCode("IDR_OPENCL_DISTORTION", "Distortion", vecParam, inputData.size().width, inputData.size().height);

	for (COpenCLParameter* parameter : vecParam)
	{
		if (!parameter->GetNoDelete())
		{
			delete parameter;
			parameter = nullptr;
		}
	}

	cv::cvtColor(dest, inputData, cv::COLOR_BGRA2BGR);
}

int COpenCLFilter::GetRgbaBitmap(cl_mem cl_image, UMat& u)
{
	vector<COpenCLParameter*> vecParam;
	cl_mem clBuffer = (cl_mem)u.handle(cv::ACCESS_READ);

	COpenCLParameterClMem* input = new COpenCLParameterClMem(true);
	input->SetValue(clBuffer);
	input->SetLibelle("input");
	input->SetNoDelete(true);
	vecParam.push_back(input);

	COpenCLParameterInt* paramWidth = new COpenCLParameterInt();
	paramWidth->SetValue(u.size().width);
	paramWidth->SetLibelle("width");
	vecParam.push_back(paramWidth);

	COpenCLParameterInt* paramHeight = new COpenCLParameterInt();
	paramHeight->SetValue(u.size().height);
	paramHeight->SetLibelle("height");
	vecParam.push_back(paramHeight);

#ifdef __APPLE__
	ExecuteOpenCLCode("IDR_OPENCL_BITMAPCONVERSION", "BitmapToOpenGLTextureApple", vecParam, u.size().width, u.size().height, cl_image);// program->ExecuteProgram(programCL->GetProgram(), "BitmapToOpenGLTextureApple");
#else
	ExecuteOpenCLCode("IDR_OPENCL_BITMAPCONVERSION", "BitmapToOpenGLTexture", vecParam, u.size().width, u.size().height, cl_image); //program->ExecuteProgram(programCL->GetProgram(), "BitmapToOpenGLTexture");
#endif


	for (COpenCLParameter* parameter : vecParam)
	{
		if (!parameter->GetNoDelete())
		{
			delete parameter;
			parameter = nullptr;
		}
	}

	return 0;
}




void COpenCLFilter::Solarize(const long& threshold, cv::UMat& inputData)
{
	cv::UMat dest;
	cv::UMat cvDest;
	cv::UMat cvDestBgra;
	cv::cvtColor(inputData, cvDestBgra, cv::COLOR_BGR2BGRA);

	vector<COpenCLParameter*> vecParam;
	cl_mem clBuffer = (cl_mem)cvDestBgra.handle(cv::ACCESS_READ);
	COpenCLParameterClMem* input = new COpenCLParameterClMem(true);
	input->SetValue(clBuffer);
	input->SetLibelle("input");
	input->SetNoDelete(true);
	vecParam.push_back(input);


	COpenCLParameterInt* paramWidth = new COpenCLParameterInt();
	paramWidth->SetValue(cvDestBgra.size().width);
	paramWidth->SetLibelle("width");
	vecParam.push_back(paramWidth);

	COpenCLParameterInt* paramHeight = new COpenCLParameterInt();
	paramHeight->SetValue(cvDestBgra.size().height);
	paramHeight->SetLibelle("height");
	vecParam.push_back(paramHeight);

	COpenCLParameterInt* paramThreshold = new COpenCLParameterInt();
	paramThreshold->SetLibelle("threshold");
	paramThreshold->SetValue((int)threshold);
	vecParam.push_back(paramThreshold);

	dest = ExecuteOpenCLCode("IDR_OPENCL_COLOR", "Solarization", vecParam, inputData.size().width, inputData.size().height);

	for (COpenCLParameter* parameter : vecParam)
	{
		if (!parameter->GetNoDelete())
		{
			delete parameter;
			parameter = nullptr;
		}
	}

	cv::cvtColor(dest, inputData, cv::COLOR_BGRA2BGR);

}

void COpenCLFilter::Median(cv::UMat& inputData)
{
	try
	{
		cv::medianBlur(inputData, inputData, 3);

	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}

void COpenCLFilter::Noise(cv::UMat& inputData)
{
	cv::UMat dest;
	cv::UMat cvDestBgra;
	cv::cvtColor(inputData, cvDestBgra, cv::COLOR_BGR2BGRA);

	vector<COpenCLParameter*> vecParam;
	cl_mem clBuffer = (cl_mem)cvDestBgra.handle(cv::ACCESS_READ);
	COpenCLParameterClMem* input = new COpenCLParameterClMem(true);
	input->SetValue(clBuffer);
	input->SetLibelle("input");
	input->SetNoDelete(true);
	vecParam.push_back(input);

	COpenCLParameterInt* paramWidth = new COpenCLParameterInt();
	paramWidth->SetValue(inputData.cols);
	paramWidth->SetLibelle("width");
	vecParam.push_back(paramWidth);

	COpenCLParameterInt* paramHeight = new COpenCLParameterInt();
	paramHeight->SetValue(inputData.rows);
	paramHeight->SetLibelle("height");
	vecParam.push_back(paramHeight);

	dest = ExecuteOpenCLCode("IDR_OPENCL_NOISE", "Noise", vecParam, inputData.size().width, inputData.size().height);

	for (COpenCLParameter* parameter : vecParam)
	{
		if (!parameter->GetNoDelete())
		{
			delete parameter;
			parameter = nullptr;
		}
	}

	cv::cvtColor(dest, inputData, cv::COLOR_BGRA2BGR);
}


void COpenCLFilter::Flip(const wxString& functionName, cv::UMat& inputData)
{
	if (functionName == "FlipVertical")
	{
		cv::flip(inputData, inputData, 0);
	}
	else
	{
		cv::flip(inputData, inputData, 1);
	}
}

void COpenCLFilter::Swirl(const float& radius, const float& angle, cv::UMat& inputData)
{
	cv::UMat dest;
	cv::UMat cvDest;
	cv::UMat cvDestBgra;
	cv::cvtColor(inputData, cvDestBgra, cv::COLOR_BGR2BGRA);

	vector<COpenCLParameter*> vecParam;
	cl_mem clBuffer = (cl_mem)cvDestBgra.handle(cv::ACCESS_READ);

	COpenCLParameterClMem* input = new COpenCLParameterClMem(true);
	input->SetValue(clBuffer);
	input->SetLibelle("input");
	input->SetNoDelete(true);
	vecParam.push_back(input);

	COpenCLParameterInt* paramWidth = new COpenCLParameterInt();
	paramWidth->SetValue(inputData.cols);
	paramWidth->SetLibelle("width");
	vecParam.push_back(paramWidth);

	COpenCLParameterInt* paramHeight = new COpenCLParameterInt();
	paramHeight->SetValue(inputData.rows);
	paramHeight->SetLibelle("height");
	vecParam.push_back(paramHeight);

	COpenCLParameterFloat* paramRadius = new COpenCLParameterFloat();
	paramRadius->SetLibelle("radius");
	paramRadius->SetValue(radius);
	vecParam.push_back(paramRadius);

	COpenCLParameterFloat* paramAngle = new COpenCLParameterFloat();
	paramAngle->SetLibelle("angle");
	paramAngle->SetValue(angle);
	vecParam.push_back(paramAngle);


	dest = ExecuteOpenCLCode("IDR_OPENCL_SWIRL", "Swirl", vecParam, inputData.size().width, inputData.size().height);

	for (COpenCLParameter* parameter : vecParam)
	{
		if (!parameter->GetNoDelete())
		{
			delete parameter;
			parameter = nullptr;
		}
	}

	cv::cvtColor(dest, inputData, cv::COLOR_BGRA2BGR);
}

void COpenCLFilter::BrightnessAndContrast(const double& brightness, const double& contrast, cv::UMat& inputData)
{
	cv::UMat cvDest;
	try
	{
		cv::convertScaleAbs(inputData, cvDest, contrast / 100.0f, brightness);
		cvDest.copyTo(inputData);
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}


void COpenCLFilter::ColorEffect(const wxString& functionName, cv::UMat& inputData)
{
	cv::UMat cvDest;
	try
	{
		if (functionName == "Sepia")
		{
			cv::Mat kernel =
				(cv::Mat_<float>(3, 3)
					<<
					0.272, 0.534, 0.131,
					0.349, 0.686, 0.168,
					0.393, 0.769, 0.189);

			cv::transform(inputData, inputData, kernel);
		}
		else if (functionName == "Negatif")
		{
			cv::bitwise_not(inputData, inputData);
		}
		else if (functionName == "NoirEtBlanc")
		{
			cv::cvtColor(inputData, cvDest, cv::COLOR_BGR2GRAY);
			cv::threshold(cvDest, cvDest, 127, 255, cv::THRESH_BINARY);
			cv::cvtColor(cvDest, inputData, cv::COLOR_GRAY2BGR);
		}
		else if (functionName == "GrayLevel")
		{
			cv::cvtColor(inputData, cvDest, cv::COLOR_BGR2GRAY);
			cv::cvtColor(cvDest, inputData, cv::COLOR_GRAY2BGR);
		}
		cvDest.release();
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}

void COpenCLFilter::HQDn3D(const double& LumSpac, const double& ChromSpac, const double& LumTmp, const double& ChromTmp, cv::UMat& inputData)
{
	if (hq3d == nullptr)
		hq3d = new Chqdn3d(inputData.cols, inputData.rows, LumSpac, LumTmp);
	else if (oldLevelDenoise != LumSpac || inputData.cols != oldwidthDenoise || inputData.rows != oldheightDenoise)
	{
		delete hq3d;
		hq3d = new Chqdn3d(inputData.cols, inputData.rows, LumSpac, LumTmp);
	}
	try
	{
		cv::UMat ycbcr;
		cv::Mat yChannel;
		cv::Mat yChannelOut;

		cvtColor(inputData, ycbcr, cv::COLOR_BGR2YCrCb);

		std::vector<cv::Mat> planes(3);
		cv::split(ycbcr, planes);

		// Extract the Y channel
		//cv::extractChannel(ycbcr, yChannel, 0);

		uint8_t* dataOut = hq3d->ApplyDenoise3D(planes[0].data, inputData.cols, inputData.rows);

		memcpy(planes[0].data, dataOut, inputData.cols * inputData.rows);

		// Merge the the color planes back into an Lab image
		//cv::insertChannel(yChannel, ycbcr, 0);
		cv::merge(planes, ycbcr);
		// convert back to RGB
		cv::cvtColor(ycbcr, inputData, cv::COLOR_YCrCb2BGR);

		ycbcr.release();
		yChannel.release();
		yChannelOut.release();
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;
	}
}

void COpenCLFilter::Rotate(const wxString& functionName, const int& widthOut, const int& heightOut, const double& angle, cv::UMat& inputData)
{
	cv::UMat cvDest;
	// get rotation matrix for rotating the image around its center in pixel coordinates
	const Point2f center((inputData.cols - 1) / 2.0, (inputData.rows - 1) / 2.0);
	Mat rot = getRotationMatrix2D(center, angle, 1.0);
	// determine bounding rectangle, center not relevant
	Rect2f bbox = RotatedRect(Point2f(), inputData.size(), angle).boundingRect2f();
	// adjust transformation matrix
	rot.at<double>(0, 2) += bbox.width / 2.0 - inputData.cols / 2.0;
	rot.at<double>(1, 2) += bbox.height / 2.0 - inputData.rows / 2.0;

	warpAffine(inputData, cvDest, rot, bbox.size());

	rot.release();
	inputData.release();
	cvDest.copyTo(inputData);
	cvDest.release();
}

cv::Rect COpenCLFilter::CalculRect(int widthIn, int heightIn, int widthOut, int heightOut, int flipH, int flipV, int angle, float ratioX, float ratioY, int x, int y, float left, float top)
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

void COpenCLFilter::ExecuteOpenCLCode(const wxString& programName, const wxString& functionName, vector<COpenCLParameter*>& vecParam, const int& width, const int& height, cl_mem& outBuffer)
{
	wxString kernelSource = CLibResource::GetOpenCLUcharProgram(programName);
	cv::ocl::ProgramSource programSource(kernelSource);
	cv::ocl::Context context = cv::ocl::Context::getDefault(false);

	// Compile the kernel code
	cv::String errmsg;
	cv::String buildopt = "";// cv::format("-D dstT=%s", cv::ocl::typeToStr(paramSrc.depth()));
	cv::ocl::Program program = context.getProg(programSource, buildopt, errmsg);

	cv::ocl::Kernel kernel(functionName, program);

	//cl_mem clBuffer = (cl_mem)paramSrc.handle(cv::ACCESS_WRITE);

	cl_int err = clSetKernelArg((cl_kernel)kernel.ptr(), 0, sizeof(cl_mem), &outBuffer);
	if (err > 0)
		throw "OpenCL Kernel Error";

	int numArg = 1;
	for (auto it = vecParam.begin(); it != vecParam.end(); ++it)
	{
		COpenCLParameter* parameter = *it;
		parameter->Add((cl_kernel)kernel.ptr(), numArg++);
	}

	size_t global_work_size[2] = { (size_t)width, (size_t)height };
	//size_t localThreads[2] = { 16, 16 };
	bool success = kernel.run(2, global_work_size, nullptr, true);
	if (!success) {
		cout << "Failed running the kernel..." << endl;
		if (err > 0)
			throw "OpenCL Kernel run";
		return;
	}

	for (auto it = vecParam.begin(); it != vecParam.end(); ++it)
	{
		COpenCLParameter* parameter = *it;
		if (!parameter->GetNoDelete())
			parameter->Release();
	}
}

cv::UMat COpenCLFilter::ExecuteOpenCLCode(const wxString & programName, const wxString & functionName, vector<COpenCLParameter*> & vecParam, const int &width, const int &height)
{
	UMat paramSrc;


	int depth = CV_8U;
	int type = CV_MAKE_TYPE(depth, 4);
	paramSrc.create((int)height, (int)width, type);

	cl_mem clBuffer = (cl_mem)paramSrc.handle(cv::ACCESS_WRITE);

	ExecuteOpenCLCode(programName, functionName, vecParam, width, height, clBuffer);

	return paramSrc;
}



cv::UMat COpenCLFilter::Interpolation(const int& widthOut, const int& heightOut, const wxRect& rc, const int& method, cv::UMat& inputData, int flipH, int flipV, int angle, int ratio)
{
	cv::UMat cvImage;
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
		cv::Rect rect_begin = CalculRect(inputData.cols, inputData.rows, widthOut, heightOut, flipH, flipV, angle, ratioX, ratioY, 0, 0, rc.x, rc.y);
		cv::Rect rect_end = CalculRect(inputData.cols, inputData.rows, widthOut, heightOut, flipH, flipV, angle, ratioX, ratioY, widthOut, heightOut, rc.x, rc.y);
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

		//cv::UMat crop;
		inputData(rectGlobal).copyTo(cvImage);
		//Mat global;
		//cvImage.copyTo(global);
		//crop.copyTo(cvImage);
		//cvImage = cvImage(rectGlobal);

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


		CRegardsConfigParam* regardsParam = CParamInit::getInstance();

		if (cvImage.cols != widthOut || cvImage.rows != heightOut)
		{
			cv::resize(cvImage, cvImage, cv::Size(widthOut, heightOut), method);
		}

		if (cvImage.cols != widthOut || cvImage.rows != heightOut)
			cv::resize(cvImage, cvImage, cv::Size(widthOut, heightOut), method);

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
		//
	}
	catch (cv::Exception& e)
	{
		const char* err_msg = e.what();
		std::cout << "exception caught: " << err_msg << std::endl;
		std::cout << "wrong file format, please input the name of an IMAGE file" << std::endl;

	}


	return cvImage;
}
