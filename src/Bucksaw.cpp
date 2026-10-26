// Bucksaw: stereo super-oscillator for VCV Rack, 4 HP.
// Recreated from the published behaviour of the Acid Rain Technology Chainsaw (docs/manual.txt).
#include <rack.hpp>
#include "Saw.hpp"
#include "Controls4ms.hpp"

using namespace rack;

Plugin* pluginInstance;

/** Shows the root as a note name plus cents. */
struct RootQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		static const char* const NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		float cents = getValue();
		int semis = (int) std::round(cents / 100.f);
		int rest = (int) std::round(cents - semis * 100.f);
		int note = ((semis % 12) + 12) % 12;
		int octave = 1 + (int) std::floor(semis / 12.f);
		float hz = bucksaw::pitch(cents, 0.f);
		return string::f("%s%d %+d ct (%.2f Hz)", NAMES[note], octave, rest, hz);
	}
};

struct Bucksaw : Module {
	enum ParamId { ROOT_PARAM, DETUNE_PARAM, MORPH_PARAM, PARAMS_LEN };
	enum InputId { VO1_INPUT, VO2_INPUT, VO3_INPUT, DETUNE_INPUT, MORPH_INPUT, FM_INPUT, INPUTS_LEN };
	enum OutputId { LEFT_OUTPUT, RIGHT_OUTPUT, OUTPUTS_LEN };
	enum LightId { NOTE_LIGHT, ALT_LIGHT, LIGHTS_LEN };

	bucksaw::Voice voices[bucksaw::VOICES];
	bool fine = false;  // encoder in 1 cent steps ("alt") instead of semitones ("note")
	int theme = 0;

	Bucksaw() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// root in cents above C1 (32.7 Hz); the encoder sets it, long press returns to C1
		configParam<RootQuantity>(ROOT_PARAM, -2400.f, 9600.f, 0.f, "Root");
		configParam(DETUNE_PARAM, 0.f, 1.f, 0.f, "Detune", "%", 0.f, 100.f);
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "Morph (saw to square)", "%", 0.f, 100.f);
		configInput(VO1_INPUT, "Voice 1 V/oct (patch to activate)");
		configInput(VO2_INPUT, "Voice 2 V/oct (patch to activate)");
		configInput(VO3_INPUT, "Voice 3 V/oct (patch to activate)");
		configInput(DETUNE_INPUT, "Detune CV (-5..+5 V)");
		configInput(MORPH_INPUT, "Morph CV (-5..+5 V)");
		configInput(FM_INPUT, "Linear FM (-5..+5 V)");
		configOutput(LEFT_OUTPUT, "Left");
		configOutput(RIGHT_OUTPUT, "Right");
		seedPhases();
	}

	void seedPhases() {
		uint32_t seed = random::u32() | 1u;
		for (bucksaw::Voice& v : voices)
			v.randomize(seed);
	}

	void onReset() override {
		fine = false;
		seedPhases();
	}

	void process(const ProcessArgs& args) override {
		float root = params[ROOT_PARAM].getValue();
		// CV is added to the knob position: 10 Vpp sweeps the whole range
		float detune = bucksaw::detuneCurve(params[DETUNE_PARAM].getValue() + inputs[DETUNE_INPUT].getVoltage() / 10.f);
		float morph = clamp(params[MORPH_PARAM].getValue() + inputs[MORPH_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		float fm = inputs[FM_INPUT].getVoltage();
		float l = 0.f, r = 0.f;
		for (int i = 0; i < bucksaw::VOICES; i++) {
			if (!inputs[VO1_INPUT + i].isConnected())
				continue;  // a voice only sounds with a cable in its v/o input
			float f = bucksaw::linearFm(bucksaw::pitch(root, inputs[VO1_INPUT + i].getVoltage()), fm);
			voices[i].process(f, detune, morph, args.sampleTime, l, r);
		}
		outputs[LEFT_OUTPUT].setVoltage(clamp(l * bucksaw::OUT_GAIN, -12.f, 12.f));
		outputs[RIGHT_OUTPUT].setVoltage(clamp(r * bucksaw::OUT_GAIN, -12.f, 12.f));
		lights[NOTE_LIGHT].setBrightness(fine ? 0.f : 1.f);
		lights[ALT_LIGHT].setBrightness(fine ? 1.f : 0.f);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "fine", json_boolean(fine));
		json_object_set_new(root, "theme", json_integer(theme));
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* f = json_object_get(root, "fine"))
			fine = json_boolean_value(f);
		if (json_t* t = json_object_get(root, "theme"))
			theme = clamp((int) json_integer_value(t), 0, 1);
	}
};

