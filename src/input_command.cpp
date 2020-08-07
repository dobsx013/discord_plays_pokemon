#include "input_command.h"
#include <iostream>
#include <mutex>
#include <iostream>
#include <map>

const char* kUP_BUTTON = "u";
const char* kDOWN_BUTTON = "d";
const char* kLEFT_BUTTON = "l";
const char* kRIGHT_BUTTON = "r";
const char* kSTART_BUTTON = "st";
const char* kSELECT_BUTTON = "se";
const char* kA_BUTTON = "a";
const char* kB_BUTTON = "b";

#define STRINGIFY(x) #x

DiscordPlaysPokemonQueueType gbl_queue;
std::mutex gbl_mutex;

InputCommandQueueMonitor *p_gbl_input_command_queue_monitor = NULL;

/* map of input strings to virtual key codes */
const std::map<std::string, WORD> string_to_virtual_key_code = {
		{kUP_BUTTON, 		0x54},	/* T key */
		{kDOWN_BUTTON, 		0x47}, 	/* G key */
		{kLEFT_BUTTON, 		0x46}, 	/* F key */
		{kRIGHT_BUTTON, 	0x48}, 	/* H key */
		{kSTART_BUTTON,		0x5A},	/* Z key */
		{kSELECT_BUTTON,	0x58},	/* X key */
		{kA_BUTTON, 		0x41},	/* A key */
		{kB_BUTTON,			0x42}	/* B key */
};
const std::map<int, WORD> multiplier_to_button_hold_time = {
		{1, 166},
		{2, 333},
		{3, 666},
		{4, 999},
		{5, 1320},
		{6, 1650},
		{7, 1980},
		{8, 2250},
		{9, 2560}
};

/* InputCommand ... */
InputCommand::InputCommand(DiscordPlaysPokemonQueueType &queue, std::mutex &mutex, INPUT input, const int multiplier) :
	queue_(queue),
	mutex_(mutex),
	input_(input),
	multiplier_(multiplier)
{
	push();
}

InputCommand::~InputCommand()
{
	
}

void InputCommand::IssueInputCommand() {
	std::cout << "Called IssueInputCommand" << std::endl;
	
	auto search_result = multiplier_to_button_hold_time.find(multiplier_);
	if (search_result == multiplier_to_button_hold_time.end()) {
		std::cout << "Invalid or unsupported multiplier: " << multiplier_ << std::endl;
		return;
	}
	
	/* press down the button */
	input_.ki.dwFlags = 0; // SD - add this back in KEYEVENTF_SCANCODE; // SCANCODE for hardware press
	SendInput(1, &input_, sizeof(input_));
	
	std::cout << "sleep time: " << search_result->second << std::endl;
	
	/* TODO: may have to tweak how long we want to hold the button down for ... */	
	Sleep(search_result->second);
	
	
	//Sleep(5000); // a hack to test something
	
	/* release the button */
	input_.ki.dwFlags = /* SD - add this back in KEYEVENTF_SCANCODE | */ KEYEVENTF_KEYUP; // SCANCODE for hardware press, KEYEVENTF_KEYUP for key release
	SendInput(1, &input_, sizeof(input_));
	
	return;
}

void InputCommand::push() {
	std::lock_guard<std::mutex> lock(mutex_);
	
	queue_.push(this);
	
	/* std::lock_guard should be smart enough to release the lock when it falls out of scope */
}
/* ... InputCommand */

/* InputCommandWrapper... */
InputCommandWrapper::InputCommandWrapper(const Napi::CallbackInfo& info) : ObjectWrap(info) {
	Napi::Env env = info.Env();
	
	if (info.Length() < 1) {
		Napi::TypeError::New(env, "Wrong number of arguments").ThrowAsJavaScriptException();
		return;
	}
	
	std::cout << "info length: " << info.Length() << std::endl;
	
	if (!info[0].IsString()) {
		Napi::TypeError::New(env, "Javascript input command should be string").ThrowAsJavaScriptException();
		return;
	}
		
	std::string javascript_input_command = info[0].As<Napi::String>().Utf8Value();
	
	std::cout << "javascript_input_command: " << javascript_input_command << std::endl;
	
	int multiplier = 1;
	if (info.Length() > 1) {
		multiplier = (int)info[1].ToNumber();
		if ((1 > multiplier) || (10 < multiplier)) {
			std::cout << "Invalid number of times" << std::endl;
			return;
		}
	}
	
	std::cout << "multiplier: " << multiplier << std::endl;
	
	INPUT input = { 0 };
	if (SD_SUCCESS != ConvertStringToInput(javascript_input_command, input)) {
		Napi::TypeError::New(env, "Failed to convert input javascript command to windows input").ThrowAsJavaScriptException();
		return;
	}
	
	/* do not call delete on this, it will push itself onto the queue */
	/* it is then the responsibility of the thread monitor to delete it when it pops it off the queue */
	InputCommand *input_command = new InputCommand(gbl_queue, gbl_mutex, input, multiplier);
	if (NULL == input_command) {
		Napi::TypeError::New(env, "Failed to create internal input command instance").ThrowAsJavaScriptException();
		return;
	}

	std::cout << "Successfully created input command with multiplier: " << multiplier << std::endl;
}

InputCommandWrapper::~InputCommandWrapper() {
	
}

Napi::Function InputCommandWrapper::GetClass(Napi::Env env) {
	return DefineClass(env, "InputCommandWrapper", { });
}

