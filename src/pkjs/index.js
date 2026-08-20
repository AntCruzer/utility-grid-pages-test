/* Code that runs on the phone side.
    Retrieves Open-Meto weather data and sends the data and unit setting to watch
    and retains the data with persisted storage */


var SETTINGS_URL = 'https://antcruzer.github.io/utility-grid-pages-test/';

var DEFAULT_USE_CELSIUS = true;

var STORAGE_KEY_USE_CELSIUS = 'useCelsius';



/**
 * Function: Reads temperature setting
 */
function readTemperatureUnitSetting() {

    // Retrieve setting from local storage
    var useCelsiusSetting =
        localStorage.getItem(STORAGE_KEY_USE_CELSIUS);

    // Case: Temperature unit was not saved
    if (useCelsiusSetting === null) {
        return DEFAULT_USE_CELSIUS;
    }

    // Return user's setting
    return useCelsiusSetting === 'true';
}


/**
 * Function: Sets temperature unit preference (local storage)
 */
function saveTemperatureUnitSetting(useCelsius) {

    localStorage.setItem(
        STORAGE_KEY_USE_CELSIUS,        // Temperature-unit key
        String(useCelsius)              // Boolean casted to string
    );
}


/**
 * Function: Sends setting preference appmessage, specified on JS to watch
 */
function sendSettingsToWatch(useCelsius) {

    // Dictionary containing tuple to be sent over to watch
    var dictionary = {
        UseCelsius: useCelsius ? 1 : 0
    };

    // Send message
    Pebble.sendAppMessage(dictionary);
}


/**
 * Function: Performs basic HTTP GET request (Open Meteo API)
 */
function xhrRequest(url, callback) {

    var xhr = new XMLHttpRequest();

    xhr.onload = function() {

        // Case: Successful response
        if (xhr.status >= 200 && xhr.status < 300) {
            callback(null, xhr.responseText);

        } else {    // Case: GET request error
            callback('HTTP error: ' + xhr.status);
        }
    };

    // Runs when GET request error occurs
    xhr.onerror = function() {
        callback('Network request failed');
    };

    // Send request for location data
    xhr.open('GET', url);
    xhr.send();
}


/**
 * Function: Runs after phone returns user's loaction
 */
function locationSuccess(position) {

    // Open Meteo Api link
    var url =
        'https://api.open-meteo.com/v1/forecast?' +
        'latitude=' + position.coords.latitude +
        '&longitude=' + position.coords.longitude +
        '&current=temperature_2m' +
        '&temperature_unit=celsius';

    xhrRequest(url, function(error, responseText) {

        // Case: Error occured for weather request
        if (error) {
            console.log('Weather request error: ' + error);
            return;
        }

        try {

            var weather = JSON.parse(responseText);

            var temperatureCelsius =
                Math.round(weather.current.temperature_2m);

            // Send temperature as an appmessage to watch
            Pebble.sendAppMessage({
                TemperatureCelsius: temperatureCelsius
            });

        } catch (parseError) {
            console.log(
                'Could not parse weather response: ' +
                parseError.message
            );
        }
    });
}


/**
 * Function: Runs if phone cannot provide user's location 
 */
function locationError(error) {

    console.log(
        'Location error (' +
        error.code +
        '): ' +
        error.message
    );
}


/**
 * Function: Requests current weather using phone's location
 */
function getWeather() {

    navigator.geolocation.getCurrentPosition(
        locationSuccess,
        locationError,
        {
            enableHighAccuracy: false,
            timeout: 15000,             // 15 secs
            maximumAge: 60000           // 1 min
        }
    );
}


/**
 * Event: PebbleKit JS is ready
 */
Pebble.addEventListener('ready', function() {

    console.log('PebbleKit JS ready!');

    var useCelsius = readTemperatureUnitSetting();

    // Resync the phone-side preference to the watch
    sendSettingsToWatch(useCelsius);

    // Fetch weather when the watchface starts
    getWeather();
});


/**
 * Event: Watch requests data from PebbleKit JS
 */
Pebble.addEventListener('appmessage', function(event) {

    if (event.payload.RequestWeather) {
        getWeather();
    }
});


/**
 * Event: Customer open watchface settings on the Pebble app
 */
Pebble.addEventListener(
    'showConfiguration',
    function() {

        var useCelsius = readTemperatureUnitSetting();

        var url =
            SETTINGS_URL +
            '?useCelsius=' +
            encodeURIComponent(String(useCelsius));

        console.log('Opening configuration: ' + url);

        Pebble.openURL(url);
    }
);


/**
 * Event: Settings page is closed
 */
Pebble.addEventListener(
    'webviewclosed',
    function(event) {

        // Case: Settings page closed without saving
        if (!event.response) {

            console.log(
                'Configuration closed without saving'
            );

            return;
        }

        // Persist preferred setting
        try {

            var decodedResponse =
                decodeURIComponent(event.response);

            var settings =
                JSON.parse(decodedResponse);

            var useCelsius =
                settings.useCelsius === true;

            // Save on the phone
            saveTemperatureUnitSetting(useCelsius);

            // Transfer to the watch
            sendSettingsToWatch(useCelsius);

        } catch (error) {   // Data persistence fail

            console.log(
                'Could not parse configuration response: ' +
                error.message
            );
        }
    }
);