// ---------------------------------------------------------------- themes
struct BTheme {
	NVGcolor panel, ink, grey, plate, plateInk, accent;
};
static const int NUM_THEMES = 2;
static const char* const THEME_NAMES[NUM_THEMES] = {"Bone", "Slate"};

static BTheme bTheme(int t) {
	BTheme k;
	if (t == 1) {
		k.panel = nvgRGB(0x17, 0x19, 0x1B);
		k.ink = nvgRGB(0xE6, 0xE8, 0xE8);
		k.grey = nvgRGB(0x8A, 0x8D, 0x93);
		k.plate = nvgRGB(0x0E, 0x11, 0x13);
		k.plateInk = nvgRGB(0xE6, 0xE8, 0xE8);
		k.accent = nvgRGB(0xC6, 0xF0, 0x3C);
	}
	else {
		k.panel = nvgRGB(0xE8, 0xE6, 0xE1);
		k.ink = nvgRGB(0x0F, 0x12, 0x16);
		k.grey = nvgRGB(0x6E, 0x71, 0x77);
		k.plate = nvgRGB(0x1B, 0x1E, 0x24);
		k.plateInk = nvgRGB(0xF6, 0xF4, 0xEF);
		k.accent = nvgRGB(0x7F, 0xB8, 0x00);
	}
	return k;
}

namespace fms {
const Colors& colors(const engine::Module* m) {
	static const Colors bone = {
		nvgRGB(0x11, 0x11, 0x11), nvgRGB(0x3A, 0x3A, 0x3A), nvgRGB(0xF6, 0xF4, 0xEF), nvgRGB(0x0F, 0x12, 0x16),
		nvgRGB(0xD3, 0xD3, 0xD0), nvgRGB(0xF7, 0xF7, 0xF5), nvgRGB(0x8E, 0x8E, 0x8A), nvgRGB(0x5C, 0x5C, 0x5A), nvgRGB(0x05, 0x05, 0x05),
		nvgRGB(0xF4, 0xF2, 0xEA), nvgRGB(0xFF, 0xFF, 0xFF), nvgRGB(0x9C, 0xE0, 0x00), nvgRGB(0xD8, 0xFF, 0x7A), nvgRGB(0x0F, 0x12, 0x16),
		nvgRGB(0xFB, 0xFB, 0xF9)};
	static const Colors slate = {
		nvgRGB(0x0B, 0x0B, 0x0D), nvgRGB(0x3A, 0x3B, 0x3F), nvgRGB(0xF6, 0xF4, 0xEF), nvgRGB(0xE6, 0xE8, 0xE8),
		nvgRGB(0xD3, 0xD3, 0xD0), nvgRGB(0xF7, 0xF7, 0xF5), nvgRGB(0x8E, 0x8E, 0x8A), nvgRGB(0x5C, 0x5C, 0x5A), nvgRGB(0x05, 0x05, 0x05),
		nvgRGB(0x2A, 0x2C, 0x30), nvgRGB(0x40, 0x42, 0x46), nvgRGB(0xC6, 0xF0, 0x3C), nvgRGB(0xE8, 0xFF, 0x9A), nvgRGB(0x6B, 0x6F, 0x75),
		nvgRGB(0xFB, 0xFB, 0xF9)};
	int t = m ? static_cast<const Bucksaw*>(m)->theme : 0;
	return t == 1 ? slate : bone;
}
}  // namespace fms

