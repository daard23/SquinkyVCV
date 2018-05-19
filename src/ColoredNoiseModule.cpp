
#include "Squinky.hpp"

#include "WidgetComposite.h"
#include "ColoredNoise.h"

/**
 * Implementation class for VocalWidget
 */
struct ColoredNoiseModule : Module
{
    ColoredNoiseModule();

    /**
     * Overrides of Module functions
     */
    void step() override;
    void onSampleRateChange() override;

    ColoredNoise<WidgetComposite> noiseSource;
private:
    typedef float T;
};

ColoredNoiseModule::ColoredNoiseModule() 
    : Module(noiseSource.NUM_PARAMS,
        noiseSource.NUM_INPUTS, 
        noiseSource.NUM_OUTPUTS, 
        noiseSource.NUM_LIGHTS),
        noiseSource(this)
{
    onSampleRateChange();
    noiseSource.init();
}

void ColoredNoiseModule::onSampleRateChange()
{
    T rate = engineGetSampleRate();
    noiseSource.setSampleRate(rate);
}

void ColoredNoiseModule::step()
{
    noiseSource.step();
}

////////////////////
// module widget
////////////////////

struct ColoredNoiseWidget : ModuleWidget
{
    ColoredNoiseWidget(ColoredNoiseModule *);
    Label * slopeLabel;
    Label * signLabel;
};

static const unsigned char red[3] = {0xff, 0x04, 0x14 };
static const unsigned char pink[3] = {0xff, 0x3a, 0x6d };
static const unsigned char white[3] = {0xff, 0xff, 0xff };
static const unsigned char blue[3] = {0x54, 0x43, 0xc1 };
static const unsigned char violet[3] = {0x9d, 0x3c, 0xe6 };

// 0 <= x <= 1
static float interp(float x, int x0, int x1) 
{
    return x1 * x + x0 * (1 - x);
}

// 0 <= x <= 3
static void interp(unsigned char * out, float x, const unsigned char* y0, const unsigned char* y1)
{
    x = x * 1.0/3.0;    // 0..1
    out[0] = interp (x, y0[0], y1[0]);
    out[1] = interp (x, y0[1], y1[1]);
    out[2] = interp (x, y0[2], y1[2]);
}

static void copyColor(unsigned char * out, const unsigned char* in)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
}

static void getColor(unsigned char * out,  float x)
{
    if (x < -6) {
       copyColor(out, red);
    } else if (x >= 6) {
        copyColor(out, violet);
    } else {
        if (x < -3) {
            interp(out, x + 6, red, pink);
        } else if ( x < 0) {
            interp(out, x + 3, pink, white);
        } else if ( x < 3) {
            interp(out, x + 0, white, blue);
        } else if ( x < 6) {
            interp(out, x - 3, blue, violet);
        } else {
            copyColor(out, white);
        }
    }
}

struct ColorDisplay : OpaqueWidget {
    ColoredNoiseModule *module;
    ColorDisplay(Label *slopeLabel, Label *signLabel) 
        : _slopeLabel(slopeLabel), _signLabel(signLabel) {}

    Label* _slopeLabel;
    Label* _signLabel;
    void draw(NVGcontext *vg) override 
    {
        const float slope = module->noiseSource.getSlope();
        unsigned char color[3];
        getColor(color, slope);
        nvgFillColor(vg, nvgRGBA(color[0], color[1], color[2], 0xff));

        nvgBeginPath(vg);
        // todo: pass in ctor
        nvgRect(vg, 0, 0, 6 * RACK_GRID_WIDTH,RACK_GRID_HEIGHT);
		nvgFill(vg);

        const bool slopeSign = slope >= 0;
        const float slopeAbs = std::abs(slope);
        std::stringstream s;
    
        s.precision(1);
        s.setf( std::ios::fixed, std::ios::floatfield );
        s << slopeAbs << " db/oct";
        _slopeLabel->text = s.str();

        const char * mini = "\u2005-";
        _signLabel->text = slopeSign ? "+" : mini;
    }
};

/**
 * Widget constructor will describe my implementation structure and
 * provide meta-data.
 * This is not shared by all modules in the DLL, just one
 */
ColoredNoiseWidget::ColoredNoiseWidget(ColoredNoiseModule *module) : ModuleWidget(module)
{
 
    box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

    // save so we can update later.
    slopeLabel = new Label();
    signLabel = new Label();

    // add the color display
    #if 1
	{
		ColorDisplay *display = new ColorDisplay(slopeLabel, signLabel);
		display->module = module;
		display->box.pos = Vec( 0, 0);
		display->box.size = Vec(6 * RACK_GRID_WIDTH,RACK_GRID_HEIGHT);
		addChild(display);
        display->module = module;
	}
    #endif
    #if 1
    {
        SVGPanel *panel = new SVGPanel();
        panel->box.size = box.size;
        panel->setBackground(SVG::load(assetPlugin(plugin, "res/colors_panel.svg")));
        addChild(panel);
    }
    #endif

#if 0
    Label * label = new Label();
    label->box.pos = Vec(23, 24);
    label->text = "Noise";
    label->color = COLOR_BLACK;
    addChild(label);
    #endif

    addOutput(Port::create<PJ301MPort>(
        Vec(30, 310),
        Port::OUTPUT,
        module,
        module->noiseSource.AUDIO_OUTPUT));

//Davies1900hBlackKnob
//Rogan1PSWhite

    addParam(ParamWidget::create<Rogan2PSWhite>(
        Vec(22, 80), module, module->noiseSource.SLOPE_PARAM, -5.0, 5.0, 0.0));

    addParam(ParamWidget::create<Trimpot>(
        Vec(58, 46),
        module, module->noiseSource.SLOPE_TRIM, -1.0, 1.0, 1.0));

    addInput(Port::create<PJ301MPort>(
        Vec(14, 42),
        Port::INPUT,
        module,
        module->noiseSource.SLOPE_CV));


    const float labelY = 146;    
    slopeLabel->box.pos = Vec(12, labelY);
    slopeLabel->text = "foo";
    slopeLabel->color = COLOR_BLACK;
    addChild(slopeLabel);
    signLabel->box.pos = Vec(2, labelY);
    signLabel->text = "x";
    signLabel->color = COLOR_BLACK;
    addChild(signLabel);
}

Model *modelColoredNoiseModule = Model::create<ColoredNoiseModule, ColoredNoiseWidget>(
    "Squinky Labs",
    "squinkylabs-coloredNoise",
    "Colored Noise", EFFECT_TAG, FILTER_TAG);
