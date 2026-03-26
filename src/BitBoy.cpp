#include "plugin.hpp"

struct BitBoy : Module {
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

	BitBoy() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(PITCH_PARAM, -4.f, 4.f, 0.f, "Pitch", " octaves");
		configParam(FINE_PARAM, -0.5f, 0.5f, 0.f, "Fine tune", " semitones", 0.f, 12.f);
		configParam(PW_PARAM, 0.01f, 0.99f, 0.5f, "Pulse width", "%", 0.f, 100.f);
		configParam(PW_CV_PARAM, -1.f, 1.f, 1.f, "PW CV amount");
		configParam(FM_PARAM, -1.f, 1.f, 1.f, "FM amount");
		configSwitch(WAVEFORM_PARAM, 0.f, 3.f, 0.f, "Waveform", {"Square", "Triangle", "Sawtooth", "Noise"});

		configInput(VOCT_INPUT, "V/Oct pitch");
		configInput(FM_INPUT, "FM");
		configInput(PW_INPUT, "Pulse width CV");
		configInput(SYNC_INPUT, "Sync");

		configOutput(MAIN_OUTPUT, "Audio");
	}

	// Quantize to 8-bit levels (-1 to 1 range -> 256 levels)
	float quantize8bit(float value) {
		float scaled = (value + 1.f) * 127.5f;
		float quantized = std::round(scaled);
		quantized = clamp(quantized, 0.f, 255.f);
		return (quantized / 127.5f) - 1.f;
	}

	// NES-style LFSR noise
	void clockLFSR() {
		uint16_t bit = ((lfsr >> 0) ^ (lfsr >> 1)) & 1;
		lfsr = (lfsr >> 1) | (bit << 14);
		noiseValue = (lfsr & 1) ? 1.f : -1.f;
	}

	void process(const ProcessArgs& args) override {
		float pitch = params[PITCH_PARAM].getValue();
		pitch += params[FINE_PARAM].getValue() / 12.f;
		pitch += inputs[VOCT_INPUT].getVoltage();

		if (inputs[FM_INPUT].isConnected()) {
			pitch += inputs[FM_INPUT].getVoltage() * params[FM_PARAM].getValue();
		}

		float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);
		freq = clamp(freq, 0.f, args.sampleRate / 2.f);

		float pw = params[PW_PARAM].getValue();
		if (inputs[PW_INPUT].isConnected()) {
			pw += inputs[PW_INPUT].getVoltage() / 10.f * params[PW_CV_PARAM].getValue();
		}
		pw = clamp(pw, 0.01f, 0.99f);

		if (inputs[SYNC_INPUT].isConnected()) {
			float sync = inputs[SYNC_INPUT].getVoltage();
			static float lastSync = 0.f;
			if (sync > 0.f && lastSync <= 0.f) {
				phase = 0.f;
			}
			lastSync = sync;
		}

		float deltaPhase = freq * args.sampleTime;
		phase += deltaPhase;
		if (phase >= 1.f) {
			phase -= 1.f;
		}

		int waveform = (int)params[WAVEFORM_PARAM].getValue();
		float out = 0.f;

		switch (waveform) {
			case 0: // Square/Pulse
				out = (phase < pw) ? 1.f : -1.f;
				break;
			case 1: // Triangle (4-bit quantized like NES), RMS-normalised to match square
				if (phase < 0.5f) {
					out = phase * 4.f - 1.f;
				} else {
					out = 3.f - phase * 4.f;
				}
				out = std::round(out * 7.5f) / 7.5f;
				out = clamp(out * 1.7321f, -1.f, 1.f); // ×√3 normalises RMS to 1.0
				break;
			case 2: // Sawtooth
				out = 2.f * phase - 1.f;
				break;
			case 3: // Noise
				if (phase < lastNoisePhase) {
					clockLFSR();
				}
				lastNoisePhase = phase;
				out = noiseValue;
				break;
		}

		out = quantize8bit(out);
		outputs[MAIN_OUTPUT].setVoltage(out * 5.f);
	}
};

// ---------------------------------------------------------------------------
// Programmatic panel background — matches GranularDelay colour palette
// ---------------------------------------------------------------------------
struct BitBoyPanel : Widget {
	BitBoyPanel(Vec size) {
		box.size = size;
	}

	void draw(const DrawArgs& args) override {
		const float inset = 4.f;

		// --- Background ---
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFillColor(args.vg, nvgRGB(0x2a, 0x2a, 0x2a));
		nvgFill(args.vg);

		// --- Inner panel face ---
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, inset, inset,
		               box.size.x - 2.f * inset, box.size.y - 2.f * inset, 3.f);
		nvgFillColor(args.vg, nvgRGB(0x22, 0x22, 0x22));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		// --- Corner pixel decorations ---
		const float sqSz = 4.5f;
		nvgFillColor(args.vg, nvgRGBA(0x55, 0x55, 0x55, 0xa0));
		nvgBeginPath(args.vg); nvgRect(args.vg, 12.f,                     30.f, sqSz, sqSz); nvgFill(args.vg);
		nvgBeginPath(args.vg); nvgRect(args.vg, 18.f,                     30.f, sqSz, sqSz); nvgFill(args.vg);
		nvgBeginPath(args.vg); nvgRect(args.vg, box.size.x - 18.f - sqSz, 30.f, sqSz, sqSz); nvgFill(args.vg);
		nvgBeginPath(args.vg); nvgRect(args.vg, box.size.x - 12.f - sqSz, 30.f, sqSz, sqSz); nvgFill(args.vg);

