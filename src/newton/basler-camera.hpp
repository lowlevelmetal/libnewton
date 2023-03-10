/*
 * Matthew Todd Geiger <mgeiger@newtonlabs.com>
 *
 * Newton Laboratories, libnewton
 *
 * \\\\\\\\\\\\\\\\\\\\\\\\\\\
 *  \--\ basler-camera.hpp \--\
 *   \\\\\\\\\\\\\\\\\\\\\\\\\\\
 */

#pragma once


#include "macro.hpp"
#include "heap.hpp"

#include <thread>
#include <mutex>

// Pylon includes
#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>

// Using a struct so that the code is simple to modify
typedef struct FrameGrabThreadData {
	Pylon::CBaslerUniversalInstantCamera *camera;
} FRAMEGRABDATA, *PFRAMEGRABDATA;

#define BASLER_NCHANNELS 3

class BaslerCamera {
	public:
		BaslerCamera(const char *camera_serial, int width, int height);
		~BaslerCamera();

		bool StartGrabbing();
		void StopGrabbing();

		void InformSize(int width, int height);

		// Use this to get local copy of buffer!
		bool CopyFrameBuffer(uint8_t *dest);
		void CopySize(int *width, int *height);
		bool SaveImage(std::string location);
		bool SetExposure(double exposuretime);
		bool GetMaxExposure(double *exposuretime);
		bool GetMinExposure(double *exposuretime);
		void SetAutoGain(bool autogain);
		void SetBrightness(double value);
		double GetMaxGain();
		double GetMinGain();
	private:
		void m_Initialize(const char *camera_serial);
		Pylon::DeviceInfoList_t::const_iterator m_FindCamera(const char * camera_serial,
				Pylon::CTlFactory& TlFactory,
				Pylon::DeviceInfoList_t& lstDevices);
		static void m_FrameGrabThread(PFRAMEGRABDATA framegrabdata);
		static bool m_CheckExit();

		static std::mutex m_exitmtx;
		static std::mutex m_framebuffermtx;
		static int m_width, m_height;

		FRAMEGRABDATA m_framegrabdata;
		Pylon::CBaslerUniversalInstantCamera *m_camera = nullptr;
		std::thread m_threadgrabthread;
};

#ifdef _CSHARP

extern "C" {

void WINEXPORT InitializeBaslerCamera(char *serial, int width, int height);
void WINEXPORT FreeBaslerCamera();
void WINEXPORT ChangeBaslerSize(int width, int height);
bool WINEXPORT CopyBaslerFrameToBuffer(uint8_t *buffer);

}

#endif
