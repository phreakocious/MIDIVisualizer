#ifndef TakeClock_h
#define TakeClock_h

#include <cstddef>

/// Least-squares line of audio sample index against host time. The device's sample clock
/// drifts against the host clock, so the MIDI side maps host times through this fit.
class TakeClock {
public:

	explicit TakeClock(double nominalRate) : _nominal(nominalRate) {}

	void add(double hostSeconds, double sampleIndex) {
		if(_n == 0) {
			_x0 = hostSeconds;
			_y0 = sampleIndex;
		}
		// Relative to the first point: host times are ~1e3 s and indices ~1e8, so raw squares would lose bits.
		const long double dx = (long double)hostSeconds - _x0;
		const long double dy = (long double)sampleIndex - _y0;
		_sx += dx;
		_sy += dy;
		_sxx += dx * dx;
		_sxy += dx * dy;
		++_n;
	}

	double sampleAt(double hostSeconds) const {
		if(_n == 0) {
			return _nominal * hostSeconds;
		}
		const long double dx = (long double)hostSeconds - _x0;
		if(_n < 2) {
			return (double)(_y0 + _nominal * dx);
		}
		const long double n = (long double)_n;
		const long double denom = n * _sxx - _sx * _sx;
		if(denom <= 0) {
			return (double)(_y0 + _nominal * dx);
		}
		const long double slope = (n * _sxy - _sx * _sy) / denom;
		const long double intercept = (_sy - slope * _sx) / n;
		return (double)(_y0 + intercept + slope * dx);
	}

	size_t count() const { return _n; }

private:

	double _nominal;
	size_t _n = 0;
	long double _x0 = 0, _y0 = 0;
	long double _sx = 0, _sy = 0, _sxx = 0, _sxy = 0;
};

#endif