		// --- Separator lines ---
		auto sep = [&](float y) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, inset + 3.f, y);
			nvgLineTo(args.vg, box.size.x - inset - 3.f, y);
			nvgStrokeColor(args.vg, nvgRGB(0x44, 0x44, 0x44));
			nvgStrokeWidth(args.vg, 0.8f);
			nvgStroke(args.vg);
		};
		sep(215.f); // above input section  (labels at y=232)
		sep(310.f); // above output section (label  at y=327)
	}
};

// Label overlay — added after controls so text renders on top of knobs/jacks
struct BitBoyLabels : Widget {
	BitBoyLabels(Vec size) {
		box.size = size;
	}

	void draw(const DrawArgs& args) override {
		const float cx   = box.size.x / 2.f;
		const float col1 = box.size.x * 0.25f;
		const float col2 = box.size.x * 0.75f;

		std::shared_ptr<Font> font = APP->window->loadFont(
			asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		// --- Title ---
		nvgFontSize(args.vg, 12.f);
		nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0xff));
		nvgText(args.vg, cx, 8.65f, "BitBoy", NULL);

		// --- Parameter labels (24px above each knob center, matching GranularDelay) ---
		nvgFontSize(args.vg, 8.5f);
		nvgFillColor(args.vg, nvgRGB(0x88, 0x88, 0x88));
		nvgText(args.vg, cx,    22.f, "WAVE",   NULL); // WAVEFORM at y=40  (40-18)
		nvgText(args.vg, cx,    58.f, "PITCH",  NULL); // PITCH     at y=82  (82-24)
		nvgText(args.vg, cx,   110.f, "FINE",   NULL); // FINE      at y=128 (128-18)
		nvgText(args.vg, col1, 145.f, "PW",     NULL); // PW        at y=163 (163-18)
		nvgText(args.vg, col2, 145.f, "CV",     NULL); // PW_CV     at y=163 (163-18)
		nvgText(args.vg, cx,   180.f, "FM AMT", NULL); // FM        at y=198 (198-18)

		// --- Input labels (18px above each jack center) ---
		nvgFontSize(args.vg, 7.5f);
		nvgFillColor(args.vg, nvgRGB(0xcc, 0xcc, 0xcc));
		nvgText(args.vg, col1, 232.f, "V/OCT", NULL); // VOCT  at y=250 (250-18)
		nvgText(args.vg, col2, 232.f, "FM",    NULL); // FM_IN at y=250 (250-18)
		nvgText(args.vg, col1, 267.f, "PW",    NULL); // PW_IN at y=285 (285-18)
		nvgText(args.vg, col2, 267.f, "SYNC",  NULL); // SYNC  at y=285 (285-18)

		// --- Output label (18px above jack center) ---
		nvgText(args.vg, cx, 327.f, "OUT", NULL);     // MAIN_OUTPUT at y=345 (345-18)

		// --- Brand: "VON" + "K" centred as one word ---
		nvgFontSize(args.vg, 7.f);
		float bounds[4];
		nvgTextBounds(args.vg, 0, 0, "VON", NULL, bounds);
		float vonW   = bounds[2] - bounds[0];
		nvgTextBounds(args.vg, 0, 0, "K",   NULL, bounds);
		float kW     = bounds[2] - bounds[0];
		float startX = cx - (vonW + kW) / 2.f;

		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, nvgRGB(0xf4, 0xf5, 0xf7));
		nvgText(args.vg, startX,         372.f, "VON", NULL);
		nvgFillColor(args.vg, nvgRGB(0xc0, 0x84, 0xfc));
		nvgText(args.vg, startX + vonW,  372.f, "K",   NULL);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	}
};

// ---------------------------------------------------------------------------
// Widget
// ---------------------------------------------------------------------------
struct BitBoyWidget : ModuleWidget {
	BitBoyWidget(BitBoy* module) {
		setModule(module);
		const float panelH = RACK_GRID_HEIGHT;
		box.size = Vec(RACK_GRID_WIDTH * 10, panelH);
		addChild(new BitBoyPanel(box.size));

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, panelH - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, panelH - RACK_GRID_WIDTH)));

		const float colCenter = box.size.x / 2.f;
		const float col1      = box.size.x * 0.25f;
		const float col2      = box.size.x * 0.75f;

		// Waveform selector
		addParam(createParamCentered<RoundBlackSnapKnob>(Vec(colCenter, 40),  module, BitBoy::WAVEFORM_PARAM));

		// Pitch controls
		addParam(createParamCentered<RoundLargeBlackKnob>(Vec(colCenter, 82),  module, BitBoy::PITCH_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(colCenter, 128), module, BitBoy::FINE_PARAM));

		// Pulse width
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(col1, 163), module, BitBoy::PW_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(col2, 163), module, BitBoy::PW_CV_PARAM));

		// FM amount
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(colCenter, 198), module, BitBoy::FM_PARAM));

		// Inputs
		addInput(createInputCentered<PJ301MPort>(Vec(col1, 250), module, BitBoy::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(col2, 250), module, BitBoy::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(col1, 285), module, BitBoy::PW_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(col2, 285), module, BitBoy::SYNC_INPUT));

		// Output
		addOutput(createOutputCentered<PJ301MPort>(Vec(colCenter, 345), module, BitBoy::MAIN_OUTPUT));

		// Label overlay — must be last so text renders above all controls
		addChild(new BitBoyLabels(box.size));
	}
};

Model* modelBitBoy = createModel<BitBoy, BitBoyWidget>("BitBoy");
