/**
 * @file
 */

#include "UIApp.h"
#include "TurboBadger.h"

#include "io/File.h"
#include "core/Command.h"
#include "core/Common.h"
#include "ui_renderer_gl.h"

extern void register_tbbf_font_renderer();

namespace ui {

namespace {
tb::MODIFIER_KEYS mapModifier(int16_t modifier) {
	tb::MODIFIER_KEYS code = tb::TB_MODIFIER_NONE;
	if (modifier & KMOD_ALT)
		code |= tb::TB_ALT;
	if (modifier & KMOD_CTRL)
		code |= tb::TB_CTRL;
	if (modifier & KMOD_SHIFT)
		code |= tb::TB_SHIFT;
	if (modifier & KMOD_GUI)
		code |= tb::TB_SUPER;
	return code;
}

tb::SPECIAL_KEY mapSpecialKey(int32_t key) {
	switch (key) {
	case SDLK_F1:
		return tb::TB_KEY_F1;
	case SDLK_F2:
		return tb::TB_KEY_F2;
	case SDLK_F3:
		return tb::TB_KEY_F3;
	case SDLK_F4:
		return tb::TB_KEY_F4;
	case SDLK_F5:
		return tb::TB_KEY_F5;
	case SDLK_F6:
		return tb::TB_KEY_F6;
	case SDLK_F7:
		return tb::TB_KEY_F7;
	case SDLK_F8:
		return tb::TB_KEY_F8;
	case SDLK_F9:
		return tb::TB_KEY_F9;
	case SDLK_F10:
		return tb::TB_KEY_F10;
	case SDLK_F11:
		return tb::TB_KEY_F11;
	case SDLK_F12:
		return tb::TB_KEY_F12;
	case SDLK_LEFT:
		return tb::TB_KEY_LEFT;
	case SDLK_UP:
		return tb::TB_KEY_UP;
	case SDLK_RIGHT:
		return tb::TB_KEY_RIGHT;
	case SDLK_DOWN:
		return tb::TB_KEY_DOWN;
	case SDLK_PAGEUP:
		return tb::TB_KEY_PAGE_UP;
	case SDLK_PAGEDOWN:
		return tb::TB_KEY_PAGE_DOWN;
	case SDLK_HOME:
		return tb::TB_KEY_HOME;
	case SDLK_END:
		return tb::TB_KEY_END;
	case SDLK_INSERT:
		return tb::TB_KEY_INSERT;
	case SDLK_TAB:
		return tb::TB_KEY_TAB;
	case SDLK_DELETE:
		return tb::TB_KEY_DELETE;
	case SDLK_BACKSPACE:
		return tb::TB_KEY_BACKSPACE;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		return tb::TB_KEY_ENTER;
	case SDLK_ESCAPE:
		return tb::TB_KEY_ESC;
	}
	return tb::TB_KEY_UNDEFINED;
}

int mapKey(int32_t key) {
	if (mapSpecialKey(key) == tb::TB_KEY_UNDEFINED)
		return key;
	return 0;
}

}

tb::UIRendererGL _renderer;

UIApp::UIApp(const io::FilesystemPtr& filesystem, const core::EventBusPtr& eventBus, uint16_t traceport) :
		WindowedApp(filesystem, eventBus, traceport), _quit(false) {
}

UIApp::~UIApp() {
}

bool UIApp::invokeKey(int key, tb::SPECIAL_KEY special, tb::MODIFIER_KEYS mod, bool down) {
#ifdef MACOSX
	bool shortcutKey = (mod & tb::TB_SUPER) ? true : false;
#else
	bool shortcutKey = (mod & tb::TB_CTRL) ? true : false;
#endif
	if (tb::TBWidget::focused_widget && down && shortcutKey) {
		bool reverseKey = (mod & tb::TB_SHIFT) ? true : false;
		if (key >= 'a' && key <= 'z')
			key += 'A' - 'a';
		tb::TBID id;
		if (key == 'X')
			id = TBIDC("cut");
		else if (key == 'C' || special == tb::TB_KEY_INSERT)
			id = TBIDC("copy");
		else if (key == 'V' || (special == tb::TB_KEY_INSERT && reverseKey))
			id = TBIDC("paste");
		else if (key == 'A')
			id = TBIDC("selectall");
		else if (key == 'Z' || key == 'Y') {
			bool undo = key == 'Z';
			if (reverseKey)
				undo = !undo;
			id = undo ? TBIDC("undo") : TBIDC("redo");
		} else if (key == 'N')
			id = TBIDC("new");
		else if (key == 'O')
			id = TBIDC("open");
		else if (key == 'S')
			id = TBIDC("save");
		else if (key == 'W')
			id = TBIDC("close");
		else if (special == tb::TB_KEY_PAGE_UP)
			id = TBIDC("prev_doc");
		else if (special == tb::TB_KEY_PAGE_DOWN)
			id = TBIDC("next_doc");
		else
			return false;

		tb::TBWidgetEvent ev(tb::EVENT_TYPE_SHORTCUT);
		ev.modifierkeys = mod;
		ev.ref_id = id;
		return tb::TBWidget::focused_widget->InvokeEvent(ev);
	}

	if (key >= ' ' && key <= '~')
		return false;
	return _root.InvokeKey(key, special, mod, down);
}

void UIApp::onMouseWheel(int32_t x, int32_t y) {
	if (_console.onMouseWheel(x, y)) {
		return;
	}
	int posX, posY;
	SDL_GetMouseState(&posX, &posY);
	_root.InvokeWheel(posX, posY, x, -y, mapModifier(SDL_GetModState()));
}

void UIApp::onMouseMotion(int32_t x, int32_t y, int32_t relX, int32_t relY) {
	if (_console.isActive()) {
		return;
	}
	_root.InvokePointerMove(x, y, mapModifier(SDL_GetModState()), false);
}

void UIApp::onMouseButtonPress(int32_t x, int32_t y, uint8_t button) {
	if (_console.isActive()) {
		return;
	}
	if (button != SDL_BUTTON_LEFT) {
		return;
	}
	static double lastTime = 0;
	static int lastX = 0;
	static int lastY = 0;
	static int counter = 1;

	const double time = tb::TBSystem::GetTimeMS();
	if (time < lastTime + 600 && lastX == x && lastY == y)
		++counter;
	else
		counter = 1;
	lastX = x;
	lastY = y;
	lastTime = time;

	_root.InvokePointerDown(x, y, counter, mapModifier(SDL_GetModState()), false);
}

void UIApp::onMouseButtonRelease(int32_t x, int32_t y, uint8_t button) {
	if (_console.isActive()) {
		return;
	}
	if (button == SDL_BUTTON_RIGHT) {
		_root.InvokePointerMove(x, y, mapModifier(SDL_GetModState()), false);
		tb::TBWidget* hover = tb::TBWidget::hovered_widget;
		if (hover != nullptr) {
			hover->ConvertFromRoot(x, y);
			tb::TBWidgetEvent ev(tb::EVENT_TYPE_CONTEXT_MENU, x, y, false, mapModifier(SDL_GetModState()));
			hover->InvokeEvent(ev);
		}
	} else {
		_root.InvokePointerUp(x, y, mapModifier(SDL_GetModState()), false);
	}
}

bool UIApp::onKeyRelease(int32_t key) {
	// TODO: broken for modifiers maybe ignore if text input is active
	video::WindowedApp::onKeyRelease(key);
	if (_console.isActive()) {
		return true;
	}
	const tb::MODIFIER_KEYS mod = mapModifier(SDL_GetModState());
	if (key == SDLK_MENU && tb::TBWidget::focused_widget) {
		tb::TBWidgetEvent ev(tb::EVENT_TYPE_CONTEXT_MENU);
		ev.modifierkeys = mod;
		tb::TBWidget::focused_widget->InvokeEvent(ev);
		return true;
	}
	return invokeKey(mapKey(key), mapSpecialKey(key), mod, false);
}

bool UIApp::onTextInput(const std::string& text) {
	if (_console.onTextInput(text)) {
		return true;
	}
	const char *c = text.c_str();
	for (;;) {
		const int key = core::string::getUTF8Next(&c);
		if (key == -1)
			return true;
		// TODO: broken for modifiers
		_root.InvokeKey(key, tb::TB_KEY_UNDEFINED, tb::TB_MODIFIER_NONE, true);
		_root.InvokeKey(key, tb::TB_KEY_UNDEFINED, tb::TB_MODIFIER_NONE, false);
	}
	return true;
}

bool UIApp::onKeyPress(int32_t key, int16_t modifier) {
	if (_console.onKeyPress(key, modifier)) {
		return true;
	}

	if (WindowedApp::onKeyPress(key, modifier)) {
		return true;
	}

	// TODO: broken for modifiers maybe ignore if text input is active
	return invokeKey(mapKey(key), mapSpecialKey(key), mapModifier(modifier), true);
}

core::AppState UIApp::onConstruct() {
	const core::AppState state = WindowedApp::onConstruct();
	core::Command::registerCommand("cl_ui_debug", [&] (const core::CmdArgs& args) {
#ifdef DEBUG
		tb::ShowDebugInfoSettingsWindow(&_root);
#endif
	});

	core::Command::registerCommand("quit", [&] (const core::CmdArgs& args) {_quit = true;});

	core::Command::registerCommand("bindlist", [this] (const core::CmdArgs& args) {
		for (core::BindMap::const_iterator i = _bindings.begin(); i != _bindings.end(); ++i) {
			const int32_t key = i->first;
			const core::CommandModifierPair& pair = i->second;
			const char* keyName = SDL_GetKeyName(key);
			const int16_t modifier = pair.second;
			std::string modifierKey;
			if (modifier & KMOD_ALT) {
				modifierKey += "ALT ";
			}
			if (modifier & KMOD_SHIFT) {
				modifierKey += "SHIFT ";
			}
			if (modifier & KMOD_CTRL) {
				modifierKey += "CTRL ";
			}
			const std::string& command = pair.first;
			Log::info("%-15s %-10s %s", modifierKey.c_str(), keyName, command.c_str());
		}
	});

	core::Command::registerCommand("bind", [this] (const core::CmdArgs& args) {
		if (args.size() != 2) {
			Log::error("Expected parameters: key+modifier command - got %i parameters", (int)args.size());
			return;
		}

		core::KeybindingParser p(args[0], args[1]);
		const core::BindMap& bindings = p.getBindings();
		for (core::BindMap::const_iterator i = bindings.begin(); i != bindings.end(); ++i) {
			const uint32_t key = i->first;
			const core::CommandModifierPair& pair = i->second;
			auto range = _bindings.equal_range(key);
			bool found = false;
			for (auto it = range.first; it != range.second; ++it) {
				if (it->second.second == pair.second) {
					it->second.first = pair.first;
					found = true;
					Log::info("Updated binding for key %s", args[0].c_str());
					break;
				}
			}
			if (!found) {
				_bindings.insert(std::make_pair(key, pair));
				Log::info("Added binding for key %s", args[0].c_str());
			}
		}
	});

	return state;
}

void UIApp::afterUI() {
	const tb::TBRect rect(0, 0, _dimension.x, _dimension.y);
	_console.render(rect, _deltaFrame);
}

core::AppState UIApp::onInit() {
	const core::AppState state = WindowedApp::onInit();
	if (!tb::tb_core_init(&_renderer)) {
		Log::error("failed to initialize the ui");
		return core::AppState::Cleanup;
	}

	tb::TBWidgetListener::AddGlobalListener(this);

	tb::g_tb_lng->Load("ui/lang/en.tb.txt");

	if (!tb::g_tb_skin->Load("ui/skin/skin.tb.txt", nullptr)) {
		Log::error("could not load the skin");
		return core::AppState::Cleanup;
	}

	if (!_renderer.init()) {
		Log::error("could not init ui renderer");
		return core::AppState::Cleanup;
	}

	tb::TBWidgetsAnimationManager::Init();

	register_tbbf_font_renderer();
	tb::g_font_manager->AddFontInfo("ui/font/font.tb.txt", "Segoe");
	tb::TBFontDescription fd;
	fd.SetID(TBIDC("Segoe"));
	fd.SetSize(tb::g_tb_skin->GetDimensionConverter()->DpToPx(14));
	tb::g_font_manager->SetDefaultFontDescription(fd);
	tb::TBFontFace *font = tb::g_font_manager->CreateFontFace(tb::g_font_manager->GetDefaultFontDescription());
	if (font == nullptr) {
		Log::error("could not create the font face");
		return core::AppState::Cleanup;
	}

	font->RenderGlyphs(" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNORSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~•·åäöÅÄÖ");
	_root.SetRect(tb::TBRect(0, 0, _dimension.x, _dimension.y));
	_root.SetSkinBg(TBIDC("background"));

	_console.init();

	_renderUI = core::Var::get(cfg::ClientRenderUI, "true");

	return state;
}

void UIApp::addChild(Window* window) {
	_root.AddChild(window);
}

core::AppState UIApp::onRunning() {
	if (_quit)
		return core::AppState::Cleanup;
	core::AppState state = WindowedApp::onRunning();

	const bool running = state == core::AppState::Running;
	if (running) {
		{
			core_trace_scoped(UIAppBeforeUI);
			beforeUI();
		}
		const bool renderUI = _renderUI->boolVal();
		if (renderUI) {
			core_trace_scoped(UIAppUpdateUI);
			tb::TBAnimationManager::Update();
			_root.InvokeProcessStates();
			_root.InvokeProcess();

			_renderer.BeginPaint(_dimension.x, _dimension.y);
			_root.InvokePaint(tb::TBWidget::PaintProps());

			++_frameCounter;

			double time = tb::TBSystem::GetTimeMS();
			if (time > _frameCounterResetRime + 1000) {
				fps = (int) round((_frameCounter / (time - _frameCounterResetRime)) * 1000);
				_frameCounterResetRime = time;
				_frameCounter = 0;
			}

			tb::TBStr str;
			str.SetFormatted("FPS: %d", fps);
			_root.GetFont()->DrawString(5, 5, tb::TBColor(255, 255, 255), str);
		}
		{
			core_trace_scoped(UIAppAfterUI);
			afterUI();
		}
		if (renderUI) {
			core_trace_scoped(UIAppEndPaint);
			_renderer.EndPaint();
			// If animations are running, reinvalidate immediately
			if (tb::TBAnimationManager::HasAnimationsRunning())
				_root.Invalidate();
		}
	}
	return state;
}

core::AppState UIApp::onCleanup() {
	tb::TBAnimationManager::AbortAllAnimations();
	tb::TBWidgetListener::RemoveGlobalListener(this);

	tb::TBWidgetsAnimationManager::Shutdown();
	tb::tb_core_shutdown();

	_console.shutdown();
	_renderer.shutdown();

	return WindowedApp::onCleanup();
}

}
