#ifndef __INPUT_COMMAND__H
#define __INPUT_COMMAND__H

#define WINVER 0x0500
#include <windows.h>
#include <napi.h>
#include <queue>
#include <mutex>
#include <utility>

typedef int SD_RETCODE;
#define SD_SUCCESS ((SD_RETCODE) 0)
#define SD_FAILURE ((SD_RETCODE) 1)

typedef enum INPUT_COMMAND_ENUM {
	UP = 0,
	DOWN,
	LEFT,
	RIGHT,
	A,
	B,
	START,
	SELECT
} INPUT_COMMAND_ENUM_TYPE;

#ifdef __cplusplus

/* forward declaration */
class InputCommand;

typedef std::queue<InputCommand *> DiscordPlaysPokemonQueueType;

class InputCommand {
public:
	InputCommand(DiscordPlaysPokemonQueueType &queue, std::mutex &mutex, INPUT input, const int multiplier);
	~InputCommand();
	
	void IssueInputCommand();
	
private:
	void push();
	const int multiplier_;

	DiscordPlaysPokemonQueueType &queue_;
	std::mutex &mutex_;
	INPUT input_;
};

/* we need to inherit from ObjectWrap, because this is the class that we want to be accessible from javascript */
class InputCommandWrapper : public Napi::ObjectWrap<InputCommandWrapper> {
public:
	InputCommandWrapper(const Napi::CallbackInfo& env);
	~InputCommandWrapper();
	
	static Napi::Function GetClass(Napi::Env);
	
private:
	SD_RETCODE ConvertStringToInput(std::string input_as_string, INPUT &out_input);
};

class InputCommandQueueMonitor {
public:
	InputCommandQueueMonitor(DiscordPlaysPokemonQueueType &queue, std::mutex &mutex);
	~InputCommandQueueMonitor();
	
	void StartThread();
	
	
private:
	std::thread thread_; /* not a ready to use thread yet, we need to call "StartThread" to get it going */
	bool stop_thread_;
	const DWORD event_check_period_msec_;
	const DWORD animation_time_msec_;
	
	void ThreadMain();
	
	DiscordPlaysPokemonQueueType &queue_;
	std::mutex &mutex_;
};

#endif /* __cplusplus */

#endif /* __INPUT_COMMAND__H */
