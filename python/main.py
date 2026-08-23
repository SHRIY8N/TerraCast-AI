import json
import time
import urllib.request
import urllib.parse

from arduino.app_utils import Bridge, App


# ============================================================
# TERRACAST AI
# Python / Linux side
# ============================================================

CONFIG_FILE = "config.json"


# ------------------------------------------------------------
# Load configuration
# ------------------------------------------------------------

with open(CONFIG_FILE, "r") as file:
    CONFIG = json.load(file)


CITY = CONFIG["city"]
LATITUDE = CONFIG["latitude"]
LONGITUDE = CONFIG["longitude"]

UPDATE_SECONDS = (
    CONFIG["weather_update_minutes"] * 60
)


# ------------------------------------------------------------
# Weather state
# ------------------------------------------------------------

last_weather_update = 0

last_temperature = 28
last_rain_probability = 0
last_rain_amount = 0.0


# ------------------------------------------------------------
# Fetch weather
# ------------------------------------------------------------

def fetch_weather():

    global last_temperature
    global last_rain_probability
    global last_rain_amount

    params = {
        "latitude": LATITUDE,
        "longitude": LONGITUDE,
        "current": "temperature_2m",
        "hourly": (
            "precipitation_probability,"
            "precipitation"
        ),
        "forecast_days": 1,
        "timezone": "auto"
    }

    url = (
        "https://api.open-meteo.com/v1/forecast?"
        + urllib.parse.urlencode(params)
    )

    try:

        with urllib.request.urlopen(
            url,
            timeout=10
        ) as response:

            data = json.loads(
                response.read().decode()
            )

        # Current temperature
        current = data.get(
            "current",
            {}
        )

        temperature = current.get(
            "temperature_2m",
            28
        )

        # Hourly forecast
        hourly = data.get(
            "hourly",
            {}
        )

        probabilities = hourly.get(
            "precipitation_probability",
            []
        )

        precipitation = hourly.get(
            "precipitation",
            []
        )

        # Look at the next several hours.
        # This is more useful for irrigation than
        # looking at only one hour.

        window = min(
            6,
            len(probabilities)
        )

        if window > 0:

            rain_probability = max(
                probabilities[:window]
            )

        else:

            rain_probability = 0


        rain_amount = 0.0

        for value in precipitation[:window]:
            rain_amount += float(value)


        last_temperature = int(
            round(float(temperature))
        )

        last_rain_probability = int(
            rain_probability
        )

        last_rain_amount = rain_amount


        print(
            "Weather:",
            CITY,
            "| Temp:",
            last_temperature,
            "C",
            "| Rain probability:",
            last_rain_probability,
            "%",
            "| Rain:",
            round(last_rain_amount, 1),
            "mm"
        )

        return True


    except Exception as error:

        print(
            "Weather update failed:",
            error
        )

        return False


# ------------------------------------------------------------
# Send weather to Arduino
# ------------------------------------------------------------

def send_weather():

    global last_temperature
    global last_rain_probability
    global last_rain_amount

    # Bridge only receives integers.
    #
    # Rain amount is multiplied by 10.
    #
    # Example:
    # 2.5 mm → 25

    rain_tenths = int(
        round(
            last_rain_amount * 10
        )
    )

    try:

        rpc = Bridge.call(
            "set_forecast",
            int(last_temperature),
            int(last_rain_probability),
            int(rain_tenths)
        )

        # Wait for the MCU response.
        #
        # set_forecast returns void,
        # so just consume the call result.

        rpc.result()

        print(
            "Weather sent to Arduino."
        )

    except Exception as error:

        print(
            "Bridge weather error:",
            error
        )


# ------------------------------------------------------------
# Read Arduino mode
# ------------------------------------------------------------

def get_demo_mode():

    try:

        rpc = Bridge.call(
            "get_demo_mode"
        )

        mode = 0

        if rpc.result(mode):

            return int(mode)

    except Exception as error:

        print(
            "Mode read error:",
            error
        )

    return 0


# ------------------------------------------------------------
# Read soil moisture
# ------------------------------------------------------------

def get_moisture():

    try:

        rpc = Bridge.call(
            "get_moisture_percent"
        )

        moisture = 0

        if rpc.result(moisture):

            return int(moisture)

    except Exception as error:

        print(
            "Moisture read error:",
            error
        )

    return 0


# ------------------------------------------------------------
# Main loop
# ------------------------------------------------------------

def loop():

    global last_weather_update

    mode = get_demo_mode()

    # --------------------------------------------------------
    # LIVE MODE
    # --------------------------------------------------------

    if mode == 0:

        if (
            time.time() -
            last_weather_update
            >= UPDATE_SECONDS
        ):

            print(
                "Updating live weather..."
            )

            if fetch_weather():

                send_weather()

            last_weather_update = (
                time.time()
            )


    # --------------------------------------------------------
    # DEMO MODE
    # --------------------------------------------------------

    elif mode == 1:

        print(
            "DEMO MODE: SUNNY"
        )

    elif mode == 2:

        print(
            "DEMO MODE: RAIN"
        )

    elif mode == 3:

        print(
            "DEMO MODE: HEAT"
        )


    # --------------------------------------------------------
    # Display soil status in console
    # --------------------------------------------------------

    moisture = get_moisture()

    print(
        "Soil moisture:",
        moisture,
        "%"
    )

    time.sleep(5)


# ------------------------------------------------------------
# Start
# ------------------------------------------------------------

print(
    "================================"
)

print(
    "      TERRACAST AI"
)

print(
    "      WEATHER SERVICE"
)

print(
    "================================"
)

print(
    "Location:",
    CITY
)

# Get first weather update immediately.
fetch_weather()
send_weather()

App.run(
    user_loop=loop
)
