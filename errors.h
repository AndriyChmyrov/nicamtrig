/*******************************************************************************\
*										*
*		National Instruments NI-DAQmx driver interface			*
*			 Error logging and reporting				*
*										*
*			Marcel Leutenegger © 1.5.2010				*
*										*
\*******************************************************************************/


/*	Check for and throw NI-DAQmx messages.
*/
void DAQError(const int error, const char* file, const int line);
#define daqError(error) DAQError((error),__FILE__,__LINE__)

/*	Get the number of logged errors and report them (first occured first reported).
*/
int getErrors(int* list, int size);

/*	Check for and log NI-DAQmx errors.
*/
void logError(const int error);

/*	Throw the last system error message.
*/
void WINError(const BOOL success, const char* file, const int line);
#define winError(error) WINError((error),__FILE__,__LINE__)
