# nicamtrig
mex function which makes triggered analog (and possibly digital) output

The code creates two tasks (analog output and digital outputs) which can be synchronized either to each other 
(in case of using no external trigger, selfTrigger == true), or to an additional task which is called SampleClock (in case of external triggering)

For external trigger mode using the SampleClock task we define a blockSize - number of values which will be written on the output task
following each incoming trigger.
