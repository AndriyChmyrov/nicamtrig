/*******************************************************************************\
*																				*
*		    NI-DAQmx pulse output function										*
*			based on the code of Marcel Leutenegger								*
*																				*
*			Andriy Chmyrov © 08.07.2016											*
*			Marcel Leutenegger © 15.6.2010										*
*																				*
\*******************************************************************************/

#include "stdafx.h"


static TaskHandle analog, digital, sampleClock;
static HANDLE readySc;
//static int queue;
static bool selfTrigger = true;


/*	Clear counter input task.
*/
int32 CVICALLBACK clear(TaskHandle task, int32 error)
{
	DAQmxClearTask(task);
	return error;
}


/*	Zero outputs and clear output tasks.
*/
int32 CVICALLBACK reset(TaskHandle task, int32 error)
{
	UNREFERENCED_PARAMETER(task);
//	if (DAQmxClearTask(digital) >= 0) digital = 0;
	if (DAQmxClearTask(analog) >= 0) analog = 0;
	if (!selfTrigger && DAQmxClearTask(sampleClock) >= 0) sampleClock = 0;

	return error;
}


/*	Check the task activity.
*/
bool taskIsDone(TaskHandle task)
{
	bool32 done = TRUE;
	int32 error = DAQmxIsTaskDone(task, &done);
	if (error < 0)
	{
		if (error == DAQmxErrorInvalidTask)
			done = TRUE;
		else
			daqError(error);
	}
	return done != 0;
}


/*	Free all tasks.
*/
static void mexClear(void)
{
	int32 analogClearRes;
	analogClearRes = DAQmxClearTask(analog);
	if (analog && analogClearRes >= 0)
	{
		analog = 0;
	}
//	if (digital && DAQmxClearTask(digital) >= 0)
//	{
//		digital = 0;
//	}
	if (!selfTrigger && sampleClock && DAQmxClearTask(sampleClock) >= 0)
	{
		sampleClock = 0;
	}
	if (readySc && CloseHandle(readySc))
	{
		readySc = 0;
	}
}


/*	Free all resources.
*/
static void mexFree(void)
{
	mexClear();
}


/*	Check for and get a scalar value.
*/
double mexScalar(const mxArray* scalar)
{
	if (mxGetNumberOfElements(scalar) != 1
		|| mxIsComplex(scalar)
		|| !mxIsNumeric(scalar)) mexErrMsgTxt("Real scalar expected.");
	return mxGetScalar(scalar);
}


#define	WAIT	prhs[0]
#define	RATE	prhs[0]
#define	CHANS	prhs[1]
#define	VOLTS	prhs[2]
#define	LINES	prhs[3]
#define	BITS	prhs[4]
#define TRIG	prhs[5]
#define BLOCK	prhs[6]
#define	PULSES	prhs[7]

#define	ACTIVE	plhs[0]


