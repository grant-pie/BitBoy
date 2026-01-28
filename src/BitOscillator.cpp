#include "plugin.hpp"

struct BitOscillator : Module {
	enum ParamId {
		PITCH_PARAM,
		FINE_PARAM,
		PW_PARAM,
		PW_CV_PARAM,
		FM_PARAM,
		WAVEFORM_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT,
		FM_INPUT,
		PW_INPUT,
		SYNC_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		MAIN_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	// Oscillator state
	float phase = 0.f;

	// LFSR for noise generation (15-bit like NES)
	uint16_t lfsr = 0x7FFF;
	float noiseValue = 0.f;
	float lastNoisePhase = 0.f;

	BitOscillator() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(PITCH_PARAM, -4.f, 4.f, 0.f, "Pitch", " octaves");
		configParam(FINE_PARAM, -0.5f, 0.5f, 0.f, "Fine tune", " semitones", 0.f, 12.f);
		configParam(PW_PARAM, 0.01f, 0.99f, 0.5f, "Pulse width", "%", 0.f, 100.f);
		configParam(PW_CV_PARAM, -1.f, 1.f, 0.f, "PW CV amount");
		configParam(FM_PARAM, -1.f, 1.f, 0.f, "FM amount");
		configSwitch(WAVEFORM_PARAM, 0.f, 3.f, 0.f, "Waveform", {"Square", "Triangle", "Sawtooth", "Noise"});

		configInput(VOCT_INPUT, "V/Oct pitch");
		configInput(FM_INPUT, "FM");
		configInput(PW_INPUT, "Pulse width CV");
		configInput(SYNC_INPUT, "Sync");

		configOutput(MAIN_OUTPUT, "Audio");
	}

	// Quantize to 8-bit levels (-1 to 1 range -> 256 levels)
	float quantize8bit(float value) {
		// Map -1..1 to 0..255, quantize, then map back
		float scaled = (value + 1.f) * 127.5f;
		float quantized = std::round(scaled);
		quantized = clamp(quantized, 0.f, 255.f);
		return (quantized / 127.5f) - 1.f;
	}

	// NES-style LFSR noise
	void clockLFSR() {
		// 15-bit LFSR with taps at bits 0 and 1 (NES style)
		uint16_t bit = ((lfsr >> 0) ^ (lfsr >> 1)) & 1;
		lfsr = (lfsr >> 1) | (bit << 14);
		noiseValue = (lfsr & 1) ? 1.f : -1.f;
	}

	void process(const ProcessArgs& args) override {
		// Calculate frequency
		float pitch = params[PITCH_PARAM].getValue();
		pitch += params[FINE_PARAM].getValue() / 12.f;
		pitch += inputs[VOCT_INPUT].getVoltage();

		// FM modulation
		if (inputs[FM_INPUT].isConnected()) {
			pitch += inputs[FM_INPUT].getVoltage() * params[FM_PARAM].getValue();
		}

		// Convert pitch to frequency (C4 = 261.63 Hz at 0V)
		float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);
		freq = clamp(freq, 0.f, args.sampleRate / 2.f);

		// Calculate pulse width
		float pw = params[PW_PARAM].getValue();
		if (inputs[PW_INPUT].isConnected()) {
			pw += inputs[PW_INPUT].getVoltage() / 10.f * params[PW_CV_PARAM].getValue();
		}
		pw = clamp(pw, 0.01f, 0.99f);

		// Hard sync
		if (inputs[SYNC_INPUT].isConnected()) {
			float sync = inputs[SYNC_INPUT].getVoltage();
			// Simple rising edge detection for sync
			static float lastSync = 0.f;
			if (sync > 0.f && lastSync <= 0.f) {
				phase = 0.f;
			}
			lastSync = sync;
		}

		// Phase increment
		float deltaPhase = freq * args.sampleTime;
		phase += deltaPhase;
		if (phase >= 1.f) {
			phase -= 1.f;
		}

		// Generate waveform based on selection
		int waveform = (int)params[WAVEFORM_PARAM].getValue();
		float out = 0.f;

		switch (waveform) {
			case 0: // Square/Pulse
				out = (phase < pw) ? 1.f : -1.f;
				break;

			case 1: // Triangle (4-bit quantized like NES for extra crunch)
				if (phase < 0.5f) {
					out = phase * 4.f - 1.f;
				} else {
					out = 3.f - phase * 4.f;
				}
				// Extra 4-bit quantization for triangle (NES style)
				out = std::round(out * 7.5f) / 7.5f;
				break;

			case 2: // Sawtooth
				out = 2.f * phase - 1.f;
				break;

			case 3: // Noise
				// Clock the LFSR at the oscillator frequency
				if (phase < lastNoisePhase) {
					clockLFSR();
				}
				lastNoisePhase = phase;
				out = noiseValue;
				break;
		}

		// Apply 8-bit quantization
		out = quantize8bit(out);

		// Output at 5V peak
		outputs[MAIN_OUTPUT].setVoltage(out * 5.f);
	}
};

struct BitOscillatorWidget : ModuleWidget {
	BitOscillatorWidget(BitOscillator* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/BitOscillator.svg")));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Module is 8HP wide
		float colCenter = box.size.x / 2.f;
		float col1 = box.size.x * 0.25f;
		float col2 = box.size.x * 0.75f;

		// Waveform selector at top
		addParam(createParamCentered<RoundBlackSnapKnob>(Vec(colCenter, 45), module, BitOscillator::WAVEFORM_PARAM));

		// Pitch controls
		addParam(createParamCentered<RoundLargeBlackKnob>(Vec(colCenter, 95), module, BitOscillator::PITCH_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(colCenter, 140), module, BitOscillator::FINE_PARAM));

		// Pulse width
		addParam(createParamCentered<RoundBlackKnob>(Vec(col1, 185), module, BitOscillator::PW_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(col2, 185), module, BitOscillator::PW_CV_PARAM));

		// FM amount
		addParam(createParamCentered<RoundBlackKnob>(Vec(colCenter, 230), module, BitOscillator::FM_PARAM));

		// Inputs
		addInput(createInputCentered<PJ301MPort>(Vec(col1, 275), module, BitOscillator::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(col2, 275), module, BitOscillator::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(col1, 315), module, BitOscillator::PW_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(col2, 315), module, BitOscillator::SYNC_INPUT));

		// Output
		addOutput(createOutputCentered<PJ301MPort>(Vec(colCenter, 355), module, BitOscillator::MAIN_OUTPUT));
	}
};

Model* modelBitOscillator = createModel<BitOscillator, BitOscillatorWidget>("BitOscillator");
