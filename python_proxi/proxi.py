from flask import Flask, request, jsonify, Response
import requests
import time

app = Flask(__name__)

# =========================
# API CONFIG
# =========================

NEWS_API = "https://bbc-news-api.vercel.app/news"

OPENWEATHER_API = "https://api.openweathermap.org/data/2.5/weather"

OPENWEATHER_API_KEY = "7c2f370f13a8b83ba4a67cd6f04428d5"

IPGEOLOCATION_API = "https://api.ipgeolocation.io/ipgeo"

IPGEOLOCATION_API_KEY = "d481d36b33e249d4abe04a1c77428884"


# =========================
# GET REAL CLIENT IP
# =========================

def get_client_ip():

    forwarded = request.headers.get("X-Forwarded-For")

    if forwarded:
        return forwarded.split(",")[0].strip()

    return request.remote_addr


# =========================
# GET LOCATION FROM IP
# =========================

def get_location_data(ip):

    # Local testing fallback
    if ip in ["127.0.0.1", "::1"]:
        ip = ""

    response = requests.get(
        IPGEOLOCATION_API,
        params={
            "apiKey": IPGEOLOCATION_API_KEY,
            "ip": ip
        },
        timeout=10
    )

    return response.json()


# =========================
# HOME
# =========================

@app.route("/")
def home():

    return jsonify({
        "status": "running",
        "routes": [
            "/news",
            "/weather",
            "/info"
        ]
    })


# =========================
# NEWS PROXY
# =========================

@app.route("/news")
def news():

    try:

        params = request.args.to_dict()

        if "lang" not in params:
            params["lang"] = "english"

        response = requests.get(
            NEWS_API,
            params=params,
            timeout=10
        )

        return Response(
            response.content,
            status=response.status_code,
            content_type=response.headers.get(
                "Content-Type",
                "application/json"
            )
        )

    except Exception as e:

        return jsonify({
            "error": str(e)
        }), 500


# =========================
# WEATHER
# =========================

@app.route("/weather")
def weather():

    try:

        client_ip = get_client_ip()

        # Get real location from IP
        geo_data = get_location_data(client_ip)

        lat = geo_data.get("latitude")
        lon = geo_data.get("longitude")

        if not lat or not lon:

            return jsonify({
                "error": "Could not determine location",
                "geo_data": geo_data
            }), 400

        # Get weather
        weather_response = requests.get(
            OPENWEATHER_API,
            params={
                "lat": lat,
                "lon": lon,
                "appid": OPENWEATHER_API_KEY,
                "units": "metric"
            },
            timeout=10
        )

        return jsonify(weather_response.json())

    except Exception as e:

        return jsonify({
            "error": str(e)
        }), 500


# =========================
# INFO ENDPOINT
# =========================

@app.route("/info")
def info():

    try:

        client_ip = get_client_ip()

        # Get location info
        geo_data = get_location_data(client_ip)

        lat = geo_data.get("latitude")
        lon = geo_data.get("longitude")

        city = geo_data.get("city")
        country = geo_data.get("country_name")

        # Get weather
        weather_response = requests.get(
            OPENWEATHER_API,
            params={
                "lat": lat,
                "lon": lon,
                "appid": OPENWEATHER_API_KEY,
                "units": "metric"
            },
            timeout=10
        )

        weather_data = weather_response.json()

        weather_type = weather_data["weather"][0]["main"]

        temp = weather_data["main"]["temp"]

        # Final clean response
        return jsonify({

            "unix_epoch": int(time.time()),

            "ip": client_ip,

            "city": city,

            "country": country,

            "latitude": lat,

            "longitude": lon,

            "weather": weather_type,

            "temperature_celsius": temp
        })

    except Exception as e:

        return jsonify({
            "error": str(e)
        }), 500


# =========================
# START SERVER
# =========================

if __name__ == "__main__":

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True
    )