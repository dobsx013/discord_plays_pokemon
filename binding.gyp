{
	"targets": [
		{
			"target_name": "discord_plays_pokemon",
			"cflags!": ["-fno-exceptions"],
			"cflags_cc": ["-fno-exceptions"],
			"sources": ["src/input_command.cpp"],
			"include_dirs": [ "<!@(node -p \"require('node-addon-api').include\")" ],
			'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ]
		}
	]
}
