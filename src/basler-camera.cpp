/*
 * Matthew Todd Geiger <mgeiger@newtonlabs.com>
 *
 * Newton Laboratories, libnewton
 *
 * \\\\\\\\\\\\\\\\\\\\\\\\\\\
 *  \--\ basler-camera.cpp \--\
 *   \\\\\\\\\\\\\\\\\\\\\\\\\\\
 */

#include "newton/basler-camera.hpp"
#include <pylon/TypeMappings.h>
#include <opencv2/opencv.hpp>

using namespace Pylon;
using namespace GenApi;
using namespace cv;

// Statics
static uint8_t *framebuffer = nullptr;
static bool grabbingframes = false;
static bool takephoto = false;
static std::string save_location;
std::mutex BaslerCamera::m_exitmtx;
std::mutex BaslerCamera::m_framebuffermtx;
int BaslerCamera::m_width;
int BaslerCamera::m_height;

void BaslerCamera::CopySize(int *width, int *height) {
	m_framebuffermtx.lock();
	*width = m_width;
	*height = m_height;
	m_framebuffermtx.unlock();
}

void BaslerCamera::InformSize(int width, int height) {
	m_framebuffermtx.lock();

	m_height = height;
	m_width = width;

	if(framebuffer != nullptr)
		free(framebuffer);

	framebuffer = (uint8_t *)ec_malloc(width * height * BASLER_NCHANNELS);

	m_framebuffermtx.unlock();
}

// Start frame grab thread
bool BaslerCamera::StartGrabbing() {
	if(m_camera == nullptr)
		return false;

	m_exitmtx.lock();
	if(grabbingframes == true)
		return false;

	grabbingframes = true;
	m_exitmtx.unlock();

	m_framegrabdata.camera = m_camera;
	m_threadgrabthread = std::thread(m_FrameGrabThread, &m_framegrabdata);

	return true;
}

void BaslerCamera::StopGrabbing() {
	if(grabbingframes == false)
		return;

	m_exitmtx.lock();
	grabbingframes = false;
	m_exitmtx.unlock();

	m_threadgrabthread.join();
}

bool BaslerCamera::m_CheckExit() {
	bool exit = false;

	m_exitmtx.lock();
	if(grabbingframes == false)
		exit = true;
	m_exitmtx.unlock();

	return exit;
}

void BaslerCamera::CopyFrameBuffer(uint8_t *dest) {
	m_framebuffermtx.lock();
	memcpy(dest, framebuffer, m_width * m_height * BASLER_NCHANNELS);
	m_framebuffermtx.unlock();
}

void BaslerCamera::SaveImage(std::string location) {
	m_framebuffermtx.lock();
	takephoto = true;
	save_location = location;
	m_framebuffermtx.unlock();
}

// Only one instance of this function can run at a time
void BaslerCamera::m_FrameGrabThread(PFRAMEGRABDATA framegrabdata) {
	CGrabResultPtr grabresult;

	CIntegerParameter width(framegrabdata->camera->GetNodeMap(), "Width");
    CIntegerParameter height(framegrabdata->camera->GetNodeMap(), "Height");

	// Start grabbing frames
	framegrabdata->camera->StartGrabbing();

	while(!m_CheckExit()) {
		framegrabdata->camera->RetrieveResult(8000, grabresult, TimeoutHandling_Return);	

		if(grabresult->GrabSucceeded()) {

			// Get pointer to basler buffer
			uint8_t *baslerbuffer = (uint8_t*)grabresult->GetBuffer();

			m_framebuffermtx.lock();
			// Hack way to resize image
			Mat img(Size(static_cast<int>(width.GetValue()), static_cast<int>(height.GetValue())), CV_8UC3, baslerbuffer); 

			if(takephoto) {
				takephoto = false;
				imwrite(save_location.c_str(), img);
			}

			resize(img, img, Size(m_width, m_height));
			if(framebuffer != nullptr)
				memcpy(framebuffer, img.data, m_width * m_height * BASLER_NCHANNELS);
			m_framebuffermtx.unlock();
		}
	}

	printf("Grab thread exiting\n");
	fflush(stdout);
}

void BaslerCamera::m_Initialize(const char *camera_serial) {
	// Initialize pylon
	PylonInitialize();

	CTlFactory& TlFactory = CTlFactory::GetInstance();
	DeviceInfoList_t lstDevices;

	try {
		// Find camera on network
		DeviceInfoList_t::const_iterator it = m_FindCamera(camera_serial, TlFactory, lstDevices);

		if(it == nullptr)
			throw GENERIC_EXCEPTION("Failed to find camera serial: %s", camera_serial);

		// Create and open camera
		m_camera = new CBaslerUniversalInstantCamera(TlFactory.CreateDevice(*it));
		m_camera->Open();

		// Configure camera settings and features
		CIntegerParameter width(m_camera->GetNodeMap(), "Width");
		CIntegerParameter height(m_camera->GetNodeMap(), "Height");
		CIntegerParameter offsetX(m_camera->GetNodeMap(), "OffsetX");
		CIntegerParameter offsetY(m_camera->GetNodeMap(), "OffsetY");
		CEnumParameter pixelFormat(m_camera->GetNodeMap(), "PixelFormat");

        width.TrySetToMaximum();
        height.TrySetToMaximum();
        offsetX.TrySetToMinimum();
        offsetY.TrySetToMinimum();
        pixelFormat.SetIntValue(PixelType_RGB8packed);

	} catch(const GenericException& e) {
		FATAL("An exception occured. %s", e.GetDescription());
	}
}

DeviceInfoList_t::const_iterator BaslerCamera::m_FindCamera(const char * camera_serial,
															CTlFactory& TlFactory,
															DeviceInfoList_t& lstDevices) {
    TlFactory.EnumerateDevices(lstDevices); // Get list of devices on network

    DeviceInfoList_t::const_iterator it; // iterate through devices to find one that matches serial number
    if(!lstDevices.empty()) {
        for(it = lstDevices.begin(); it != lstDevices.end(); ++it) {
            printf("Camera found on network: %s\n", it->GetSerialNumber().c_str());
			fflush(stdout);
            if(strcmp(it->GetSerialNumber().c_str(), camera_serial) == 0) {
                return it;
            }
        }
    }

    return nullptr;
}

BaslerCamera::BaslerCamera(const char *camera_serial, int width, int height) {
	InformSize(width, height);	

	m_Initialize(camera_serial);
}

BaslerCamera::~BaslerCamera() {
	StopGrabbing();

	if(m_camera != nullptr) {
		m_camera->Close();
		delete m_camera;
	}

	if(framebuffer != nullptr)
		free(framebuffer);

	PylonTerminate();
}
