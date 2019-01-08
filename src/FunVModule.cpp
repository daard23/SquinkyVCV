
#include <sstream>
#include "Squinky.hpp"

#ifdef _FUN
#include "WidgetComposite.h"
#include "ctrl/SQHelper.h"
#include "ctrl/SqMenuItem.h"
#include "FunVCOComposite.h"

/**
 */
struct FunVModule : Module
{
public:
    FunVModule();
    /**
     *
     * Overrides of Module functions
     */
    void step() override;
    void onSampleRateChange() override;

    FunVCOComposite<WidgetComposite> vco;
private:
};

void FunVModule::onSampleRateChange()
{
    float rate = SQHelper::engineGetSampleRate();
    vco.setSampleRate(rate);
}

#ifdef _V1
FunVModule::FunVModule() : vco(this)
{
    		// Set the number of components
	//setup(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    setup(vco.NUM_PARAMS, vco.NUM_INPUTS, vco.NUM_OUTPUTS, vco.NUM_LIGHTS);
    onSampleRateChange();
    SQHelper::setupParams(vco, this);
}
#else
FunVModule::FunVModule()
    : Module(vco.NUM_PARAMS,
    vco.NUM_INPUTS,
    vco.NUM_OUTPUTS,
    vco.NUM_LIGHTS),
    vco(this)
{
    onSampleRateChange();
}
#endif

void FunVModule::step()
{
    vco.step();
}

////////////////////
// module widget
////////////////////

struct FunVWidget : ModuleWidget
{
    FunVWidget(FunVModule *);

    void addTop3(FunVModule *, float verticalShift);
    void addMiddle4(FunVModule *, float verticalShift);
    void addJacks(FunVModule *, float verticalShift);
    Menu* createContextMenu() override;

    Label* addLabel(const Vec& v, const char* str, const NVGcolor& color)
    {
        Label* label = new Label();
        label->box.pos = v;
        label->text = str;
        label->color = color;
        addChild(label);
        return label;
    }
    Label* addLabel(const Vec& v, const char* str) 
    {
        return addLabel(v, str, SQHelper::COLOR_BLACK);
    }
};

inline Menu* FunVWidget::createContextMenu()
{
    Menu* theMenu = ModuleWidget::createContextMenu();
    ManualMenuItem* manual = new ManualMenuItem(
        "https://github.com/squinkylabs/SquinkyVCV/blob/master/docs/functional-vco-1.md");
    theMenu->addChild(manual);
    return theMenu;
}

void FunVWidget::addTop3(FunVModule * module, float verticalShift)
{
    const float left = 8;
    const float right = 112;
    const float center = 49;

    addParam(SQHelper::createParam<NKK>(
        module->vco,
        Vec(left, 66 + verticalShift),
        module,
        module->vco.MODE_PARAM));
    addLabel(Vec(left -4, 48+ verticalShift), "anlg");
    addLabel(Vec(left -3, 108+ verticalShift), "dgtl");

    addParam(SQHelper::createParam<Rogan3PSBlue>(
        module->vco,
        Vec(center, 61 + verticalShift),
        module, 
        module->vco.FREQ_PARAM));
    auto label = addLabel(Vec(center + 3, 40+ verticalShift), "pitch");
    label->fontSize = 16;

    addParam(SQHelper::createParam<NKK>(
        module->vco,
        Vec(right, 66 + verticalShift),
        module,
        module->vco.SYNC_PARAM));
    addLabel(Vec(right-5, 48+ verticalShift), "hard");
    addLabel(Vec(right-2, 108+ verticalShift), "soft");
}