/*	Synopsis:
nipulse(rate,channels,volts,lines,bits,trigger,blockSize);
nipulse(...,pulses);

nipulse(wait);

clear('nipulse');

Example:     ** non-updated example **
active = nipulse(1e4,'Dev1/ao2:3',volts,'Dev1/port0',bits,'/Dev1/pfi0',100);
while active
[counts,active]=spulse(1);
plot((0.5:numel(counts.profile))/counts.rate,counts.profile,'r');
drawnow;
end
*/
void mexFunction(int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[])
{
	if (nrhs == 0 && nlhs == 0)
	{
		mexPrintf("\nNI-DAQmx pulse output function with trigger line.\n\n\tAndriy Chmyrov based on code of Marcel Leutenegger © 27.2.2012\n\n");
		return;
	}
	mexAtExit(mexFree);

	char cTrigTerms[256];

	uInt64 pulses = 1;
	switch (nrhs)
	{
	default:
		mexErrMsgTxt("Invalid number of arguments.");
	case 8:
		pulses = static_cast<int>(mexScalar(PULSES));
		if (pulses < 1) mexErrMsgTxt("Invalid number of pulses");
	case 7:
	case 6:
		mxGetString(TRIG, cTrigTerms, sizeof(cTrigTerms));
		if ( strlen(cTrigTerms) )
			selfTrigger = false;
	case 5:
	{
		mexFree();

		daqError(DAQmxCreateTask(NULL, &analog));
		char terms[256];
		if (mxGetString(CHANS, terms, sizeof(terms))) mexErrMsgTxt("Invalid analog output channels.");
		daqError(DAQmxCreateAOVoltageChan(analog, terms, NULL, -10, 10, DAQmx_Val_Volts, NULL));
		uInt32 channels = 0;
		daqError(DAQmxGetWriteNumChans(analog, &channels));
		if (!mxIsDouble(VOLTS) || mxGetM(VOLTS) != channels) mexErrMsgTxt("Invalid output voltages.");

		/*
		daqError(DAQmxCreateTask(NULL, &digital));
		if (mxGetString(LINES, terms, sizeof(terms))) mexErrMsgTxt("Invalid digital output lines.");
		daqError(DAQmxCreateDOChan(digital, terms, NULL, DAQmx_Val_ChanForAllLines));
		char ch[256];
		daqError(DAQmxGetTaskChannels(digital, ch, sizeof(ch)));
		*/

		char device[128] = "/";
		daqError(DAQmxGetNthTaskDevice(analog, 1, device + 1, 127));
		int32 devBus;
		daqError(DAQmxGetDevBusType(device + 1, &devBus));
		if ((devBus == DAQmx_Val_PCI) || (devBus == DAQmx_Val_PCIe))
		{
			daqError(DAQmxSetAODataXferMech(analog, terms, DAQmx_Val_DMA));
			//daqError(DAQmxSetDODataXferMech(digital, ch, DAQmx_Val_DMA));
		}


		readySc = CreateEvent(NULL, false, false, NULL);
		winError(readySc != 0);

		float64 rate = mexScalar(RATE);

		int32 samples = static_cast<int32>(mxGetN(VOLTS));
		if (mxGetM(BITS) != 1 || mxGetN(BITS) != samples) mexErrMsgTxt("Number of samples mismatch.");
		if (pulses > 1)
		{
			daqError(DAQmxCfgOutputBuffer(analog, samples));
//			daqError(DAQmxCfgOutputBuffer(digital, samples));
			daqError(DAQmxSetWriteRegenMode(analog, DAQmx_Val_AllowRegen));
//			daqError(DAQmxSetWriteRegenMode(digital, DAQmx_Val_AllowRegen));
		}

		if (selfTrigger)
		{
			/*
			Synchronize all sample clocks on the AO sample clock.
			*/
			char Dev[256] = "";
			sprintf_s(Dev, 256, "%s%s", device, "/ao/SampleClock");
			daqError(DAQmxCfgSampClkTiming(analog, NULL, rate, DAQmx_Val_Rising, DAQmx_Val_ContSamps, pulses*samples+1));
//			daqError(DAQmxCfgSampClkTiming(digital, Dev, rate, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, pulses*samples+1));
		}
		else
		{
			/*
			Create SampleClock which will be triggered by an external pulse
			*/
			daqError(DAQmxCreateTask(NULL, &sampleClock));
			char Dev[256] = "";
			sprintf_s(Dev, 256, "%s%s", device, "/ctr0");
			daqError(DAQmxCreateCOPulseChanFreq(sampleClock, Dev, "", DAQmx_Val_Hz, DAQmx_Val_Low, 0, rate, 0.5));
			daqError(DAQmxCfgDigEdgeStartTrig(sampleClock, cTrigTerms, DAQmx_Val_Rising));
			uInt64 blockSize = static_cast<uInt64>(mexScalar(BLOCK));
			daqError(DAQmxCfgImplicitTiming(sampleClock, DAQmx_Val_FiniteSamps, blockSize));
			daqError(DAQmxSetStartTrigRetriggerable(sampleClock, TRUE));

			/*
			Synchronize all sample clocks on our own created SampleClock.
			*/
			char DevInt[256] = "";
			sprintf_s(DevInt, 256, "%s%s", device, "/Ctr0InternalOutput");
			daqError(DAQmxCfgSampClkTiming(analog, DevInt, rate, DAQmx_Val_Rising, DAQmx_Val_ContSamps, pulses*samples));
//			daqError(DAQmxCfgSampClkTiming(digital, DevInt, rate, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, pulses*samples));
		}
//		daqError(DAQmxRegisterDoneEvent(digital, 0, (DAQmxDoneEventCallbackPtr)reset, NULL));

		int32 written = 0;
		daqError(DAQmxWriteAnalogF64(analog, samples, false, 0, DAQmx_Val_GroupByScanNumber, mxGetPr(VOLTS), &written, NULL));
		/*
		switch (mxGetClassID(BITS))
		{
		default:
			mexErrMsgTxt("Invalid digital output.");
		case mxUINT8_CLASS:
			daqError(DAQmxWriteDigitalU8(digital, written, false, 0, DAQmx_Val_GroupByScanNumber, static_cast<const uInt8*>(mxGetData(BITS)), &written, NULL));
			break;
		case mxUINT16_CLASS:
			daqError(DAQmxWriteDigitalU16(digital, written, false, 0, DAQmx_Val_GroupByScanNumber, static_cast<const uInt16*>(mxGetData(BITS)), &written, NULL));
			break;
		case mxUINT32_CLASS:
			daqError(DAQmxWriteDigitalU32(digital, written, false, 0, DAQmx_Val_GroupByScanNumber, static_cast<const uInt32*>(mxGetData(BITS)), &written, NULL));
		}
		*/
		if (written < samples) mexErrMsgTxt("Could not write all samples.");

//		daqError(DAQmxStartTask(digital));
		daqError(DAQmxStartTask(analog));
		if (!selfTrigger)
			daqError(DAQmxStartTask(sampleClock));
		break;
	}
	case 1:
		if (!taskIsDone(analog))
		{
			double wait = mexScalar(WAIT);
			if (wait > 0)
			{
				int error = DAQmxWaitUntilTaskDone(analog, wait);
				if (error != DAQmxErrorTimeoutExceeded
					&& error != DAQmxErrorPALSyncTimedOut
					&& error != DAQmxErrorInvalidTask) daqError(error);
			}
		}

	case 0:;
	}
	switch (nlhs)
	{
	default:
		mexErrMsgTxt("Invalid number of results.");
	case 1:
		*mxGetPr(ACTIVE = mxCreateDoubleMatrix(1, 1, mxREAL)) = taskIsDone(analog) ? 0 : 1;
	case 0:;
	}
}
