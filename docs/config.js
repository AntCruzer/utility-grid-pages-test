/* Code responsible for Setting page behaviour */

// Key variables
var DEFAULT_USE_CELSIUS = true;
var PAGE_STORAGE_KEY = 'useCelsius';

// ID settings form
var form = document.getElementById('settings-form');

// ID temperature-unit fields
var celsiusInput = document.getElementById('unit-celsius');
var fahrenheitInput = document.getElementById('unit-fahrenheit');



/**
 * Function: Reads a specific value from the page URL
 */
function getQueryParameter(parameterName) {

    // Retrieve all query parameters from the current page URL
    var parameters =
        new URLSearchParams(window.location.search);

    // Return the value belonging to the requested parameter
    return parameters.get(parameterName);
}


/**
 * Function: Determines which temperature unit should be selected
 */
function loadTemperatureUnitSetting() {

    // Determine whether or not celius was used
    var queryValue =
        getQueryParameter('useCelsius');

    // Case: PebbleKit JS supplied a setting through the URL
    if (queryValue !== null) {

        // Convert the string value into a boolean
        return queryValue === 'true';
    }


    // Read setting preference in local storage
    var savedPageValue =
        localStorage.getItem(PAGE_STORAGE_KEY);

    // Case: Webpage previously saved a setting
    if (savedPageValue !== null) {

        // Convert the saved string into a boolean
        return savedPageValue === 'true';
    }

    // Case: initial page load
    // Return the default temperature unit if no saved value exists
    return DEFAULT_USE_CELSIUS;
}


// Set initial temperature-unit selection when the settings page opens

var useCelsius = loadTemperatureUnitSetting();

celsiusInput.checked = useCelsius;
fahrenheitInput.checked = !useCelsius;


/**
 * Event: User saves setting preference
 */
form.addEventListener('submit', function(event) {

    event.preventDefault(); // ensures preference is saved on submission

    // Create settings object using the selected temperature unit
    var settings = {
        useCelsius: celsiusInput.checked
    };

    // Local storage save
    localStorage.setItem(
        PAGE_STORAGE_KEY,                   // Temperature-unit key
        String(settings.useCelsius)         // Boolean casted to string
    );


    // Convert settings object into JSON text
    var settingsJSON = JSON.stringify(settings);

    /////////
    // Encode settings text so that it can safely be placed in a URL
    var encodedSettings = encodeURIComponent(settingsJSON);

    // Close the configuration settings webpage
    window.location.href =
        'pebblejs://close#' + encodedSettings;
});
