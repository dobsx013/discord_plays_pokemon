var Discord = require('discord.io');
var logger = require('winston');
var auth = require('./auth.json');

const discord_plays_pokemon = require('./build/Release/discord_plays_pokemon.node');

// Configure logger settings
logger.remove(logger.transports.Console);
logger.add(new logger.transports.Console, {
    colorize: true
});
logger.level = 'debug';

logger.info('hello world');

// Initialize Discord Bot
var bot = new Discord.Client({
   token: auth.token,
   autorun: true
});
bot.on('ready', function (evt) {
    logger.info('Connected');
    logger.info('Logged in as: ');
    logger.info(bot.username + ' - (' + bot.id + ')');
});
bot.on('message', function (user, userID, channelID, message, evt) {
	const wobbots_playground_specific_channel_id = "737726525880926278";
	const wobbot_hideout_channel_id = "737375420348891216";
	
	if ((channelID != wobbots_playground_specific_channel_id) && (channelID != wobbot_hideout_channel_id)) {
		// this message is not on a channel that we care about
		logger.info('message is on the wrong channel');
		return;
	}
	
    // Our bot needs to know if it will execute a command
    // It will listen for messages that will start with `!`
	
	if (message.length > 3) {
		logger.info('message is too long, expect 1, 2, or 3 characters');
		return;
	}
	
	var command = message.substring(0, 1);
	if (command === 's') {
		if (message.length < 2) {
			logger.info('invalid message: \'s\', did you mean st or se?');
			return;
		}
		command = message.substring(0,2);
	}
	
	var multiplier = 1;
	
	logger.info('message: ' + message);
	
	switch (command) {
		case discord_plays_pokemon.kUP_BUTTON:
		case discord_plays_pokemon.kDOWN_BUTTON:
		case discord_plays_pokemon.kLEFT_BUTTON:
		case discord_plays_pokemon.kRIGHT_BUTTON:
		case discord_plays_pokemon.kA_BUTTON:
		case discord_plays_pokemon.kB_BUTTON:
		{
			if (message.length > 1) {
				multiplier = message.substring(1, 2);
				if (isNaN(multiplier)) {
					logger.info('second argument is not a number');
					return;
				}
			}
			
			logger.info('number of times ' + multiplier);
			
			var input_command_wrapper = new discord_plays_pokemon.InputCommandWrapper(command, multiplier);
			
			/* it will automagically queue itself and then the back end will pop off & issue it -- the same is true for all cases here */
			
			/* input_command_wrapper will be garbage collected at some point after it falls out of scope */
			break;
		}
		case discord_plays_pokemon.kSTART_BUTTON:
		case discord_plays_pokemon.kSELECT_BUTTON:
		{
			if (message.length > 2) {
				multiplier = message.substring(2, 3);
				if (isNaN(multiplier)) {
					logger.info('third argument is not a number');
					return;
				}
			}
			
			var input_command_wrapper = new discord_plays_pokemon.InputCommandWrapper(command, multiplier);
			
			/* it will automagically queue itself and then the back end will pop off & issue it -- the same is true for all cases here */
			
			/* input_command_wrapper will be garbage collected at some point after it falls out of scope */
			
			break;
		}	
		default:
			/* not a handled button type, ignore */
			return;
	}
});