static std::shared_ptr<window::Font> pluginFont(const char* file) {
	std::shared_ptr<window::Font> f = APP->window->loadFont(asset::plugin(pluginInstance, std::string("res/fonts/") + file));
	if (!f)
		f = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
	return f;
}

// ---------------------------------------------------------------- layout (mm), after the manual's panel figure
static const float W = 20.32f;
static const float CX = W / 2.f;
static const float LX = 5.6f, RX = 14.72f;
static const Vec ENC_POS(CX, 17.0f);
static const Vec NOTE_LED(LX - 0.6f, 25.6f), ALT_LED(RX + 0.6f, 25.6f);
static const Vec DETUNE_POS(CX, 37.0f), MORPH_POS(CX, 52.5f);
static const float ROW[4] = {68.0f, 81.0f, 94.0f, 107.0f};
static const float ENC_R = 4.6f, KNOB_R = 4.4f;

// ---------------------------------------------------------------- widgets
/** Endless encoder with push button: turn = semitone (note) or cent (alt) steps, click toggles the
step size, a 2 s hold returns the root to C1. Scrolling the wheel also turns it. */
struct Encoder : app::ParamWidget {
	float acc = 0.f;
	double pressTime = -1.0;
	bool dragged = false, longDone = false;
	float angle = 0.f;

	Encoder() {
		box.size = mm2px(Vec(2 * ENC_R + 2.4f, 2 * ENC_R + 2.4f));
	}

	Bucksaw* bmodule() {
		return static_cast<Bucksaw*>(module);
	}

	void turn(int steps) {
		engine::ParamQuantity* pq = getParamQuantity();
		Bucksaw* m = bmodule();
		if (!pq || !m || steps == 0)
			return;
		float v = pq->getValue();
		if (m->fine)
			v += steps;
		else
			v = std::round(v / 100.f) * 100.f + 100.f * steps;  // semitone steps land on the grid
		pq->setValue(clamp(v, pq->getMinValue(), pq->getMaxValue()));
		angle += steps * (float) M_PI / 12.f;
	}

	void onButton(const ButtonEvent& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			pressTime = system::getTime();
			dragged = longDone = false;
			acc = 0.f;
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		acc += (e.mouseDelta.x - e.mouseDelta.y) / getAbsoluteZoom();
		const float STEP = 7.f;
		int steps = (int) (acc / STEP);
		if (steps != 0) {
			dragged = true;
			acc -= steps * STEP;
			turn(steps);
		}
	}

	void onDragEnd(const DragEndEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		if (!dragged && !longDone && bmodule())
			bmodule()->fine = !bmodule()->fine;
		pressTime = -1.0;
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		turn(e.scrollDelta.y > 0 ? 1 : (e.scrollDelta.y < 0 ? -1 : 0));
		e.consume(this);
	}

	void step() override {
		if (pressTime >= 0.0 && !dragged && !longDone && system::getTime() - pressTime > 2.0) {
			if (getParamQuantity())
				getParamQuantity()->setValue(0.f);  // C1
			longDone = true;
		}
		ParamWidget::step();
	}

	void draw(const DrawArgs& args) override {
		const fms::Colors& c = fms::colors(module);
		NVGcontext* vg = args.vg;
		float cx = box.size.x / 2, cy = box.size.y / 2, r = mm2px(ENC_R);
		fms::softShadow(vg, cx, cy, r, r * 0.1f, 0.3f);
		// knurled edge: many short ridges
		fms::circle(vg, cx, cy, r, c.knobEdge, c.knob, 0.f);
		for (int i = 0; i < 36; i++) {
			float a = angle + i * 2.f * (float) M_PI / 36.f;
			nvgBeginPath(vg);
			nvgMoveTo(vg, cx + std::sin(a) * r * 0.86f, cy - std::cos(a) * r * 0.86f);
			nvgLineTo(vg, cx + std::sin(a) * r, cy - std::cos(a) * r);
			nvgStrokeColor(vg, c.knob);
			nvgStrokeWidth(vg, std::max(1.f, r * 0.07f));
			nvgStroke(vg);
		}
		fms::circle(vg, cx, cy, r * 0.8f, c.knob, c.knob, 0.f);
		// dimple
		float dx = std::sin(angle) * r * 0.5f, dy = -std::cos(angle) * r * 0.5f;
		fms::circle(vg, cx + dx, cy + dy, r * 0.11f, c.pointer, c.pointer, 0.f);
	}
};

