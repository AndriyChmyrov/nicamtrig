/*******************************************************************************\
*										*
*		National Instruments NI-DAQmx driver interface			*
*			 Error logging and reporting				*
*										*
*			Marcel Leutenegger © 1.5.2010				*
*										*
\*******************************************************************************/

#include "stdafx.h"


static	int errors[32], next;


/*	Check for and throw NI-DAQmx messages.
*/
void DAQError(const int error, const char* file, const int line)
{	
	if (error)
	{	
		mexPrintf("%s:%d - ",file,line);
		int size = DAQmxGetErrorString(error, NULL, 0);
		char* message = static_cast<char*>(mxMalloc(size));
		if (DAQmxGetErrorString(error, message, size))
		{	
			if (error < 0) mexErrMsgTxt("Error occured: no information available.");
			mexWarnMsgTxt("Warning occured: no information available.");
		}
		else	if (error < 0) mexErrMsgTxt(message);
		else	mexWarnMsgTxt(message);
	}
}


/*	Get the number of logged errors and report them (first occured first reported).
*/
int getErrors(int* list, int size)
{	int last=next, many=0, n=0;
	for(n; n < 32; n++) if (errors[n]) many++;
	if (many && list && size > 0)
	{	last=(last-many)&31;
		size=min(many,size);
		while(--size >= 0)
		{	*list++=errors[last];
			errors[last]=0;
			last=(last+1)&31;
		}
	}
	return many;
}


/*	Check for and log NI-DAQmx errors.
*/
void logError(const int error)
{	if (error < 0)
	{	errors[next]=error;
		next=(next+1)&31;
	}
}


/*	Throw the last system error message.
*/
void WINError(const BOOL success, const char* file, const int line)
{	if (!success)
	{	TCHAR message[512];
		char messg[512];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,NULL,GetLastError(),MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),message,512,NULL);
		mexPrintf("%s:%d - ",file,line);
		size_t *pReturnValue = NULL;
		wcstombs_s(pReturnValue, messg, 512, message, 512);
		mexErrMsgTxt(messg);
	}
}
