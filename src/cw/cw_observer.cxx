#include "cw_observer.h"

#include <cstring>


cw_observation::cw_observation() :
	sequence(0),
	frequency(0.0),
	start_time(0.0),
	end_time(0.0),
	duration_ms(0.0),
	preceding_space_valid(false),
	preceding_space_ms(0.0),
	confidence(0.0),
	carrier_mean_hz(0.0),
	carrier_sigma_hz(0.0),
	carrier_coherence(0.0),
	carrier_evidence(0.0),
	recent_mean_hz(0.0),
	recent_sigma_hz(0.0),
	recent_median_hz(0.0),
	recent_mad_hz(0.0),
	recent_good_count(0),
	valid(false)
{
}


cw_signal_attributes::cw_signal_attributes() :
	frequency(0.0),
	start_sample(0),
	end_sample(0),
	duration_samples(0),
	confidence(0.0),
	carrier_mean_hz(0.0),
	carrier_sigma_hz(0.0),
	carrier_coherence(0.0),
	carrier_evidence(0.0),
	recent_mean_hz(0.0),
	recent_sigma_hz(0.0),
	recent_median_hz(0.0),
	recent_mad_hz(0.0),
	recent_good_count(0)
{
}


cw_observer::cw_observer() :
	sample_rate(8000.0),
	decimation(1),
	sequence(0),
	have_previous_mark(false),
	previous_end_sample(0),
	csv(0)
{
}


cw_observer::~cw_observer()
{
	close_csv();
}


void cw_observer::reset()
{
	sequence = 0;

	have_previous_mark = false;
	previous_end_sample = 0;
}


void cw_observer::set_sample_timing(double rate, unsigned dec)
{
	if (rate > 0.0)
		sample_rate = rate;

	if (dec > 0)
		decimation = dec;
}


double cw_observer::samples_to_seconds(unsigned long samples) const
{
	return
		static_cast<double>(samples) *
		static_cast<double>(decimation) /
		sample_rate;
}


double cw_observer::samples_to_ms(unsigned long samples) const
{
	return 1000.0 * samples_to_seconds(samples);
}


bool cw_observer::open_csv(const std::string& filename)
{
	close_csv();

	if (filename.empty())
		return false;

	csv = std::fopen(filename.c_str(), "w");

	if (!csv)
		return false;

	write_csv_header();

	return true;
}


void cw_observer::close_csv()
{
	if (csv) {
		std::fclose(csv);
		csv = 0;
	}
}


void cw_observer::write_csv_header()
{
	if (!csv)
		return;

	std::fprintf(csv,
		"sequence,"
		"frequency,"
		"start_time,"
		"end_time,"
		"duration_ms,"
		"preceding_space_valid,"
		"preceding_space_ms,"
		"confidence,"
		"carrier_mean_hz,"
		"carrier_sigma_hz,"
		"carrier_coherence,"
		"carrier_evidence,"
		"recent_mean_hz,"
		"recent_sigma_hz,"
		"recent_median_hz,"
		"recent_mad_hz,"
		"recent_good_count\n");
}


void cw_observer::write_csv(const cw_observation& obs)
{
	if (!csv || !obs.valid)
		return;

	std::fprintf(csv,
		"%lu,"
		"%.6f,"
		"%.6f,"
		"%.6f,"
		"%.3f,"
		"%d,"
		"%.3f,"
		"%.9g,"
		"%.9g,"
		"%.9g,"
		"%.9g,"
		"%.9g,"
		"%.9g,"
		"%.9g,"
		"%.9g,"
		"%.9g,"
		"%u\n",
		obs.sequence,
		obs.frequency,
		obs.start_time,
		obs.end_time,
		obs.duration_ms,
		obs.preceding_space_valid,
		obs.preceding_space_ms,
		obs.confidence,
		obs.carrier_mean_hz,
		obs.carrier_sigma_hz,
		obs.carrier_coherence,
		obs.carrier_evidence,
		obs.recent_mean_hz,
		obs.recent_sigma_hz,
		obs.recent_median_hz,
		obs.recent_mad_hz,
		obs.recent_good_count);
}


bool cw_observer::observe(const cw_signal_attributes& attr,
			  cw_observation& obs)
{
	obs = cw_observation();

	if (attr.duration_samples == 0)
		return false;

	obs.sequence = ++sequence;

	obs.frequency = attr.frequency;

	obs.start_time =
		samples_to_seconds(attr.start_sample);

	obs.end_time =
		samples_to_seconds(attr.end_sample);

	obs.duration_ms =
		samples_to_ms(attr.duration_samples);

	/*
	 * Space duration is deliberately measured rather than
	 * interpreted.
	 *
	 * The first observation after reset has no known preceding
	 * completed mark, so preceding_space_valid remains false.
	 */
	if (have_previous_mark &&
	    attr.start_sample >= previous_end_sample) {

		const unsigned long space_samples =
			attr.start_sample - previous_end_sample;

		obs.preceding_space_ms =
			samples_to_ms(space_samples);

		obs.preceding_space_valid = true;
	}

	obs.confidence = attr.confidence;

	obs.carrier_mean_hz =
		attr.carrier_mean_hz;

	obs.carrier_sigma_hz =
		attr.carrier_sigma_hz;

	obs.carrier_coherence =
		attr.carrier_coherence;

	obs.carrier_evidence =
		attr.carrier_evidence;

	obs.recent_mean_hz =
		attr.recent_mean_hz;

	obs.recent_sigma_hz =
		attr.recent_sigma_hz;

	obs.recent_median_hz =
		attr.recent_median_hz;

	obs.recent_mad_hz =
		attr.recent_mad_hz;

	obs.recent_good_count =
		attr.recent_good_count;

	obs.valid = true;

	/*
	 * Only after using the previous mark to calculate the space
	 * do we make this mark the previous mark for the next call.
	 */
	previous_end_sample = attr.end_sample;
	have_previous_mark = true;

	write_csv(obs);

	return true;
}
