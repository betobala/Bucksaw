#pragma once
// Stereo super-oscillator: 3 voices of 7 waves that morph from super saw to super square.
// Written from the published behaviour of the Acid Rain Technology Chainsaw (manual fw 1.1, docs/);
// the firmware is not public, so the detune curve follows the measured Roland JP-8000 super saw
// that the Chainsaw is modelled on. Independent of Rack so it can be unit tested.
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace bucksaw {

static const int WAVES = 7;
static const int VOICES = 3;

/** Relative frequency offset of each wave at full detune (3 down, root, 3 up), JP-8000 measurements. */
static const float OFFSETS[WAVES] = {-0.11002313f, -0.06288439f, -0.01952356f, 0.f, 0.01991221f, 0.06216538f, 0.10745242f};

/** Pan of each wave, 0 = left, 1 = right: the waves alternate sides so each output gets a spread. */
static const float PAN[WAVES] = {0.f, 1.f, 1.f / 6.f, 0.5f, 5.f / 6.f, 1.f / 3.f, 2.f / 3.f};

/** detune 0..1 -> spread amount 0..1 along the non linear JP-8000 curve (slow start, steep end) */
inline float detuneCurve(float d) {
	if (d <= 0.f)
		return 0.f;  // perfect unison
	d = std::min(1.f, d);
	float y = 10028.7312891634f * std::pow(d, 11) - 50818.8652045924f * std::pow(d, 10) + 111363.4808729368f * std::pow(d, 9)
	          - 138150.6761080548f * std::pow(d, 8) + 106649.6679158292f * std::pow(d, 7) - 53046.9642751875f * std::pow(d, 6)
	          + 17019.9518580080f * std::pow(d, 5) - 3425.0836591318f * std::pow(d, 4) + 404.2703938388f * std::pow(d, 3)
	          - 24.1878824391f * d * d + 0.6717417634f * d + 0.0030115596f;
	return std::max(0.f, std::min(1.f, y));
}

/** PolyBLEP residual for a discontinuity at phase 0, t = phase, dt = phase increment. */
inline float blep(float t, float dt) {
	if (t < dt) {
		t /= dt;
		return t + t - t * t - 1.f;
	}
	if (t > 1.f - dt) {
		t = (t - 1.f) / dt;
		return t * t + t + t + 1.f;
	}
	return 0.f;
}

/** Band limited saw (-1..1, falling at the wrap) at phase p. */
inline float saw(float p, float dt) {
	return 2.f * p - 1.f - blep(p, dt);
}

/** Morph: a saw plus an inverted saw half a period later. At m = 1 the sum is a square. */
inline float morphWave(float p, float dt, float m) {
	float q = p + 0.5f;
	if (q >= 1.f)
		q -= 1.f;
	return (saw(p, dt) - m * saw(q, dt)) / (1.f + m) * (1.f + 0.6f * m);
}

struct Voice {
	float phase[WAVES];

	void randomize(uint32_t& seed) {
		for (int i = 0; i < WAVES; i++) {
			seed = seed * 1664525u + 1013904223u;
			phase[i] = (seed >> 8) / 16777216.f;
		}
	}

	/** Adds this voice into l/r. f: root frequency (Hz), may be negative with deep linear FM. */
	void process(float f, float spread, float morph, float sampleTime, float& l, float& r) {
		for (int i = 0; i < WAVES; i++) {
			float fi = f * (1.f + spread * OFFSETS[i]);
			float dt = std::min(std::fabs(fi) * sampleTime, 0.45f);
			float y = morphWave(phase[i], std::max(dt, 1e-6f), morph);
			l += y * (1.f - PAN[i]);
			r += y * PAN[i];
			phase[i] += fi >= 0.f ? dt : -dt;
			phase[i] -= std::floor(phase[i]);
		}
	}
};

static const float C1 = 32.7032f;

/** Root in cents from C1 + v/oct -> Hz */
inline float pitch(float rootCents, float voct) {
	return C1 * std::exp2(rootCents / 1200.f + voct);
}

/** Linear FM: +/-5 V swings the frequency by +/-100 % (through zero at the extremes). */
inline float linearFm(float f, float fmVolts) {
	return f * (1.f + std::max(-5.f, std::min(5.f, fmVolts)) / 5.f);
}

/** Output gain: one voice of 7 waves gives about 10 Vpp on each side. */
static const float OUT_GAIN = 5.f / 2.2f;

}  // namespace bucksaw