/** Small round LED in the accent colour. */
struct Led : TransparentWidget {
	Bucksaw* module = nullptr;
	int lightId = 0;

	Led() {
		box.size = mm2px(Vec(2.4f, 2.4f));
	}

	float level() {
		return module ? module->lights[lightId].getBrightness() : (lightId == Bucksaw::NOTE_LIGHT ? 1.f : 0.f);
	}

	void draw(const DrawArgs& args) override {
		const fms::Colors& c = fms::colors(module);
		float r = box.size.x / 2;
		fms::circle(args.vg, r, r, r * 0.9f, fms::mix(c.jackDark, c.btnOn, level()), c.outline, 0.8f);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1 || level() < 0.02f)
			return;
		const fms::Colors& c = fms::colors(module);
		NVGcontext* vg = args.vg;
		float r = box.size.x / 2;
		fms::circle(vg, r, r, r * 0.8f, c.btnOn, c.btnOn, 0.f);
		nvgBeginPath(vg);
		nvgCircle(vg, r, r, r * 2.6f);
		nvgFillPaint(vg, nvgRadialGradient(vg, r, r, r * 0.6f, r * 2.6f, nvgRGBAf(c.btnOn.r, c.btnOn.g, c.btnOn.b, 0.35f),
		                                   nvgRGBAf(c.btnOn.r, c.btnOn.g, c.btnOn.b, 0.f)));
		nvgFill(vg);
	}
};

/** Panel background and every label, drawn in code so the theme can change. */
struct PanelArt : Widget {
	Bucksaw* module = nullptr;

	void text(NVGcontext* vg, Vec mm, const char* s) {
		nvgText(vg, mm2px(mm.x), mm2px(mm.y), s, NULL);
	}

	void draw(const DrawArgs& args) override {
		BTheme k = bTheme(module ? module->theme : 0);
		NVGcontext* vg = args.vg;
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, box.size.x, box.size.y);
		nvgFillColor(vg, k.panel);
		nvgFill(vg);

		// the two outputs on a dark plate in the right column
		nvgBeginPath(vg);
		nvgRoundedRect(vg, mm2px(RX - 4.5f), mm2px(ROW[2] - 4.8f), mm2px(9.0f), mm2px(ROW[3] - ROW[2] + 11.4f), mm2px(2.2f));
		nvgFillColor(vg, k.plate);
		nvgFill(vg);

		// a saw tooth line under the name
		nvgBeginPath(vg);
		for (int i = 0; i <= 4; i++) {
			float x0 = 3.0f + i * (W - 6.0f) / 4.f;
			if (i == 0)
				nvgMoveTo(vg, mm2px(x0), mm2px(10.2f));
			else {
				nvgLineTo(vg, mm2px(x0), mm2px(8.4f));
				nvgLineTo(vg, mm2px(x0), mm2px(10.2f));
			}
		}
		nvgStrokeColor(vg, k.accent);
		nvgStrokeWidth(vg, mm2px(0.4f));
		nvgLineJoin(vg, NVG_MITER);
		nvgStroke(vg);

		std::shared_ptr<window::Font> logo = pluginFont("Syne-Bold.ttf");
		std::shared_ptr<window::Font> reg = pluginFont("SpaceMono-Regular.ttf");
		if (!logo || !reg)
			return;
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontFaceId(vg, logo->handle);
		nvgFontSize(vg, mm2px(4.0f));
		nvgTextLetterSpacing(vg, -mm2px(0.1f));
		nvgFillColor(vg, k.ink);
		text(vg, Vec(CX, 5.2f), "bucksaw");
		nvgTextLetterSpacing(vg, 0.f);

