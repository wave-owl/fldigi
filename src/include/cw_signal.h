#ifndef CW_SIGNAL_H
#define CW_SIGNAL_H

struct cw_spectral_peak
{
	double frequency;
	double power;
	double noise;
	double snr;
	double width;
};

#endif // CW_SIGNAL_H