void FunVWidget::addMiddle4(FunVModule * module, float verticalShift)
{
    addParam(SQHelper::createParam<Rogan1PSBlue>(
        module->vco,
        Vec(23, 143 + verticalShift),
        module, module->vco.FINE_PARAM));
    addLabel(Vec(25, 124 +verticalShift), "fine");

    addParam(SQHelper::createParam<Rogan1PSBlue>(
        module->vco,
        Vec(91, 143 + verticalShift),
        module, module->vco.PW_PARAM));
    addLabel(Vec(84, 124 +verticalShift), "p width");

    addParam(SQHelper::createParam<Rogan1PSBlue>(
        module->vco,
        Vec(23, 208 + verticalShift),
        module,
        module->vco.FM_PARAM));
    addLabel(Vec(19, 188 +verticalShift), "fm cv");

    addParam(SQHelper::createParam<Rogan1PSBlue>(
        module->vco,
        Vec(91, 208 + verticalShift),
        module, 
        module->vco.PWM_PARAM
    ));
    addLabel(Vec(82, 188 +verticalShift), "pwm cv");
}

void FunVWidget::addJacks(FunVModule * module, float verticalShift)
{
    const float col1 = 12;
    const float col2 = 46;
    const float col3 = 81;
    const float col4 = 115;
    const float outputLabelY = 300;

    // this is the v1 format
    addInput(createInput<PJ301MPort>(
        Vec(col1, 273+verticalShift),
        module,
        module->vco.PITCH_INPUT));
    addLabel(Vec(10, 255+verticalShift), "cv");

    addInput(createInput<PJ301MPort>(
        Vec(col2, 273+verticalShift),
        module,
        module->vco.FM_INPUT));
    addLabel(Vec(43, 255+verticalShift), "fm");

    addInput(createInput<PJ301MPort>(
        Vec(col3, 273+verticalShift),
        module,
        module->vco.SYNC_INPUT));
    addLabel(Vec(73, 255+verticalShift), "sync");

    addInput(createInput<PJ301MPort>(
        Vec(col4, 273+verticalShift),
        module,
        module->vco.PW_INPUT));
    addLabel(Vec(106, 255+verticalShift), "pwm");

    addOutput(createOutput<PJ301MPort>(
        Vec(col1, 317+verticalShift),
        module,
        module->vco.SIN_OUTPUT));
    addLabel(Vec(8, outputLabelY+verticalShift), "sin", SQHelper::COLOR_WHITE);

    addOutput(createOutput<PJ301MPort>(
        Vec(col2, 317+verticalShift),
        module,
        module->vco.TRI_OUTPUT));
    addLabel(Vec(44, outputLabelY+verticalShift), "tri", SQHelper::COLOR_WHITE);

    addOutput(createOutput<PJ301MPort>(
        Vec(col3, 317+verticalShift),
        module,
        module->vco.SAW_OUTPUT));
    addLabel(Vec(75, outputLabelY+verticalShift),
        "saw",
        SQHelper::COLOR_WHITE);

    addOutput(createOutput<PJ301MPort>(
        Vec(col4, 317+verticalShift),
        module,
        module->vco.SQR_OUTPUT));
 
    addLabel(Vec(111, outputLabelY+verticalShift), "sqr", SQHelper::COLOR_WHITE);
}

/**
 * Widget constructor will describe my implementation structure and
 * provide meta-data.
 * This is not shared by all modules in the DLL, just one
 */
FunVWidget::FunVWidget(FunVModule *module) : ModuleWidget(module)
{
    box.size = Vec(10 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
    {
        SVGPanel *panel = new SVGPanel();
        panel->box.size = box.size;   
        panel->setBackground(SVG::load(SQHelper::assetPlugin(plugin, "res/fun_panel.svg")));        
        addChild(panel);
    }

    addTop3(module, 0);
    addMiddle4(module, 0);
    addJacks(module, 0);

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
}
#ifndef _V1
Model *modelFunVModule = Model::create<FunVModule,
    FunVWidget>("Squinky Labs",
    "squinkylabs-funv",
    "Functional VCO-1", OSCILLATOR_TAG);
#else
Model *modelFunVModule = createModel<FunVModule, FunVWidget>(
    "fun1");
#endif


#endif