		nvgFontFaceId(vg, reg->handle);
		nvgFontSize(vg, mm2px(2.3f));
		text(vg, Vec(NOTE_LED.x, NOTE_LED.y + 2.9f), "note");
		text(vg, Vec(ALT_LED.x, ALT_LED.y + 2.9f), "alt");
		nvgFontSize(vg, mm2px(2.6f));
		text(vg, Vec(CX, DETUNE_POS.y + KNOB_R + 3.0f), "detune");
		text(vg, Vec(CX, MORPH_POS.y + KNOB_R + 3.0f), "morph");
		const char* left[4] = {"detune", "v/o 1", "v/o 2", "v/o 3"};
		const char* right[4] = {"morph", "FM", "out L", "out R"};
		nvgFontSize(vg, mm2px(2.2f));
		for (int i = 0; i < 4; i++) {
			nvgFillColor(vg, k.ink);
			text(vg, Vec(LX, ROW[i] + 5.2f), left[i]);
			nvgFillColor(vg, i >= 2 ? k.plateInk : k.ink);
			text(vg, Vec(RX, ROW[i] + 5.2f), right[i]);
		}
	}
};

struct BucksawWidget : ModuleWidget {
	BucksawWidget(Bucksaw* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Bucksaw.svg")));

		PanelArt* art = new PanelArt;
		art->module = module;
		art->box.size = box.size;
		addChild(art);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		Encoder* enc = createParamCentered<Encoder>(mm2px(ENC_POS), module, Bucksaw::ROOT_PARAM);
		addParam(enc);
		for (int i = 0; i < 2; i++) {
			Led* led = new Led;
			led->module = module;
			led->lightId = i == 0 ? Bucksaw::NOTE_LIGHT : Bucksaw::ALT_LIGHT;
			led->box.pos = mm2px(i == 0 ? NOTE_LED : ALT_LED).minus(led->box.size.div(2));
			addChild(led);
		}
		addParam(fms::knob(mm2px(DETUNE_POS), module, Bucksaw::DETUNE_PARAM, KNOB_R));
		addParam(fms::knob(mm2px(MORPH_POS), module, Bucksaw::MORPH_PARAM, KNOB_R));

		addInput(createInputCentered<fms::Jack>(mm2px(Vec(LX, ROW[0])), module, Bucksaw::DETUNE_INPUT));
		addInput(createInputCentered<fms::Jack>(mm2px(Vec(RX, ROW[0])), module, Bucksaw::MORPH_INPUT));
		addInput(createInputCentered<fms::Jack>(mm2px(Vec(LX, ROW[1])), module, Bucksaw::VO1_INPUT));
		addInput(createInputCentered<fms::Jack>(mm2px(Vec(RX, ROW[1])), module, Bucksaw::FM_INPUT));
		addInput(createInputCentered<fms::Jack>(mm2px(Vec(LX, ROW[2])), module, Bucksaw::VO2_INPUT));
		addOutput(createOutputCentered<fms::Jack>(mm2px(Vec(RX, ROW[2])), module, Bucksaw::LEFT_OUTPUT));
		addInput(createInputCentered<fms::Jack>(mm2px(Vec(LX, ROW[3])), module, Bucksaw::VO3_INPUT));
		addOutput(createOutputCentered<fms::Jack>(mm2px(Vec(RX, ROW[3])), module, Bucksaw::RIGHT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Bucksaw* m = getModule<Bucksaw>();
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Fine tune (alt)", "", &m->fine));
		menu->addChild(createMenuItem("Root to C1", "", [=]() { m->params[Bucksaw::ROOT_PARAM].setValue(0.f); }));
		menu->addChild(createIndexPtrSubmenuItem("Theme", std::vector<std::string>(THEME_NAMES, THEME_NAMES + NUM_THEMES), &m->theme));
	}
};

Model* modelBucksaw = createModel<Bucksaw, BucksawWidget>("Bucksaw");

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelBucksaw);
}
