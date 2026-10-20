#pragma once

// Developer tool (the PROJECTFOXY_DEV_SERVER build, Windows): a poor man's sampling profiler. startSampling() starts a
// thread that, every `intervalMs`, stops the calling thread, walks its stack and lets it go; stopSamplingAndReport()
// prints, on stderr, the functions that were on the stack most often (inclusive) and the ones the thread was running
// most often (self). Symbols come from the PDBs next to the program and from the export tables of the other DLLs.
void startSampling(int intervalMs = 1);
void stopSamplingAndReport(int top = 45);