SD_RETCODE InputCommandWrapper::ConvertStringToInput(std::string input_as_string, INPUT &out_input) {
	
	memset(&out_input, 0, sizeof(out_input));
	
	auto search_result = string_to_virtual_key_code.find(input_as_string);
	
	if (string_to_virtual_key_code.end() == search_result) {
		std::cout << "Invalid or unsupported input: " << input_as_string;
		return SD_FAILURE;
	}
	
	out_input.type = INPUT_KEYBOARD;
	//out_input.ki.wVk = 0;	// virtual key code for key
	out_input.ki.wVk = search_result->second; // SD - trying something.... delete THIS
	out_input.ki.time = 0;
	out_input.ki.dwExtraInfo = 0;
	
	out_input.ki.wScan = 0; // SD - TRYING SOMETHING ADD THIS BACK INsearch_result->second; // hardware scan code for key
	
	
	return SD_SUCCESS;
}
/* ... InputCommandWrapper */



/* InputCommandQueueMonitor... */
InputCommandQueueMonitor::InputCommandQueueMonitor(DiscordPlaysPokemonQueueType &queue, std::mutex &mutex) :
	queue_(queue),
	mutex_(mutex),
	stop_thread_(false),
	event_check_period_msec_(10),
	animation_time_msec_(289)
{
	/* we cannot call StartThread from here, since it could potentially start before we're done constructing ourself */
	/* it will have to be up to the caller to call Start on us */
}

InputCommandQueueMonitor::~InputCommandQueueMonitor()
{
	/* should never happen... if it does then we will leak memory forever */
	/* this is probably the sign of a bad design, but i'm tired and want to get this working */
	
	/* really the only time that we should destruct this is when the program is going offline */
	
	stop_thread_ = true;
	
	if (thread_.joinable()) {
		thread_.join();
	}
}

void InputCommandQueueMonitor::StartThread()
{
	thread_ = std::thread(&InputCommandQueueMonitor::ThreadMain, this);
}

void InputCommandQueueMonitor::ThreadMain()
{
	while (!stop_thread_) {
		Sleep(event_check_period_msec_);
		
		mutex_.lock();
		
		if (queue_.empty()) {
			mutex_.unlock();
			continue;
		}
		/* there is data for us to process */
		
		InputCommand *p_data = queue_.front();
		queue_.pop();
		
		mutex_.unlock();
		
		p_data->IssueInputCommand();
		delete p_data;
		
		// removing this for now... we're going to be holding the button for the whole duration now
		// // we're moving this to be how long we hold the button down for...
		//Sleep(animation_time_msec_);
	}
}

/* ... InputCommandQueueMonitor */


// callback method when module is registered with Node.js
Napi::Object Init(Napi::Env env, Napi::Object exports) {

	std::cout << "Called Napi::Init!" << std::endl;

	p_gbl_input_command_queue_monitor = new InputCommandQueueMonitor(gbl_queue, gbl_mutex);
	if (NULL == p_gbl_input_command_queue_monitor) {
		Napi::TypeError::New(env, "Failed to create input command queue monitor").ThrowAsJavaScriptException();
		return exports;
	}
	p_gbl_input_command_queue_monitor->StartThread();

	Napi::String class_name = Napi::String::New(env, "InputCommandWrapper");
	exports.Set(class_name, InputCommandWrapper::GetClass(env));
	
	/* this is ugly... if we had an enum we could walk over it and set all this automagically */
	/* if we ever need to add new types we should look into this */
	Napi::String up_button_name = Napi::String::New(env, STRINGIFY(kUP_BUTTON));
	Napi::String up_button_value = Napi::String::New(env, kUP_BUTTON);
	exports.Set(up_button_name, up_button_value);
	
	Napi::String down_button_name = Napi::String::New(env, STRINGIFY(kDOWN_BUTTON));
	Napi::String down_button_value = Napi::String::New(env, kDOWN_BUTTON);
	exports.Set(down_button_name, down_button_value);
	
	Napi::String left_button_name = Napi::String::New(env, STRINGIFY(kLEFT_BUTTON));
	Napi::String left_button_value = Napi::String::New(env, kLEFT_BUTTON);
	exports.Set(left_button_name, left_button_value);
	
	Napi::String right_button_name = Napi::String::New(env, STRINGIFY(kRIGHT_BUTTON));
	Napi::String right_button_value = Napi::String::New(env, kRIGHT_BUTTON);
	exports.Set(right_button_name, right_button_value);
	
	Napi::String start_button_name = Napi::String::New(env, STRINGIFY(kSTART_BUTTON));
	Napi::String start_button_value = Napi::String::New(env, kSTART_BUTTON);
	exports.Set(start_button_name, start_button_value);
	
	Napi::String select_button_name = Napi::String::New(env, STRINGIFY(kSELECT_BUTTON));
	Napi::String select_button_value = Napi::String::New(env, kSELECT_BUTTON);
	exports.Set(select_button_name, select_button_value);
	
	Napi::String a_button_name = Napi::String::New(env, STRINGIFY(kA_BUTTON));
	Napi::String a_button_value = Napi::String::New(env, kA_BUTTON);
	exports.Set(a_button_name, a_button_value);
	
	Napi::String b_button_name = Napi::String::New(env, STRINGIFY(kB_BUTTON));
	Napi::String b_button_value = Napi::String::New(env, kB_BUTTON);
	exports.Set(b_button_name, b_button_value);

	return exports;
}

// register 'discord_plays_pokemon' module which calls 'Init' method
NODE_API_MODULE(discord_plays_pokemon, Init);
